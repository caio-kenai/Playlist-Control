#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pc::ui
{

// Icons of icon libraries and .ico files (pgm\Icones), loaded through Windows
// and kept in memory. Empty images when a file or index cannot be read.
class ShellIcons
{
public:
    static ShellIcons& instance();

    juce::Image get (const juce::String& file, int index, int size);
    int count (const juce::String& file);

private:
    std::map<juce::String, juce::Image> cache_;
    std::map<juce::String, int> counts_;
};

} // namespace pc::ui
