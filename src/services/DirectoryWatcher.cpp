#include "services/DirectoryWatcher.h"
#include "platform/WinInclude.h"

namespace pc
{

DirectoryWatcher::DirectoryWatcher() : juce::Thread ("PlaylistControl directory watcher")
{
    stopEvent_ = CreateEventW (nullptr, TRUE, FALSE, nullptr);
}

DirectoryWatcher::~DirectoryWatcher()
{
    stop();
    CloseHandle (static_cast<HANDLE> (stopEvent_));
}

bool DirectoryWatcher::isRelevant (const juce::String& rel)
{
    auto r = rel.replaceCharacter ('/', '\\').toLowerCase();
    if (r.contains (".pctmp-"))
        return false;
    auto top = r.upToFirstOccurrenceOf ("\\", false, false);
    if (! r.containsChar ('\\'))
        return r == "playlist.ini" || r == "config.xml" || r == "folders.xml" || r == "remote.ini";
    if (top == "mapas" || top == "grades" || top == "indices" || top == "modelos")
        return ! r.contains ("\\pkinfo\\");
    if (top == "dados")
        return r.endsWith ("ligacao.dbf");
    if (top == "montagem")
        return r.endsWith (".merge");
    if (top == "operadores")
        return r.endsWith ("config.xml");
    return false;
}

void DirectoryWatcher::start (const juce::File& folder, Callback callback)
{
    stop();
    folder_ = folder;
    callback_ = std::move (callback);
    ResetEvent (static_cast<HANDLE> (stopEvent_));
    startThread (juce::Thread::Priority::low);
    startTimer (700);
}

void DirectoryWatcher::stop()
{
    stopTimer();
    if (isThreadRunning())
    {
        SetEvent (static_cast<HANDLE> (stopEvent_));
        stopThread (3000);
    }
    const juce::ScopedLock sl (lock_);
    pending_.clear();
}

void DirectoryWatcher::run()
{
    HANDLE dir = CreateFileW (folder_.getFullPathName().toWideCharPointer(), FILE_LIST_DIRECTORY,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (dir == INVALID_HANDLE_VALUE)
        return;

    OVERLAPPED ov {};
    ov.hEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
    alignas (DWORD) BYTE buffer[64 * 1024];
    HANDLE waits[2] = { ov.hEvent, static_cast<HANDLE> (stopEvent_) };

    while (! threadShouldExit())
    {
        ResetEvent (ov.hEvent);
        DWORD bytes = 0;
        if (! ReadDirectoryChangesW (dir, buffer, sizeof (buffer), TRUE,
                                     FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                                     nullptr, &ov, nullptr))
            break;

        auto w = WaitForMultipleObjects (2, waits, FALSE, INFINITE);
        if (w != WAIT_OBJECT_0)
        {
            CancelIoEx (dir, &ov);
            break;
        }
        if (! GetOverlappedResult (dir, &ov, &bytes, FALSE) || bytes == 0)
            continue; // buffer overflow: nothing specific to report

        std::vector<Change> found;
        for (auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*> (buffer);;)
        {
            juce::String rel (info->FileName, (size_t) info->FileNameLength / sizeof (WCHAR));
            if (isRelevant (rel))
            {
                juce::String action;
                switch (info->Action)
                {
                    case FILE_ACTION_ADDED:            action = "criado"; break;
                    case FILE_ACTION_REMOVED:          action = "removido"; break;
                    case FILE_ACTION_RENAMED_OLD_NAME: action = "renomeado"; break;
                    case FILE_ACTION_RENAMED_NEW_NAME: action = "criado"; break;
                    default:                           action = "alterado"; break;
                }
                found.push_back ({ folder_.getChildFile (rel), action, juce::Time::getCurrentTime() });
            }
            if (info->NextEntryOffset == 0)
                break;
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*> (reinterpret_cast<BYTE*> (info) + info->NextEntryOffset);
        }
        if (! found.empty())
        {
            const juce::ScopedLock sl (lock_);
            pending_.insert (pending_.end(), found.begin(), found.end());
        }
    }
    CloseHandle (ov.hEvent);
    CloseHandle (dir);
}

void DirectoryWatcher::timerCallback()
{
    std::vector<Change> batch;
    {
        const juce::ScopedLock sl (lock_);
        if (pending_.empty())
            return;
        // Wait until the burst is over.
        if ((juce::Time::getCurrentTime() - pending_.back().time).inMilliseconds() < 600)
            return;
        batch.swap (pending_);
    }
    // One entry per file, keeping the last action.
    std::vector<Change> merged;
    for (auto& c : batch)
    {
        auto it = std::find_if (merged.begin(), merged.end(), [&] (const Change& m) { return m.file == c.file; });
        if (it == merged.end())
            merged.push_back (c);
        else
            *it = c;
    }
    if (callback_)
        callback_ (merged);
}

} // namespace pc
