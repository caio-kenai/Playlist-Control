#include "formats/ini/IniDocument.h"

namespace pc
{

IniDocument::Line IniDocument::classify (const juce::String& raw, const juce::String& eol)
{
    Line l;
    l.raw = raw;
    l.eol = eol;
    auto t = raw.trim();
    if (t.isEmpty())
    {
        l.kind = LineKind::blank;
    }
    else if (t.startsWithChar (';') || t.startsWithChar ('#'))
    {
        l.kind = LineKind::comment;
    }
    else if (t.startsWithChar ('[') && t.containsChar (']'))
    {
        l.kind = LineKind::section;
        l.name = t.substring (1, t.indexOfChar (']')).trim();
    }
    else if (raw.containsChar ('='))
    {
        l.kind = LineKind::keyValue;
        auto eq = raw.indexOfChar ('=');
        l.name = raw.substring (0, eq).trim();
        int v = eq + 1;
        while (v < raw.length() && (raw[v] == ' ' || raw[v] == '\t'))
            ++v;
        l.valueStart = v;
        l.value = raw.substring (v).trim();
    }
    else
    {
        l.kind = LineKind::other;
    }
    return l;
}

IniDocument IniDocument::fromLines (TextLines text)
{
    IniDocument d;
    d.encoding_ = text.encoding;
    d.eol_ = text.dominantEol();
    for (auto& l : text.lines)
        d.lines_.push_back (classify (l.content, l.eol));
    d.reindex();
    return d;
}

IniDocument IniDocument::fromBytes (const juce::MemoryBlock& bytes)
{
    return fromLines (TextLines::fromBytes (bytes));
}

void IniDocument::reindex()
{
    int current = -1;
    for (int i = 0; i < (int) lines_.size(); ++i)
    {
        if (lines_[(size_t) i].kind == LineKind::section)
            current = i;
        lines_[(size_t) i].section = lines_[(size_t) i].kind == LineKind::section ? i : current;
    }
}

juce::String IniDocument::toString() const
{
    juce::String s;
    for (auto& l : lines_)
        s << l.raw << l.eol;
    return s;
}

bool IniDocument::toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad) const
{
    return encodeText (toString(), encoding_, out, firstBad);
}

juce::StringArray IniDocument::sectionNames() const
{
    juce::StringArray names;
    for (auto& l : lines_)
        if (l.kind == LineKind::section)
            names.addIfNotAlreadyThere (l.name, true);
    return names;
}

juce::StringArray IniDocument::commentedSectionNames() const
{
    juce::StringArray names;
    for (auto& l : lines_)
    {
        if (l.kind != LineKind::comment)
            continue;
        auto t = l.raw.trim().substring (1).trim();
        if (t.startsWithChar ('[') && t.containsChar (']'))
            names.addIfNotAlreadyThere (t.substring (1, t.indexOfChar (']')).trim(), true);
    }
    return names;
}

int IniDocument::findSection (const juce::String& section) const
{
    for (int i = 0; i < (int) lines_.size(); ++i)
        if (lines_[(size_t) i].kind == LineKind::section && lines_[(size_t) i].name.equalsIgnoreCase (section))
            return i;
    return -1;
}

int IniDocument::findKey (int sectionLine, const juce::String& key) const
{
    if (sectionLine < 0)
        return -1;
    for (int i = sectionLine + 1; i < (int) lines_.size(); ++i)
    {
        auto& l = lines_[(size_t) i];
        if (l.kind == LineKind::section)
            break;
        if (l.kind == LineKind::keyValue && l.name.equalsIgnoreCase (key))
            return i;
    }
    return -1;
}

bool IniDocument::hasSection (const juce::String& section) const
{
    return findSection (section) >= 0;
}

std::optional<juce::String> IniDocument::get (const juce::String& section, const juce::String& key) const
{
    auto k = findKey (findSection (section), key);
    if (k < 0)
        return std::nullopt;
    return lines_[(size_t) k].value;
}

juce::String IniDocument::getOr (const juce::String& section, const juce::String& key, const juce::String& fallback) const
{
    auto v = get (section, key);
    return v.has_value() ? *v : fallback;
}

std::vector<std::pair<juce::String, juce::String>> IniDocument::entries (const juce::String& section) const
{
    std::vector<std::pair<juce::String, juce::String>> out;
    auto s = findSection (section);
    if (s < 0)
        return out;
    juce::StringArray seen;
    for (int i = s + 1; i < (int) lines_.size(); ++i)
    {
        auto& l = lines_[(size_t) i];
        if (l.kind == LineKind::section)
            break;
        if (l.kind == LineKind::keyValue && ! seen.contains (l.name, true))
        {
            seen.add (l.name);
            out.emplace_back (l.name, l.value);
        }
    }
    return out;
}

int IniDocument::lastContentLineOf (int sectionLine) const
{
    int last = sectionLine;
    for (int i = sectionLine + 1; i < (int) lines_.size(); ++i)
    {
        auto kind = lines_[(size_t) i].kind;
        if (kind == LineKind::section)
            break;
        if (kind == LineKind::keyValue || kind == LineKind::other)
            last = i;
    }
    return last;
}

void IniDocument::insertLine (int index, const juce::String& raw)
{
    // The line before the insertion point may be the last line of a file
    // without a final newline; it needs a terminator now.
    juce::String eol = eol_;
    if (index > 0 && lines_[(size_t) index - 1].eol.isEmpty())
    {
        lines_[(size_t) index - 1].eol = eol_;
        if (index == (int) lines_.size())
            eol = {};
    }
    lines_.insert (lines_.begin() + index, classify (raw, eol));
    reindex();
}

void IniDocument::set (const juce::String& section, const juce::String& key, const juce::String& value)
{
    auto s = findSection (section);
    if (s < 0)
    {
        bool needsBlank = ! lines_.empty() && lines_.back().kind != LineKind::blank;
        if (needsBlank)
            insertLine ((int) lines_.size(), {});
        insertLine ((int) lines_.size(), "[" + section + "]");
        insertLine ((int) lines_.size(), key + "=" + value);
        return;
    }

    auto k = findKey (s, key);
    if (k >= 0)
    {
        auto& l = lines_[(size_t) k];
        if (l.value == value)
            return;
        auto eol = l.eol;
        auto raw = l.raw.substring (0, l.valueStart) + value;
        l = classify (raw, eol);
        reindex();
        return;
    }

    insertLine (lastContentLineOf (s) + 1, key + "=" + value);
}

bool IniDocument::remove (const juce::String& section, const juce::String& key)
{
    auto k = findKey (findSection (section), key);
    if (k < 0)
        return false;
    // Keep "no final newline" when the last line goes away.
    bool wasLast = k == (int) lines_.size() - 1;
    auto eol = lines_[(size_t) k].eol;
    lines_.erase (lines_.begin() + k);
    if (wasLast && k > 0 && eol.isEmpty())
        lines_[(size_t) k - 1].eol = {};
    reindex();
    return true;
}

bool IniDocument::removeSection (const juce::String& section)
{
    auto s = findSection (section);
    if (s < 0)
        return false;
    int end = s + 1;
    while (end < (int) lines_.size() && lines_[(size_t) end].kind != LineKind::section)
        ++end;
    lines_.erase (lines_.begin() + s, lines_.begin() + end);
    reindex();
    return true;
}

int IniDocument::lineOf (const juce::String& section, const juce::String& key) const
{
    auto s = findSection (section);
    if (s < 0)
        return 0;
    if (key.isEmpty())
        return s + 1;
    auto k = findKey (s, key);
    return k < 0 ? 0 : k + 1;
}

} // namespace pc
