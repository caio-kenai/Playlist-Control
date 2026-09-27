#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

// One saved change: the file, when, which operation, and the complete
// before/after contents so the operator can compare and restore.
struct HistoryEntry
{
    juce::String id;
    juce::Time time;
    juce::File target;
    juce::String operation; // short verb, e.g. "Editar mapa"
    juce::String summary;   // what changed, for the operator
    juce::String user;
    juce::String machine;
    bool hadBefore = false;
    juce::int64 beforeSize = 0, afterSize = 0;
    juce::String beforeSha256, afterSha256;
    juce::String restoredFrom; // id of the entry restored, if any
    juce::File folder;

    juce::File beforeFile() const { return folder.getChildFile ("before.bin"); }
    juce::File afterFile() const { return folder.getChildFile ("after.bin"); }
};

class HistoryStore
{
public:
    explicit HistoryStore (juce::File root);

    const juce::File& root() const noexcept { return root_; }

    // Copies the contents to a new entry folder marked as pending. Returns an
    // error message on failure.
    juce::String begin (HistoryEntry& entry, const juce::MemoryBlock* before, const juce::MemoryBlock& after);
    void commit (HistoryEntry& entry);
    void abandon (HistoryEntry& entry);

    // Completed entries, newest first.
    std::vector<HistoryEntry> list (int maxEntries = 1000) const;
    std::vector<HistoryEntry> listFor (const juce::File& target, int maxEntries = 200) const;

    bool readBefore (const HistoryEntry& entry, juce::MemoryBlock& out) const;
    bool readAfter (const HistoryEntry& entry, juce::MemoryBlock& out) const;

    // Removes completed entries older than the limit; leftovers from
    // interrupted writes are removed too.
    int prune (int keepDays);

private:
    static bool loadEntry (const juce::File& folder, HistoryEntry& entry, juce::String& state);
    static void writeManifest (const HistoryEntry& entry, const juce::String& state);

    juce::File root_;
};

} // namespace pc
