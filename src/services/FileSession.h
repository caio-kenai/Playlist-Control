#pragma once

#include "storage/SafeWriter.h"

namespace pc
{

// A file opened for editing: the bytes and snapshot taken when it was read,
// so a later save can tell whether another program changed it meanwhile.
class FileSession
{
public:
    FileSession() = default;
    explicit FileSession (juce::File file) : file_ (std::move (file)) {}

    const juce::File& file() const noexcept { return file_; }
    const juce::MemoryBlock& bytes() const noexcept { return bytes_; }
    const FileSnapshot& snapshot() const noexcept { return snapshot_; }
    bool loaded() const noexcept { return loaded_; }

    bool load (juce::String& error);

    // True when the file on disk no longer matches what was loaded.
    bool changedOnDisk() const;

    WriteResult save (SafeWriter& writer, const juce::MemoryBlock& content, const juce::String& operation,
                      const juce::String& summary, std::function<juce::String (const juce::MemoryBlock&)> verify = {});

private:
    juce::File file_;
    juce::MemoryBlock bytes_;
    FileSnapshot snapshot_;
    bool loaded_ = false;
};

} // namespace pc
