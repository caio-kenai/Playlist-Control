#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

// Reads a file while other programs keep it open (the Playlist holds DBF and
// XML files open). Opens with read/write/delete sharing.
bool readFileShared (const juce::File& file, juce::MemoryBlock& out, juce::String& error);

// SHA-256 as lowercase hex.
juce::String sha256Hex (const void* data, size_t size);
juce::String sha256Hex (const juce::MemoryBlock& block);

struct FileSnapshot
{
    bool exists = false;
    juce::int64 size = 0;
    juce::Time modified;
    juce::String sha256;

    static FileSnapshot take (const juce::File& file);
    static FileSnapshot fromBytes (const juce::File& file, const juce::MemoryBlock& bytes);

    // Content comparison; timestamps alone are not trusted.
    bool sameContent (const FileSnapshot& other) const noexcept
    {
        return exists == other.exists && (! exists || sha256 == other.sha256);
    }
};

// Windows error text for the last error or a given code.
juce::String describeWin32Error (unsigned long code);

} // namespace pc
