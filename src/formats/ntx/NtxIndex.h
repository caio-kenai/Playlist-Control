#pragma once

#include "formats/dbf/DbfTable.h"
#include "formats/ntx/NtxHeader.h"

#include <string>

namespace pc
{

// Clipper NTX index: 1024-byte header followed by 1024-byte pages. Each page
// holds a count, an offset table with maxItems + 1 entries and the items
// (child page, record number, key). Keys are compared byte by byte.
struct NtxEntry
{
    std::string key;     // raw bytes, padded to the key size
    juce::uint32 record; // 1-based record number of the DBF
    bool operator== (const NtxEntry& o) const { return key == o.key && record == o.record; }
};

struct NtxReadResult
{
    NtxHeader header;
    std::vector<NtxEntry> entries; // in key order
    juce::String problem;          // empty when the tree is consistent
    int pages = 0;
};

NtxReadResult readNtx (const juce::MemoryBlock& bytes);

// Key expressions used by the Playlist: FIELD, UPPER(x), DTOS(x), DESCEND(x)
// joined with '+'. Returns false for anything else.
bool evaluateNtxKey (const juce::String& expression, const DbfTable& table, int record, std::string& key);

// Entries an index should contain for a table, sorted as the index keeps them.
bool expectedNtxEntries (const NtxHeader& header, const DbfTable& table, std::vector<NtxEntry>& out, juce::String& error);

struct NtxVerification
{
    bool readable = false;
    bool consistent = false;
    juce::String summary;
    int indexEntries = 0, tableRecords = 0;
};

NtxVerification verifyNtx (const juce::MemoryBlock& ntx, const DbfTable& table);

// Builds an index the way the Playlist does when it creates one from scratch
// (sorted bulk load). The header fields come from 'templateHeader'.
juce::MemoryBlock buildNtx (const NtxHeader& templateHeader, const std::vector<NtxEntry>& sortedEntries);

// Windows-1252 upper case, as used by UPPER() in the Playlist indexes.
unsigned char upper1252 (unsigned char c) noexcept;

} // namespace pc
