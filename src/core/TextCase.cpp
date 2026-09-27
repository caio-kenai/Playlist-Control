#include "core/TextCase.h"

namespace pc
{

juce::juce_wchar toUpperLatin (juce::juce_wchar c) noexcept
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7)
        return c - 32;
    switch (c)
    {
        case 0xFF:  return 0x178; // ÿ
        case 0x161: return 0x160; // š
        case 0x153: return 0x152; // œ
        case 0x17E: return 0x17D; // ž
        default:    return c;
    }
}

juce::String toUpperLatin (const juce::String& text)
{
    juce::String out;
    out.preallocateBytes (text.getNumBytesAsUTF8() + 8);
    for (auto p = text.getCharPointer(); ! p.isEmpty();)
        out += toUpperLatin (p.getAndAdvance());
    return out;
}

} // namespace pc
