#pragma once

#include "formats/ntx/NtxIndex.h"
#include "services/PlaylistProcess.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <map>
#include <mutex>

namespace pc
{

// State of one index of pgm\Indices compared with its table.
struct IndexStatus
{
    juce::File file;
    juce::String table; // "LIGACAO.DBF", "COMPROVE.DBF" or empty when unknown
    bool exists = false;
    NtxVerification verification;
    juce::Time modified;
};

std::vector<IndexStatus> inspectIndexes (const juce::File& pgm);

struct IndexRebuildCheck
{
    std::vector<PlaylistProgram> playlist; // Playlist Digital processes of this pgm
    juce::StringArray blockers;            // reasons that prevent the operation
    juce::StringArray warnings;            // points the operator should know
    juce::File separaComprove;
    juce::File playlistExe;                // executable to start at the end
};

// What must be true before the indexes can be recreated.
IndexRebuildCheck checkIndexRebuild (const juce::File& pgm);

// Recreation of the indexes, as done by the Playlist support: close the
// Playlist Digital, delete pgm\Indices\*.NTX, run SeparaComprove.exe and
// start the Playlist again, which writes the indexes when it opens.
//
// Runs on its own thread. The Playlist is closed through its window (as the
// operator would); it is never terminated. Every file removed is copied first
// and put back if a step fails before the Playlist is reopened.
class IndexRebuild : private juce::Thread
{
public:
    using Step = JobStep;
    using StepState = JobStep::State;

    struct Options
    {
        bool startPlaylistAtEnd = true;
        int closeTimeoutMs = 90000;
        int separaTimeoutMs = 300000;
        int reopenTimeoutMs = 120000;
    };

    IndexRebuild (juce::File pgm, juce::File backupRoot, Options options);
    ~IndexRebuild() override;

    void start();
    bool running() const { return isThreadRunning(); }
    bool finished() const { return finished_; }
    bool succeeded() const { return succeeded_; }
    juce::File backupFolder() const { return backupFolder_; }

    std::vector<Step> steps() const;
    juce::String outcome() const;

    // Called on the message thread whenever a step changes.
    std::function<void()> onChange;

private:
    enum StepId
    {
        stepCheck,
        stepClose,
        stepBackup,
        stepDelete,
        stepSepara,
        stepStart,
        stepVerify,
        stepCount
    };

    void run() override;
    void set (int step, StepState state, const juce::String& detail = {});
    void finish (bool ok, const juce::String& outcome);
    bool restoreIndexes (juce::String& error);
    bool runSeparaComprove (const juce::File& exe);
    bool waitForIndexes();

    juce::File pgm_, backupRoot_, backupFolder_;
    Options options_;
    mutable std::mutex lock_;
    std::vector<Step> steps_;
    juce::String outcome_;
    std::atomic<bool> finished_ { false }, succeeded_ { false };
    juce::StringArray deletedIndexes_;
    bool separaStillRunning_ = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (IndexRebuild)
};

} // namespace pc
