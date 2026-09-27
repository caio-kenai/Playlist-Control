#pragma once

#include <juce_events/juce_events.h>

namespace pc
{

// Watches the pgm folder with ReadDirectoryChangesW (JUCE has no directory
// watcher on Windows) and reports relevant changes on the message thread,
// coalesced so a program rewriting a file several times produces one event.
class DirectoryWatcher : private juce::Thread, private juce::Timer
{
public:
    struct Change
    {
        juce::File file;
        juce::String action; // "criado", "alterado", "removido", "renomeado"
        juce::Time time;
    };

    using Callback = std::function<void (const std::vector<Change>&)>;

    DirectoryWatcher();
    ~DirectoryWatcher() override;

    void start (const juce::File& folder, Callback callback);
    void stop();

    // Files PlaylistControl itself is writing are not reported.
    static bool isRelevant (const juce::String& relativePath);

private:
    void run() override;
    void timerCallback() override;

    juce::File folder_;
    Callback callback_;
    juce::CriticalSection lock_;
    std::vector<Change> pending_;
    void* stopEvent_ = nullptr;
};

} // namespace pc
