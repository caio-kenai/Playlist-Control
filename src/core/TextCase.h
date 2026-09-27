#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

// Upper case that also converts accented Latin letters (ç, ã, é...).
// juce::String::toUpperCase() leaves them unchanged.
juce::juce_wchar toUpperLatin (juce::juce_wchar c) noexcept;
juce::String toUpperLatin (const juce::String& text);

} // namespace pc
