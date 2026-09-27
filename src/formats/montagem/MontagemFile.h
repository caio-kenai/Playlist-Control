#pragma once

#include "core/TextCodec.h"
#include "core/TimeOfDay.h"

namespace pc
{

// Line of Montagem\dd-mm-aaaa.TXT and of *.merge files:
//   HH:MM T, posição, "Pasta", "Arquivo"
struct MontagemEntry
{
    int line = 0;
    TimeOfDay block;
    juce::juce_wchar type = 0; // 'M' musical, 'C' commercial
    int position = 0;
    juce::String folder;
    juce::String file;
};

struct MontagemFile
{
    TextEncoding encoding = TextEncoding::ascii;
    std::vector<MontagemEntry> entries;
    std::vector<std::pair<int, juce::String>> invalidLines;

    static MontagemFile parse (const juce::MemoryBlock& bytes);
};

} // namespace pc
