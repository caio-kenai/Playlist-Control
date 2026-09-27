#include "core/LineDiff.h"

namespace pc
{

std::vector<DiffLine> diffLines (const juce::StringArray& a, const juce::StringArray& b)
{
    std::vector<DiffLine> out;
    int n = a.size(), m = b.size();
    int prefix = 0;
    while (prefix < n && prefix < m && a[prefix] == b[prefix])
        ++prefix;
    int suffix = 0;
    while (suffix < n - prefix && suffix < m - prefix && a[n - 1 - suffix] == b[m - 1 - suffix])
        ++suffix;

    for (int i = 0; i < prefix; ++i)
        out.push_back ({ DiffLine::Kind::same, a[i], i + 1, i + 1 });

    int an = n - prefix - suffix, bm = m - prefix - suffix;
    if ((juce::int64) an * bm > 4'000'000)
    {
        // Too large for a full comparison: show the middle as replaced.
        for (int i = 0; i < an; ++i)
            out.push_back ({ DiffLine::Kind::removed, a[prefix + i], prefix + i + 1, 0 });
        for (int j = 0; j < bm; ++j)
            out.push_back ({ DiffLine::Kind::added, b[prefix + j], 0, prefix + j + 1 });
    }
    else
    {
        std::vector<int> table ((size_t) (an + 1) * (size_t) (bm + 1), 0);
        auto at = [&] (int i, int j) -> int& { return table[(size_t) i * (size_t) (bm + 1) + (size_t) j]; };
        for (int i = an - 1; i >= 0; --i)
            for (int j = bm - 1; j >= 0; --j)
                at (i, j) = a[prefix + i] == b[prefix + j] ? at (i + 1, j + 1) + 1 : juce::jmax (at (i + 1, j), at (i, j + 1));
        int i = 0, j = 0;
        while (i < an || j < bm)
        {
            if (i < an && j < bm && a[prefix + i] == b[prefix + j])
            {
                out.push_back ({ DiffLine::Kind::same, a[prefix + i], prefix + i + 1, prefix + j + 1 });
                ++i;
                ++j;
            }
            else if (j < bm && (i == an || at (i, j + 1) >= at (i + 1, j)))
            {
                out.push_back ({ DiffLine::Kind::added, b[prefix + j], 0, prefix + j + 1 });
                ++j;
            }
            else
            {
                out.push_back ({ DiffLine::Kind::removed, a[prefix + i], prefix + i + 1, 0 });
                ++i;
            }
        }
    }

    for (int k = 0; k < suffix; ++k)
        out.push_back ({ DiffLine::Kind::same, a[n - suffix + k], n - suffix + k + 1, m - suffix + k + 1 });
    return out;
}

std::vector<DiffLine> compactDiff (const std::vector<DiffLine>& diff, int context)
{
    std::vector<bool> keep (diff.size(), false);
    for (int i = 0; i < (int) diff.size(); ++i)
        if (diff[(size_t) i].kind != DiffLine::Kind::same)
            for (int k = juce::jmax (0, i - context); k <= juce::jmin ((int) diff.size() - 1, i + context); ++k)
                keep[(size_t) k] = true;
    std::vector<DiffLine> out;
    for (size_t i = 0; i < diff.size(); ++i)
        if (keep[i])
            out.push_back (diff[i]);
    return out;
}

} // namespace pc
