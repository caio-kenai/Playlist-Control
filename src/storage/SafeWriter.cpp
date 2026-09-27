#include "storage/SafeWriter.h"
#include "logging/Logger.h"
#include "platform/WinInclude.h"

namespace pc
{

namespace
{
bool isTransient (DWORD code)
{
    return code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION || code == ERROR_ACCESS_DENIED
        || code == ERROR_UNABLE_TO_REMOVE_REPLACED || code == ERROR_USER_MAPPED_FILE;
}

juce::String writeTemp (const juce::File& temp, const juce::MemoryBlock& content)
{
    HANDLE h = CreateFileW (temp.getFullPathName().toWideCharPointer(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return L"Não foi possível criar o arquivo temporário: " + describeWin32Error (GetLastError());

    juce::String error;
    size_t done = 0;
    while (done < content.getSize() && error.isEmpty())
    {
        DWORD chunk = (DWORD) juce::jmin<size_t> (content.getSize() - done, 1 << 20);
        DWORD written = 0;
        if (! WriteFile (h, static_cast<const char*> (content.getData()) + done, chunk, &written, nullptr) || written == 0)
            error = L"Falha ao gravar o arquivo temporário: " + describeWin32Error (GetLastError());
        done += written;
    }
    if (error.isEmpty() && ! FlushFileBuffers (h))
        error = L"Falha ao descarregar o arquivo temporário no disco: " + describeWin32Error (GetLastError());
    CloseHandle (h);
    return error;
}

// Replaces 'target' with 'temp' atomically, retrying while another program
// holds the target without delete sharing.
juce::String replaceWithRetry (const juce::File& target, const juce::File& temp, bool targetExists, int retryMs)
{
    auto start = juce::Time::getMillisecondCounter();
    DWORD last = 0;
    for (;;)
    {
        BOOL ok;
        if (targetExists)
        {
            ok = ReplaceFileW (target.getFullPathName().toWideCharPointer(), temp.getFullPathName().toWideCharPointer(),
                               nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, nullptr, nullptr);
            if (! ok)
            {
                last = GetLastError();
                // Some file systems (network shares) do not support ReplaceFile.
                if (last == ERROR_INVALID_PARAMETER || last == ERROR_NOT_SUPPORTED || last == ERROR_INVALID_FUNCTION)
                    ok = MoveFileExW (temp.getFullPathName().toWideCharPointer(), target.getFullPathName().toWideCharPointer(),
                                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            }
        }
        else
        {
            ok = MoveFileExW (temp.getFullPathName().toWideCharPointer(), target.getFullPathName().toWideCharPointer(),
                              MOVEFILE_WRITE_THROUGH);
        }
        if (ok)
            return {};
        last = GetLastError();
        if (! isTransient (last) || (int) (juce::Time::getMillisecondCounter() - start) > retryMs)
            break;
        juce::Thread::sleep (100);
    }
    if (last == ERROR_SHARING_VIOLATION || last == ERROR_LOCK_VIOLATION || last == ERROR_UNABLE_TO_REMOVE_REPLACED)
        return L"O arquivo está em uso por outro programa e não pôde ser substituído agora. "
               "Tente novamente em alguns segundos. (" + describeWin32Error (last) + ")";
    return L"Não foi possível substituir o arquivo: " + describeWin32Error (last);
}
} // namespace

SafeWriter::SafeWriter (HistoryStore& history) : history_ (history) {}

WriteResult SafeWriter::write (const WriteRequest& req)
{
    WriteResult r;
    auto& log = Logger::instance();
    juce::NamedValueSet f;
    f.set ("file", req.target.getFullPathName());
    f.set ("operation", req.operation);

    if (readOnly_)
    {
        r.status = WriteStatus::readOnly;
        r.message = L"O PlaylistControl está em modo somente leitura. Nenhum arquivo foi alterado.";
        return r;
    }

    // 1. Current state of the disk.
    juce::MemoryBlock current;
    bool exists = req.target.existsAsFile();
    if (exists)
    {
        juce::String err;
        if (! readFileShared (req.target, current, err))
        {
            r.status = WriteStatus::ioError;
            r.message = L"Não foi possível ler o arquivo atual antes de gravar: " + err;
            log.error ("write.read_failed", r.message, f);
            return r;
        }
    }
    auto currentSnap = exists ? FileSnapshot::fromBytes (req.target, current) : FileSnapshot {};

    // 2. Conflict with an external change.
    if (req.expectedBase.has_value() && ! req.expectedBase->sameContent (currentSnap))
    {
        r.status = WriteStatus::conflict;
        r.message = currentSnap.exists
                        ? "O arquivo foi alterado por outro programa depois que foi aberto aqui. "
                          L"Recarregue o arquivo e refaça a alteração, ou compare as versões antes de decidir."
                        : L"O arquivo foi removido por outro programa depois que foi aberto aqui.";
        log.warning ("write.conflict", r.message, f);
        return r;
    }

    if (exists && current == req.content)
    {
        r.status = WriteStatus::unchanged;
        r.after = currentSnap;
        return r;
    }

    // 3. Verification of the exact bytes.
    if (req.verify)
    {
        auto problem = req.verify (req.content);
        if (problem.isNotEmpty())
        {
            r.status = WriteStatus::verifyFailed;
            r.message = problem;
            log.warning ("write.verify_failed", problem, f);
            return r;
        }
    }

    // 4. Backup and history.
    r.entry.target = req.target;
    r.entry.operation = req.operation;
    r.entry.summary = req.summary;
    r.entry.restoredFrom = req.restoredFrom;
    auto historyError = history_.begin (r.entry, exists ? &current : nullptr, req.content);
    if (historyError.isNotEmpty())
    {
        history_.abandon (r.entry);
        r.status = WriteStatus::ioError;
        r.message = historyError + " Nada foi alterado.";
        log.error ("write.backup_failed", r.message, f);
        return r;
    }

    // 5. Temporary file next to the target (same volume, so the rename is atomic).
    auto temp = req.target.getSiblingFile ("." + req.target.getFileName() + ".pctmp-"
                                           + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
    auto fail = [&] (juce::String message) {
        temp.deleteFile();
        history_.abandon (r.entry);
        r.status = WriteStatus::ioError;
        r.message = std::move (message);
        log.error ("write.failed", r.message, f);
        return r;
    };

    auto err = writeTemp (temp, req.content);
    if (err.isNotEmpty())
        return fail (err + " Nada foi alterado.");

    juce::MemoryBlock check;
    if (! readFileShared (temp, check, err) || check != req.content)
        return fail (L"O arquivo temporário não confere com o conteúdo esperado. Nada foi alterado.");

    // 6. Atomic replacement.
    err = replaceWithRetry (req.target, temp, exists, retryMs_);
    if (err.isNotEmpty())
        return fail (err + L" O arquivo original não foi alterado.");

    // 7. Confirm what is on disk now.
    juce::MemoryBlock written;
    if (! readFileShared (req.target, written, err) || written != req.content)
    {
        // Something else wrote in between; the backup stays available.
        history_.commit (r.entry);
        r.status = WriteStatus::ioError;
        r.message = L"O arquivo foi gravado, mas o conteúdo lido em seguida é diferente. "
                    L"Outro programa pode ter alterado o arquivo ao mesmo tempo. A versão anterior está no Histórico.";
        log.error ("write.post_check_failed", r.message, f);
        return r;
    }

    history_.commit (r.entry);
    r.after = FileSnapshot::fromBytes (req.target, written);
    r.status = WriteStatus::written;
    r.message = L"Alteração gravada com cópia de segurança.";
    f.set ("history", r.entry.id);
    f.set ("beforeSha256", r.entry.beforeSha256);
    f.set ("afterSha256", r.entry.afterSha256);
    log.info ("write.done", req.summary.isNotEmpty() ? req.summary : req.operation, f);
    return r;
}

} // namespace pc
