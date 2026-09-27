#include "formats/montagem/MontagemFile.h"

namespace pc
{

namespace
{
bool readQuoted (const juce::String& s, int& pos, juce::String& out)
{
    while (pos < s.length() && (s[pos] == ' ' || s[pos] == ','))
        ++pos;
    if (pos >= s.length() || s[pos] != '"')
        return false;
    auto close = s.indexOfChar (pos + 1, '"');
    if (close < 0)
        return false;
    out = s.substring (pos + 1, close);
    pos = close + 1;
    return true;
}
} // namespace

MontagemFile MontagemFile::parse (const juce::MemoryBlock& bytes)
{
    MontagemFile m;
    auto lines = TextLines::fromBytes (bytes);
    m.encoding = lines.encoding;
    int number = 0;
    for (auto& l : lines.lines)
    {
        ++number;
        auto s = l.content;
        if (s.trim().isEmpty())
            continue;

        auto t = TimeOfDay::parsePrefix (s);
        MontagemEntry e;
        e.line = number;
        bool ok = t.time.has_value();
        int pos = t.length;
        if (ok)
        {
            e.block = *t.time;
            while (pos < s.length() && s[pos] == ' ')
                ++pos;
            ok = pos < s.length() && (s[pos] == 'M' || s[pos] == 'C');
        }
        if (ok)
        {
            e.type = s[pos];
            auto comma = s.indexOfChar (pos, ',');
            auto next = comma >= 0 ? s.indexOfChar (comma + 1, ',') : -1;
            ok = comma >= 0 && next >= 0;
            if (ok)
            {
                auto posText = s.substring (comma + 1, next).trim();
                ok = posText.isNotEmpty() && posText.trimCharactersAtStart ("-").containsOnly ("0123456789");
                e.position = posText.getIntValue();
                pos = next;
            }
        }
        ok = ok && readQuoted (s, pos, e.folder) && readQuoted (s, pos, e.file);
        if (ok)
            m.entries.push_back (e);
        else
            m.invalidLines.emplace_back (number, s);
    }
    return m;
}

} // namespace pc
