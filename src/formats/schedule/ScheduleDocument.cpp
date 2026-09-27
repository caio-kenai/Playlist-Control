#include "formats/schedule/ScheduleDocument.h"

#include <map>

namespace pc
{

juce::String toDisplayString (ItemKind kind)
{
    switch (kind)
    {
        case ItemKind::code:        return L"Código";
        case ItemKind::quotedFile:  return "Arquivo";
        case ItemKind::codeAndFile: return L"Código e arquivo";
        case ItemKind::command:     return "Comando";
        case ItemKind::bareText:    return "Texto sem aspas";
        case ItemKind::empty:       return "Vazio";
    }
    return {};
}

// ----------------------------------------------------------------------------
// Items

ScheduleItem ScheduleItem::parse (const juce::String& text)
{
    ScheduleItem it;
    it.raw = text.trim();
    const auto& t = it.raw;

    if (t.isEmpty())
    {
        it.kind = ItemKind::empty;
    }
    else if (t.length() >= 2 && t.startsWithChar ('"') && t.endsWithChar ('"'))
    {
        auto inner = t.substring (1, t.length() - 1);
        auto bar = inner.indexOfChar ('|');
        if (bar >= 0)
        {
            it.kind = ItemKind::codeAndFile;
            it.code = inner.substring (0, bar).trim();
            it.file = inner.substring (bar + 1).trim();
        }
        else
        {
            it.kind = ItemKind::quotedFile;
            it.file = inner;
        }
    }
    else if (t.length() >= 2 && t.startsWithChar ('<') && t.endsWithChar ('>'))
    {
        it.kind = ItemKind::command;
        it.code = t.substring (1, t.length() - 1).trim();
    }
    else if (t.containsAnyOf (" \t\""))
    {
        it.kind = ItemKind::bareText;
        it.file = t;
    }
    else
    {
        it.kind = ItemKind::code;
        it.code = t;
        it.chorus = t.length() > 2 && t.endsWithIgnoreCase ("-R");
    }
    return it;
}

ScheduleItem ScheduleItem::makeCode (const juce::String& code)
{
    return parse (code.trim().removeCharacters (" \t,\"<>"));
}

ScheduleItem ScheduleItem::makeFile (const juce::String& file)
{
    return parse ("\"" + file.trim().removeCharacters ("\"") + "\"");
}

ScheduleItem ScheduleItem::makeCodeAndFile (const juce::String& code, const juce::String& file)
{
    return parse ("\"" + code.trim().removeCharacters ("\"|") + "|" + file.trim().removeCharacters ("\"") + "\"");
}

ScheduleItem ScheduleItem::makeCommand (const juce::String& name)
{
    return parse ("<" + name.trim().removeCharacters ("<>,") + ">");
}

juce::String ScheduleItem::baseCode() const
{
    return chorus ? code.dropLastCharacters (2) : code;
}

juce::String ScheduleItem::displayText() const
{
    switch (kind)
    {
        case ItemKind::code:        return code;
        case ItemKind::quotedFile:  return file;
        case ItemKind::codeAndFile: return file + "  [" + code + "]";
        case ItemKind::command:     return "<" + code + ">";
        case ItemKind::bareText:    return file;
        case ItemKind::empty:       return "(vazio)";
    }
    return raw;
}

// ----------------------------------------------------------------------------
// Parameters

BlockParams BlockParams::parse (const juce::String& inner)
{
    BlockParams p;
    for (auto& token : juce::StringArray::fromTokens (inner, ",", {}))
    {
        auto t = token.trim();
        if (t.isEmpty())
            continue;
        BlockParam bp;
        auto eq = t.indexOfChar ('=');
        if (eq >= 0)
        {
            bp.name = t.substring (0, eq).trim();
            bp.value = t.substring (eq + 1).trim();
            bp.hasValue = true;
        }
        else
        {
            bp.name = t;
        }
        p.items_.push_back (bp);
    }
    return p;
}

juce::String BlockParams::toString() const
{
    juce::StringArray parts;
    for (auto& i : items_)
        parts.add (i.toString());
    return parts.joinIntoString (", ");
}

bool BlockParams::has (const juce::String& name) const
{
    for (auto& i : items_)
        if (i.name.equalsIgnoreCase (name))
            return true;
    return false;
}

std::optional<juce::String> BlockParams::value (const juce::String& name) const
{
    for (auto& i : items_)
        if (i.name.equalsIgnoreCase (name))
            return i.value;
    return std::nullopt;
}

void BlockParams::setFlag (const juce::String& name, bool on)
{
    if (on == has (name))
        return;
    if (on)
        items_.push_back ({ name, {}, false });
    else
        remove (name);
}

void BlockParams::setValue (const juce::String& name, const juce::String& value)
{
    auto v = value.trim();
    if (v.isEmpty())
    {
        remove (name);
        return;
    }
    for (auto& i : items_)
    {
        if (i.name.equalsIgnoreCase (name))
        {
            i.value = v;
            i.hasValue = true;
            return;
        }
    }
    items_.push_back ({ name, v, true });
}

void BlockParams::remove (const juce::String& name)
{
    items_.erase (std::remove_if (items_.begin(), items_.end(),
                                  [&] (const BlockParam& p) { return p.name.equalsIgnoreCase (name); }),
                  items_.end());
}

bool BlockParams::isKnown (const juce::String& name)
{
    for (auto* k : { "ID", "DUR", "FIXO", "LOCAL", "SAT", "LOCKED", "DESCARTE" })
        if (name.equalsIgnoreCase (k))
            return true;
    return false;
}

BlockDuration parseDuration (const juce::String& value)
{
    BlockDuration d;
    auto v = value.trim();
    if (v.isEmpty())
        return d;
    auto colon = v.indexOfChar (':');
    if (colon >= 0)
    {
        auto m = v.substring (0, colon);
        auto s = v.substring (colon + 1);
        if (m.isEmpty() || ! m.containsOnly ("0123456789") || s.length() != 2 || ! s.containsOnly ("0123456789"))
            return d;
        if (s.getIntValue() > 59)
            return d;
        d.valid = true;
        d.seconds = m.getIntValue() * 60 + s.getIntValue();
        return d;
    }
    if (! v.containsOnly ("0123456789") || v.length() > 6)
        return d;
    d.valid = true;
    d.seconds = v.getIntValue();
    d.numeric = true;
    return d;
}

juce::String formatDuration (int seconds)
{
    return juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2);
}

// ----------------------------------------------------------------------------
// Lines

namespace
{
// Splits on commas that are not inside quotes or angle brackets.
juce::StringArray splitItems (const juce::String& text)
{
    juce::StringArray parts;
    juce::String current;
    bool inQuotes = false, inAngle = false;
    for (auto p = text.getCharPointer(); ! p.isEmpty();)
    {
        auto c = p.getAndAdvance();
        if (c == '"' && ! inAngle)
            inQuotes = ! inQuotes;
        else if (c == '<' && ! inQuotes)
            inAngle = true;
        else if (c == '>' && ! inQuotes)
            inAngle = false;

        if (c == ',' && ! inQuotes && ! inAngle)
        {
            parts.add (current);
            current.clear();
        }
        else
        {
            current += c;
        }
    }
    parts.add (current);
    return parts;
}
} // namespace

ScheduleLine ScheduleDocument::parseLine (const juce::String& raw, const juce::String& eol)
{
    ScheduleLine line;
    line.raw = raw;
    line.eol = eol;

    if (raw.trim().isEmpty())
    {
        line.kind = ScheduleLine::Kind::blank;
        return line;
    }

    auto parsed = TimeOfDay::parsePrefix (raw);
    if (! parsed.time.has_value())
    {
        line.kind = ScheduleLine::Kind::invalid;
        return line;
    }

    line.kind = ScheduleLine::Kind::block;
    auto& b = line.block;
    b.time = *parsed.time;
    b.timeText = raw.substring (0, parsed.length);
    b.canonicalTime = parsed.canonical;

    auto rest = raw.substring (parsed.length);
    // "12:00x" is not a block line.
    if (rest.isNotEmpty() && rest[0] != ' ' && rest[0] != '\t' && rest[0] != '(')
    {
        line.kind = ScheduleLine::Kind::invalid;
        return line;
    }

    auto afterTime = rest.trimStart();
    if (afterTime.startsWithChar ('('))
    {
        auto close = afterTime.indexOfChar (')');
        if (close < 0)
        {
            line.kind = ScheduleLine::Kind::invalid;
            return line;
        }
        b.hasParams = true;
        b.params = BlockParams::parse (afterTime.substring (1, close));
        rest = afterTime.substring (close + 1);
    }

    if (rest.trim().isEmpty())
    {
        line.emptyTail = rest;
        line.emptyTailKnown = true;
        return line;
    }

    auto itemsText = rest.trimStart();
    auto parts = splitItems (itemsText);

    // A comma after the last item is a terminator, not an empty item.
    if (parts.size() > 1 && parts[parts.size() - 1].trim().isEmpty())
    {
        line.trailing = "," + parts[parts.size() - 1];
        parts.remove (parts.size() - 1);
    }
    line.trailingKnown = true;
    if (parts.size() > 1)
    {
        line.separator = parts[1].startsWithChar (' ') ? ", " : ",";
        line.separatorKnown = true;
    }

    for (auto& p : parts)
        b.items.push_back (ScheduleItem::parse (p));
    return line;
}

juce::String ScheduleDocument::renderBlock (const ScheduleBlock& b, const juce::String& separator,
                                            const juce::String& trailing, const juce::String& emptyTail)
{
    juce::String s = b.canonicalTime ? b.timeText : b.time.toString();
    if (s.isEmpty())
        s = b.time.toString();
    if (! b.params.empty())
        s << " (" << b.params.toString() << ")";

    if (b.items.empty())
        return s + emptyTail;

    juce::StringArray parts;
    for (auto& it : b.items)
        parts.add (it.raw);
    s << " " << parts.joinIntoString (separator) << trailing;
    return s;
}

ScheduleDocument ScheduleDocument::fromLines (TextLines text)
{
    ScheduleDocument d;
    d.encoding_ = text.encoding;
    d.style_.eol = text.dominantEol();
    for (auto& l : text.lines)
        d.lines_.push_back (parseLine (l.content, l.eol));
    d.inferStyle();
    return d;
}

ScheduleDocument ScheduleDocument::fromBytes (const juce::MemoryBlock& bytes)
{
    return fromLines (TextLines::fromBytes (bytes));
}

void ScheduleDocument::inferStyle()
{
    int comma = 0, commaSpace = 0;
    std::map<juce::String, int> trailing, emptyTail;
    for (auto& l : lines_)
    {
        if (l.kind != ScheduleLine::Kind::block)
            continue;
        if (l.block.items.empty())
        {
            ++emptyTail[l.emptyTail];
            continue;
        }
        ++trailing[l.trailing];
        if (l.block.items.size() > 1)
            (l.separator == "," ? comma : commaSpace)++;
    }
    style_.separator = comma > commaSpace ? "," : ", ";
    auto mostCommon = [] (const std::map<juce::String, int>& m, const juce::String& fallback) {
        juce::String best = fallback;
        int n = -1;
        for (auto& [k, v] : m)
            if (v > n) { best = k; n = v; }
        return best;
    };
    style_.trailing = mostCommon (trailing, {});
    style_.emptyTail = mostCommon (emptyTail, {});
}

juce::String ScheduleDocument::toString() const
{
    juce::String s;
    for (auto& l : lines_)
        s << l.raw << l.eol;
    return s;
}

bool ScheduleDocument::toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad) const
{
    return encodeText (toString(), encoding_, out, firstBad);
}

std::vector<int> ScheduleDocument::blockIndexes() const
{
    std::vector<int> idx;
    for (int i = 0; i < (int) lines_.size(); ++i)
        if (lines_[(size_t) i].kind == ScheduleLine::Kind::block)
            idx.push_back (i);
    return idx;
}

int ScheduleDocument::findBlock (TimeOfDay time) const
{
    for (int i = 0; i < (int) lines_.size(); ++i)
        if (lines_[(size_t) i].kind == ScheduleLine::Kind::block && lines_[(size_t) i].block.time == time)
            return i;
    return -1;
}

void ScheduleDocument::markDirty (int index)
{
    auto& l = lines_[(size_t) index];
    if (l.kind != ScheduleLine::Kind::block)
        return;
    l.dirty = true;
    // Formatting the line did not show (e.g. a block that was empty) comes
    // from the rest of the file.
    if (! l.separatorKnown)  l.separator = style_.separator;
    if (! l.trailingKnown)   l.trailing = style_.trailing;
    if (! l.emptyTailKnown)  l.emptyTail = style_.emptyTail;
    l.raw = renderBlock (l.block, l.separator, l.trailing, l.emptyTail);
}

int ScheduleDocument::insertBlock (const ScheduleBlock& block)
{
    int insertAt = (int) lines_.size();
    for (int i = 0; i < (int) lines_.size(); ++i)
    {
        auto& l = lines_[(size_t) i];
        if (l.kind == ScheduleLine::Kind::block && block.time < l.block.time)
        {
            insertAt = i;
            break;
        }
    }

    ScheduleLine line;
    line.kind = ScheduleLine::Kind::block;
    line.block = block;
    line.block.timeText = block.time.toString();
    line.block.canonicalTime = true;
    line.eol = style_.eol;
    // Appending after the last line of a file without a final newline keeps
    // the file without one.
    if (insertAt == (int) lines_.size() && insertAt > 0 && lines_[(size_t) insertAt - 1].eol.isEmpty())
    {
        lines_[(size_t) insertAt - 1].eol = style_.eol;
        line.eol = {};
    }
    lines_.insert (lines_.begin() + insertAt, line);
    markDirty (insertAt);
    return insertAt;
}

void ScheduleDocument::removeLine (int index)
{
    bool wasLast = index == (int) lines_.size() - 1;
    auto eol = lines_[(size_t) index].eol;
    lines_.erase (lines_.begin() + index);
    if (wasLast && index > 0 && eol.isEmpty())
        lines_[(size_t) index - 1].eol = {};
}

} // namespace pc
