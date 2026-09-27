#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

struct DiffLine
{
    enum class Kind
    {
        same,
        removed,
        added
    };

    Kind kind = Kind::same;
    juce::String text;
    int oldLine = 0; // 1-based, 0 when not present
    int newLine = 0;
};

// Line diff (longest common subsequence after trimming the common prefix and
// suffix, which keeps typical single-block edits cheap).
std::vector<DiffLine> diffLines (const juce::StringArray& before, const juce::StringArray& after);

// Only the changed lines plus 'context' unchanged lines around them.
std::vector<DiffLine> compactDiff (const std::vector<DiffLine>& diff, int context = 2);

} // namespace pc
