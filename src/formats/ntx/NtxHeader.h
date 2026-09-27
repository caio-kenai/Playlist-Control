#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

// Header of a Clipper NTX index (Indices\*.NTX). Read only.
struct NtxHeader
{
    bool valid = false;
    juce::String problem;

    int signature = 0;     // 6 for NTX
    int version = 0;       // incremented by the owner on updates
    juce::uint32 rootPage = 0;
    juce::uint32 nextFreePage = 0;
    int itemSize = 0;
    int keySize = 0;
    int keyDecimals = 0;
    int maxItems = 0;
    int halfPage = 0;
    juce::String keyExpression;
    bool unique = false;
    juce::int64 fileSize = 0;
    int pages = 0;         // 1024-byte pages after the header

    static NtxHeader parse (const juce::MemoryBlock& bytes);
};

} // namespace pc
