#include "storage/FileIO.h"
#include "platform/WinInclude.h"

#include <juce_cryptography/juce_cryptography.h>

namespace pc
{

juce::String describeWin32Error (unsigned long code)
{
    wchar_t* buffer = nullptr;
    auto n = FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, 0, reinterpret_cast<LPWSTR> (&buffer), 0, nullptr);
    juce::String text;
    if (n > 0 && buffer != nullptr)
        text = juce::String (buffer).trim();
    if (buffer != nullptr)
        LocalFree (buffer);
    return text.isNotEmpty() ? text + " (" + juce::String ((int) code) + ")"
                             : "Erro do Windows " + juce::String ((int) code);
}

bool readFileShared (const juce::File& file, juce::MemoryBlock& out, juce::String& error)
{
    out.reset();
    HANDLE h = CreateFileW (file.getFullPathName().toWideCharPointer(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        error = describeWin32Error (GetLastError());
        return false;
    }

    LARGE_INTEGER size {};
    bool ok = GetFileSizeEx (h, &size) != 0;
    if (ok && size.QuadPart > (LONGLONG) 512 * 1024 * 1024)
    {
        error = L"Arquivo grande demais para ser lido como configuração.";
        ok = false;
    }
    if (ok)
    {
        out.setSize ((size_t) size.QuadPart);
        size_t done = 0;
        while (ok && done < out.getSize())
        {
            DWORD chunk = (DWORD) juce::jmin<size_t> (out.getSize() - done, 1 << 20);
            DWORD read = 0;
            ok = ReadFile (h, static_cast<char*> (out.getData()) + done, chunk, &read, nullptr) != 0;
            if (ok && read == 0)
            {
                // The file shrank while reading.
                out.setSize (done);
                break;
            }
            done += read;
        }
        if (! ok)
            error = describeWin32Error (GetLastError());
    }
    CloseHandle (h);
    return ok;
}

juce::String sha256Hex (const void* data, size_t size)
{
    return juce::SHA256 (data, size).toHexString();
}

juce::String sha256Hex (const juce::MemoryBlock& block)
{
    return sha256Hex (block.getData(), block.getSize());
}

FileSnapshot FileSnapshot::take (const juce::File& file)
{
    FileSnapshot s;
    if (! file.existsAsFile())
        return s;
    juce::MemoryBlock bytes;
    juce::String error;
    if (! readFileShared (file, bytes, error))
    {
        // Exists but unreadable: keep what the file system says so a later
        // comparison still notices changes.
        s.exists = true;
        s.size = file.getSize();
        s.modified = file.getLastModificationTime();
        s.sha256 = "unreadable:" + juce::String (s.modified.toMilliseconds()) + ":" + juce::String (s.size);
        return s;
    }
    return fromBytes (file, bytes);
}

FileSnapshot FileSnapshot::fromBytes (const juce::File& file, const juce::MemoryBlock& bytes)
{
    FileSnapshot s;
    s.exists = true;
    s.size = (juce::int64) bytes.getSize();
    s.modified = file.getLastModificationTime();
    s.sha256 = sha256Hex (bytes);
    return s;
}

} // namespace pc
