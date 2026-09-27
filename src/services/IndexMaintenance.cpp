#include "services/IndexMaintenance.h"
#include "formats/dbf/DbfTable.h"
#include "install/Installation.h"
#include "logging/Logger.h"
#include "platform/WindowsSystem.h"
#include "storage/FileIO.h"

namespace pc
{

namespace
{
const char* const knownIndexes[] = { "LIGA_COD.NTX", "LIGA_ARQ.NTX", "COMPROVE-C.NTX", "COMPROVE-A.NTX" };

std::optional<DbfTable> loadTable (const juce::File& file)
{
    juce::MemoryBlock bytes;
    juce::String error;
    if (! readFileShared (file, bytes, error))
        return std::nullopt;
    return DbfTable::parse (bytes, error);
}

juce::String tableFor (const juce::String& indexName)
{
    if (indexName.startsWithIgnoreCase ("LIGA"))
        return "LIGACAO.DBF";
    if (indexName.startsWithIgnoreCase ("COMPROVE"))
        return "COMPROVE.DBF";
    return {};
}
} // namespace

std::vector<IndexStatus> inspectIndexes (const juce::File& pgm)
{
    std::vector<IndexStatus> out;
    auto folder = pgm.getChildFile ("Indices");
    auto ligacao = loadTable (pgm.getChildFile ("Dados/LIGACAO.DBF"));
    auto comprove = loadTable (pgm.getChildFile ("Dados/COMPROVE.DBF"));

    juce::StringArray names;
    for (auto* n : knownIndexes)
        names.add (n);
    for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.NTX"))
        if (! names.contains (f.getFileName(), true))
            names.add (f.getFileName());

    for (auto& name : names)
    {
        IndexStatus s;
        s.file = folder.getChildFile (name);
        for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.NTX"))
            if (f.getFileName().equalsIgnoreCase (name))
                s.file = f;
        s.table = tableFor (name);
        s.exists = s.file.existsAsFile();
        if (! s.exists)
        {
            s.verification.summary = L"Arquivo não existe. O Playlist cria o índice quando abre.";
            out.push_back (s);
            continue;
        }
        s.modified = s.file.getLastModificationTime();
        const auto& table = s.table == "LIGACAO.DBF" ? ligacao : comprove;
        juce::MemoryBlock bytes;
        juce::String error;
        if (s.table.isEmpty())
            s.verification.summary = L"Índice não conhecido: não verificado.";
        else if (! table.has_value())
            s.verification.summary = L"Não foi possível ler Dados\\" + s.table + ".";
        else if (! readFileShared (s.file, bytes, error))
            s.verification.summary = error;
        else
            s.verification = verifyNtx (bytes, *table);
        out.push_back (s);
    }
    return out;
}

IndexRebuildCheck checkIndexRebuild (const juce::File& pgm)
{
    IndexRebuildCheck c;
    if (! pgm.getChildFile ("Indices").isDirectory())
        c.blockers.add (L"A pasta Indices não existe nesta instalação.");
    c.separaComprove = pgm.getChildFile ("SeparaComprove.exe");
    if (! c.separaComprove.existsAsFile())
        c.blockers.add (L"SeparaComprove.exe não foi encontrado na pasta pgm.");

    auto programs = checkPlaylistPrograms (pgm);
    c.playlist = programs.playlist;
    c.blockers.addArray (programs.blockers);
    c.warnings.addArray (programs.warnings);
    c.playlistExe = programs.playlistExe;
    return c;
}

IndexRebuild::IndexRebuild (juce::File pgm, juce::File backupRoot, Options options)
    : juce::Thread ("Index rebuild"), pgm_ (std::move (pgm)), backupRoot_ (std::move (backupRoot)), options_ (options)
{
    steps_ = {
        { L"Conferir programas abertos" },
        { L"Fechar o Playlist Digital" },
        { L"Guardar cópia dos índices" },
        { L"Apagar os índices" },
        { L"Executar o SeparaComprove" },
        { L"Abrir o Playlist Digital" },
        { L"Conferir os índices recriados" },
    };
}

IndexRebuild::~IndexRebuild()
{
    signalThreadShouldExit();
    waitForThreadToExit (-1);
    masterReference.clear();
}

void IndexRebuild::start()
{
    startThread();
}

std::vector<IndexRebuild::Step> IndexRebuild::steps() const
{
    std::lock_guard<std::mutex> g (lock_);
    return steps_;
}

juce::String IndexRebuild::outcome() const
{
    std::lock_guard<std::mutex> g (lock_);
    return outcome_;
}

void IndexRebuild::set (int step, StepState state, const juce::String& detail)
{
    {
        std::lock_guard<std::mutex> g (lock_);
        steps_[(size_t) step].state = state;
        steps_[(size_t) step].detail = detail;
    }
    if (state == StepState::failed)
        Logger::instance().warning ("index.rebuild.step", steps_[(size_t) step].title + ": " + detail);
    else if (state != StepState::running)
        Logger::instance().info ("index.rebuild.step", steps_[(size_t) step].title + ": " + detail);
    juce::WeakReference<IndexRebuild> self (this);
    juce::MessageManager::callAsync ([self] {
        if (self != nullptr && self->onChange)
            self->onChange();
    });
}

void IndexRebuild::finish (bool ok, const juce::String& text)
{
    {
        std::lock_guard<std::mutex> g (lock_);
        outcome_ = text;
        for (auto& s : steps_)
            if (s.state == StepState::pending)
                s.state = StepState::skipped;
    }
    succeeded_ = ok;
    finished_ = true;
    Logger::instance().log (ok ? Logger::Level::info : Logger::Level::warning, "index.rebuild.finish", text,
                            { { "backup", backupFolder_.getFullPathName() } });
    juce::WeakReference<IndexRebuild> self (this);
    juce::MessageManager::callAsync ([self] {
        if (self != nullptr && self->onChange)
            self->onChange();
    });
}

bool IndexRebuild::restoreIndexes (juce::String& error)
{
    bool ok = true;
    for (auto& name : deletedIndexes_)
    {
        auto from = backupFolder_.getChildFile ("Indices").getChildFile (name);
        auto to = pgm_.getChildFile ("Indices").getChildFile (name);
        if (! from.copyFileTo (to))
        {
            ok = false;
            error << name << " ";
        }
    }
    if (ok)
        deletedIndexes_.clear();
    return ok;
}

bool IndexRebuild::runSeparaComprove (const juce::File& exe)
{
    juce::String error;
    auto pid = launchProcess (exe, pgm_, error);
    if (pid == 0)
    {
        set (stepSepara, StepState::failed, L"Não foi possível iniciar o SeparaComprove: " + error);
        return false;
    }
    // The program opens a dialog ("SeparaComprove") whose OK button (id 1)
    // starts the work with the default voucher file.
    juce::pointer_sized_int dialog = 0;
    for (int i = 0; i < 80 && dialog == 0 && isProcessRunning (pid); ++i)
    {
        for (auto& w : windowsOfProcess (pid))
            if (w.isDialog() && dialogItemText (w.handle, 1).isNotEmpty())
                dialog = w.handle;
        if (dialog == 0)
            juce::Thread::sleep (250);
    }
    if (dialog == 0)
    {
        if (! isProcessRunning (pid))
        {
            set (stepSepara, StepState::failed, L"O SeparaComprove fechou sem mostrar a janela esperada.");
            return false;
        }
        set (stepSepara, StepState::running, L"Janela do SeparaComprove não reconhecida: clique em OK nela para continuar.");
    }
    else if (! clickDialogButton (dialog, 1))
        set (stepSepara, StepState::running, L"Não foi possível acionar o OK: clique em OK na janela do SeparaComprove.");
    else
        set (stepSepara, StepState::running, L"Separando comprovações. Se aparecer alguma mensagem, responda na janela do SeparaComprove.");

    if (! waitForProcessExit (pid, options_.separaTimeoutMs))
    {
        separaStillRunning_ = true;
        set (stepSepara, StepState::failed,
             L"O SeparaComprove não terminou em " + juce::String (options_.separaTimeoutMs / 60000)
                 + L" min. Conclua-o e abra o Playlist Digital manualmente.");
        return false;
    }
    set (stepSepara, StepState::done, L"Concluído.");
    return true;
}

bool IndexRebuild::waitForIndexes()
{
    auto folder = pgm_.getChildFile ("Indices");
    auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) options_.reopenTimeoutMs;
    std::map<juce::String, juce::int64> lastSizes;
    int stableRounds = 0;
    for (;;)
    {
        bool all = true, changed = false;
        for (auto& name : deletedIndexes_)
        {
            auto f = folder.getChildFile (name);
            auto size = f.existsAsFile() ? f.getSize() : -1;
            all = all && size > 0;
            changed = changed || lastSizes[name] != size;
            lastSizes[name] = size;
        }
        stableRounds = all && ! changed ? stableRounds + 1 : 0;
        if (stableRounds >= 4)
            return true;
        if (juce::Time::getMillisecondCounter() > deadline)
            return false;
        juce::Thread::sleep (500);
    }
}

void IndexRebuild::run()
{
    auto& log = Logger::instance();
    log.info ("index.rebuild.start", L"Recriação dos índices iniciada", { { "pgm", pgm_.getFullPathName() } });

    // 1. Programs that use the data files.
    set (stepCheck, StepState::running);
    auto check = checkIndexRebuild (pgm_);
    if (! check.blockers.isEmpty())
    {
        set (stepCheck, StepState::failed, check.blockers.joinIntoString (" "));
        finish (false, L"Nada foi alterado.");
        return;
    }
    auto playlistExe = check.playlistExe;
    bool wasRunning = ! check.playlist.empty();
    set (stepCheck, StepState::done,
         wasRunning ? L"Playlist Digital aberto: " + check.playlist.front().name : juce::String (L"Playlist Digital fechado."));

    // 2. Close the Playlist through its window.
    if (wasRunning)
    {
        set (stepClose, StepState::running, L"Pedindo ao Playlist que feche…");
        juce::String error;
        if (! pc::closePlaylist (check.playlist, options_.closeTimeoutMs,
                                 [this] (const juce::String& text) { set (stepClose, StepState::running, text); },
                                 [this] { return threadShouldExit(); }, error))
        {
            set (stepClose, StepState::failed, error);
            finish (false, L"Nada foi alterado.");
            return;
        }
        set (stepClose, StepState::done, L"Playlist fechado.");
    }
    else
        set (stepClose, StepState::skipped, L"Já estava fechado.");

    auto reopen = [&] {
        if (! (wasRunning || options_.startPlaylistAtEnd) || playlistExe == juce::File())
        {
            set (stepStart, StepState::skipped, L"O Playlist Digital não será aberto agora.");
            return false;
        }
        set (stepStart, StepState::running, L"Abrindo " + playlistExe.getFileName() + L"…");
        juce::String detail;
        auto ok = pc::startPlaylist (playlistExe, pgm_, detail);
        set (stepStart, ok ? StepState::done : StepState::failed, detail);
        return ok;
    };

    // 3. Copy of what will be removed or rewritten.
    set (stepBackup, StepState::running);
    backupFolder_ = backupRoot_.getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S"));
    auto indices = pgm_.getChildFile ("Indices");
    auto ntx = indices.findChildFiles (juce::File::findFiles, false, "*.NTX");
    bool backupOk = backupFolder_.getChildFile ("Indices").createDirectory() && backupFolder_.getChildFile ("Dados").createDirectory();
    for (auto& f : ntx)
        backupOk = backupOk && f.copyFileTo (backupFolder_.getChildFile ("Indices").getChildFile (f.getFileName()))
                   && backupFolder_.getChildFile ("Indices").getChildFile (f.getFileName()).getSize() == f.getSize();
    for (auto* name : { "COMPROVE.DBF", "LIGACAO.DBF" })
    {
        auto f = pgm_.getChildFile ("Dados").getChildFile (name);
        if (f.existsAsFile())
            backupOk = backupOk && f.copyFileTo (backupFolder_.getChildFile ("Dados").getChildFile (name));
    }
    if (! backupOk)
    {
        set (stepBackup, StepState::failed, L"Não foi possível copiar os arquivos para " + backupFolder_.getFullPathName() + ".");
        reopen();
        finish (false, L"Nada foi alterado.");
        return;
    }
    backupFolder_.getChildFile ("LEIA-ME.txt")
        .replaceWithText (L"Cópia feita pelo Playlist Control antes de recriar os índices.\r\nInstalação: " + pgm_.getFullPathName()
                          + "\r\nData: " + juce::Time::getCurrentTime().toString (true, true, true, true) + "\r\n");
    set (stepBackup, StepState::done, juce::String ((int) ntx.size()) + L" índice(s) e tabelas copiados para " + backupFolder_.getFullPathName());

    // 4. Delete the indexes.
    set (stepDelete, StepState::running);
    for (auto& f : ntx)
    {
        if (! f.deleteFile())
        {
            juce::String restoreError;
            auto restored = restoreIndexes (restoreError);
            set (stepDelete, StepState::failed,
                 L"Não foi possível apagar " + f.getFileName() + L" (arquivo em uso?). "
                     + (restored ? juce::String (L"Os índices já apagados foram devolvidos.")
                                 : L"Falha ao devolver: " + restoreError + L"— copie de " + backupFolder_.getFullPathName()));
            reopen();
            finish (false, restored ? juce::String (L"Nada foi alterado.") : juce::String (L"Índices incompletos: veja a cópia de segurança."));
            return;
        }
        deletedIndexes_.add (f.getFileName());
    }
    set (stepDelete, StepState::done, juce::String (deletedIndexes_.size()) + L" arquivo(s) apagado(s).");

    // 5. SeparaComprove.
    set (stepSepara, StepState::running, L"Iniciando…");
    auto separaOk = runSeparaComprove (check.separaComprove);
    if (separaStillRunning_)
    {
        set (stepStart, StepState::skipped, L"O Playlist Digital não foi aberto porque o SeparaComprove ainda está em execução.");
        finish (false, L"Conclua o SeparaComprove e abra o Playlist Digital manualmente.");
        return;
    }

    // 6. Start the Playlist, which writes the indexes when it opens.
    bool started = reopen();

    // 7. Verify.
    if (! started)
    {
        set (stepVerify, StepState::skipped, L"Os índices serão criados quando o Playlist Digital for aberto.");
        finish (separaOk, separaOk ? juce::String (L"Índices apagados; abra o Playlist Digital para que ele os crie.")
                                   : juce::String (L"Operação incompleta: veja as etapas acima."));
        return;
    }
    set (stepVerify, StepState::running, L"Aguardando o Playlist gravar os índices…");
    if (! waitForIndexes())
    {
        set (stepVerify, StepState::failed,
             L"O Playlist não gravou todos os índices em " + juce::String (options_.reopenTimeoutMs / 1000)
                 + L" s. Verifique na tela Índices; a cópia está em " + backupFolder_.getFullPathName());
        finish (false, L"Operação concluída com pendências.");
        return;
    }
    juce::StringArray problems;
    for (auto& s : inspectIndexes (pgm_))
        if (deletedIndexes_.contains (s.file.getFileName(), true) && ! s.verification.consistent)
            problems.add (s.file.getFileName() + ": " + s.verification.summary);
    if (problems.isEmpty())
    {
        set (stepVerify, StepState::done, juce::String (deletedIndexes_.size()) + L" índice(s) recriados e conferidos com as tabelas.");
        finish (separaOk, separaOk ? juce::String (L"Índices recriados com sucesso.")
                                   : juce::String (L"Índices recriados, mas o SeparaComprove não concluiu."));
    }
    else
    {
        set (stepVerify, StepState::failed, problems.joinIntoString (" "));
        finish (false, L"Índices recriados com divergências.");
    }
}

} // namespace pc
