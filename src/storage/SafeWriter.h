#pragma once

#include "storage/FileIO.h"
#include "storage/HistoryStore.h"

#include <functional>
#include <optional>

namespace pc
{

struct WriteRequest
{
    juce::File target;
    juce::MemoryBlock content;
    juce::String operation;
    juce::String summary;

    // State of the file when the operator opened it. When it no longer
    // matches the disk, the write is refused as a conflict.
    std::optional<FileSnapshot> expectedBase;

    // Re-checks the exact bytes that will be written (for example by parsing
    // them again). Returns an empty string when valid.
    std::function<juce::String (const juce::MemoryBlock&)> verify;

    juce::String restoredFrom;
};

enum class WriteStatus
{
    written,
    unchanged,
    readOnly,
    conflict,
    verifyFailed,
    ioError
};

struct WriteResult
{
    WriteStatus status = WriteStatus::ioError;
    juce::String message;
    HistoryEntry entry;
    FileSnapshot after;

    bool succeeded() const noexcept { return status == WriteStatus::written || status == WriteStatus::unchanged; }
};

// The only path that writes into the Playlist installation:
// conflict check -> verification -> backup -> temporary file in the same
// folder -> flush -> read back -> atomic replace -> read back -> history.
class SafeWriter
{
public:
    explicit SafeWriter (HistoryStore& history);

    void setReadOnly (bool shouldBeReadOnly) noexcept { readOnly_ = shouldBeReadOnly; }
    bool isReadOnly() const noexcept { return readOnly_; }

    // How long to keep retrying while another program holds the file.
    void setRetryWindow (int milliseconds) noexcept { retryMs_ = milliseconds; }

    WriteResult write (const WriteRequest& request);

private:
    HistoryStore& history_;
    bool readOnly_ = false;
    int retryMs_ = 3000;
};

} // namespace pc
