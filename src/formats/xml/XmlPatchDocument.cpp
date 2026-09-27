#include "formats/xml/XmlPatchDocument.h"

#include <algorithm>

namespace pc
{

// juce::String stores UTF-8, so indexing it is linear. The scanner works on a
// UTF-32 copy and positions are code point indexes into that copy.

namespace
{
std::u32string toU32 (const juce::String& s)
{
    std::u32string out;
    out.reserve ((size_t) s.getNumBytesAsUTF8());
    for (auto p = s.getCharPointer(); ! p.isEmpty();)
        out.push_back ((char32_t) p.getAndAdvance());
    return out;
}

juce::String fromU32 (const std::u32string& w, size_t start, size_t end)
{
    end = std::min (end, w.size());
    if (start >= end)
        return {};
    std::u32string part (w.begin() + (std::ptrdiff_t) start, w.begin() + (std::ptrdiff_t) end);
    return juce::String (juce::CharPointer_UTF32 (reinterpret_cast<const juce::CharPointer_UTF32::CharType*> (part.c_str())));
}

bool isSpace (char32_t c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int findFrom (const std::u32string& w, int from, const char32_t* what)
{
    auto pos = w.find (what, (size_t) from);
    return pos == std::u32string::npos ? -1 : (int) pos;
}

int findChar (const std::u32string& w, int from, char32_t c)
{
    auto pos = w.find (c, (size_t) from);
    return pos == std::u32string::npos ? -1 : (int) pos;
}
} // namespace

juce::String XmlPatchDocument::escape (const juce::String& s)
{
    return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;");
}

juce::String XmlPatchDocument::unescape (const juce::String& s)
{
    if (! s.containsChar ('&'))
        return s;
    auto w = toU32 (s);
    std::u32string out;
    for (size_t i = 0; i < w.size(); ++i)
    {
        if (w[i] != '&')
        {
            out.push_back (w[i]);
            continue;
        }
        auto semi = w.find (U';', i);
        if (semi == std::u32string::npos)
        {
            out.push_back (w[i]);
            continue;
        }
        auto ent = fromU32 (w, i + 1, semi);
        juce::juce_wchar c = 0;
        if (ent == "amp") c = '&';
        else if (ent == "lt") c = '<';
        else if (ent == "gt") c = '>';
        else if (ent == "quot") c = '"';
        else if (ent == "apos") c = '\'';
        else if (ent.startsWithChar ('#'))
            c = (juce::juce_wchar) (ent[1] == 'x' || ent[1] == 'X' ? ent.substring (2).getHexValue32()
                                                                    : ent.substring (1).getIntValue());
        if (c == 0)
        {
            out.push_back (w[i]);
            continue;
        }
        out.push_back ((char32_t) c);
        i = semi;
    }
    return fromU32 (out, 0, out.size());
}

std::optional<XmlPatchDocument> XmlPatchDocument::parse (const juce::MemoryBlock& bytes, juce::String& error)
{
    auto decoded = decodeText (bytes);
    return parseText (decoded.text, decoded.encoding, error);
}

std::optional<XmlPatchDocument> XmlPatchDocument::parseText (const juce::String& text, TextEncoding encoding, juce::String& error)
{
    XmlPatchDocument d;
    d.text_ = text;
    d.encoding_ = encoding;
    if (! d.scan (error))
        return std::nullopt;
    return d;
}

bool XmlPatchDocument::scan (juce::String& error)
{
    nodes_.clear();
    w_ = toU32 (text_);
    const auto& t = w_;
    const int n = (int) t.size();

    lineStarts_.clear();
    lineStarts_.push_back (0);
    for (int k = 0; k < n; ++k)
        if (t[(size_t) k] == '\n')
            lineStarts_.push_back (k + 1);

    std::vector<int> stack;
    int i = 0;

    auto fail = [&] (const juce::String& message, int pos) {
        error = message + " (linha " + juce::String (lineAt (pos)) + ")";
        return false;
    };
    auto at = [&] (int k) -> char32_t { return k < n ? t[(size_t) k] : 0; };

    while (i < n)
    {
        if (t[(size_t) i] != '<')
        {
            ++i;
            continue;
        }
        if (t.compare ((size_t) i, 4, U"<!--") == 0)
        {
            auto e = findFrom (t, i + 4, U"-->");
            if (e < 0) return fail (L"Comentário sem fim", i);
            i = e + 3;
            continue;
        }
        if (t.compare ((size_t) i, 9, U"<![CDATA[") == 0)
        {
            auto e = findFrom (t, i + 9, U"]]>");
            if (e < 0) return fail ("CDATA sem fim", i);
            i = e + 3;
            continue;
        }
        if (at (i + 1) == '?' || at (i + 1) == '!')
        {
            auto e = findChar (t, i, '>');
            if (e < 0) return fail (L"Declaração sem fim", i);
            i = e + 1;
            continue;
        }
        if (at (i + 1) == '/')
        {
            auto e = findChar (t, i, '>');
            if (e < 0) return fail ("Tag de fechamento sem fim", i);
            auto name = fromU32 (t, (size_t) i + 2, (size_t) e).trim();
            if (stack.empty() || nodes_[(size_t) stack.back()].name != name)
                return fail ("Tag de fechamento inesperada </" + name + ">", i);
            auto& node = nodes_[(size_t) stack.back()];
            node.contentEnd = i;
            node.end = e + 1;
            stack.pop_back();
            i = e + 1;
            continue;
        }

        // Start tag; attribute values may contain '>'.
        int j = i + 1;
        char32_t quote = 0;
        while (j < n)
        {
            auto c = t[(size_t) j];
            if (quote != 0) { if (c == quote) quote = 0; }
            else if (c == '"' || c == '\'') quote = c;
            else if (c == '>') break;
            ++j;
        }
        if (j >= n)
            return fail ("Tag sem fim", i);

        Node node;
        node.start = i;
        int innerEnd = j;
        node.selfClosing = t[(size_t) j - 1] == '/';
        if (node.selfClosing)
            --innerEnd;
        int k = i + 1;
        while (k < innerEnd && ! isSpace (t[(size_t) k]))
            ++k;
        node.name = fromU32 (t, (size_t) i + 1, (size_t) k);
        if (node.name.isEmpty())
            return fail ("Elemento sem nome", i);

        // Attributes: name="value" pairs.
        int p = k;
        while (p < innerEnd)
        {
            while (p < innerEnd && isSpace (t[(size_t) p])) ++p;
            int eq = findChar (t, p, '=');
            if (eq < 0 || eq >= innerEnd) break;
            auto attrName = fromU32 (t, (size_t) p, (size_t) eq).trim();
            int q = eq + 1;
            while (q < innerEnd && isSpace (t[(size_t) q])) ++q;
            if (q >= innerEnd) break;
            auto qc = t[(size_t) q];
            if (qc != '"' && qc != '\'') break;
            int close = findChar (t, q + 1, qc);
            if (close < 0 || close >= innerEnd) break;
            node.attributes.set (attrName, unescape (fromU32 (t, (size_t) q + 1, (size_t) close)));
            p = close + 1;
        }

        node.contentStart = j + 1;
        node.contentEnd = j + 1;
        node.end = j + 1;
        node.parent = stack.empty() ? -1 : stack.back();
        if (stack.empty() && ! nodes_.empty())
            return fail ("Mais de um elemento raiz", i);

        auto index = (int) nodes_.size();
        nodes_.push_back (node);
        if (node.parent >= 0)
            nodes_[(size_t) node.parent].children.push_back (index);
        if (! node.selfClosing)
            stack.push_back (index);
        i = j + 1;
    }

    if (! stack.empty())
        return fail ("Elemento <" + nodes_[(size_t) stack.back()].name + L"> não foi fechado", nodes_[(size_t) stack.back()].start);
    if (nodes_.empty())
        return fail ("Nenhum elemento encontrado", 0);
    return true;
}

int XmlPatchDocument::lineAt (int position) const
{
    auto it = std::upper_bound (lineStarts_.begin(), lineStarts_.end(), position);
    return (int) (it - lineStarts_.begin());
}

bool XmlPatchDocument::toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad) const
{
    return encodeText (text_, encoding_, out, firstBad);
}

int XmlPatchDocument::childNamed (int parent, const juce::String& name) const
{
    if (parent < 0)
        return -1;
    for (auto c : nodes_[(size_t) parent].children)
        if (nodes_[(size_t) c].name == name)
            return c;
    return -1;
}

int XmlPatchDocument::find (const juce::String& slashPath) const
{
    int current = root();
    for (auto& part : juce::StringArray::fromTokens (slashPath, "/", {}))
    {
        if (part.isEmpty())
            continue;
        current = childNamed (current, part);
        if (current < 0)
            return -1;
    }
    return current;
}

juce::String XmlPatchDocument::value (int index) const
{
    auto& nd = nodes_[(size_t) index];
    if (nd.selfClosing)
        return {};
    auto raw = fromU32 (w_, (size_t) nd.contentStart, (size_t) nd.contentEnd);
    if (raw.trim().isEmpty())
        return {};
    return unescape (raw);
}

juce::String XmlPatchDocument::indentationOf (int position) const
{
    int lineStart = lineStarts_[(size_t) lineAt (position) - 1];
    int k = lineStart;
    while (k < position && (w_[(size_t) k] == ' ' || w_[(size_t) k] == '\t'))
        ++k;
    return fromU32 (w_, (size_t) lineStart, (size_t) k);
}

juce::String XmlPatchDocument::dominantEol() const
{
    return text_.contains ("\r\n") || ! text_.containsChar ('\n') ? "\r\n" : "\n";
}

bool XmlPatchDocument::setValue (int index, const juce::String& newValue)
{
    auto& nd = nodes_[(size_t) index];
    if (nd.selfClosing || ! nd.children.empty())
        return false;
    if (value (index) == newValue)
        return true;

    // An empty value is written the way the Playlist writes it: newline plus
    // the element's indentation.
    auto content = newValue.isEmpty() ? dominantEol() + indentationOf (nd.start) : escape (newValue);
    text_ = fromU32 (w_, 0, (size_t) nd.contentStart) + content + fromU32 (w_, (size_t) nd.contentEnd, w_.size());
    juce::String error;
    auto ok = scan (error);
    jassert (ok);
    return ok;
}

juce::String XmlPatchDocument::textBetween (int start, int end) const
{
    return fromU32 (w_, (size_t) juce::jmax (0, start), (size_t) juce::jmax (0, end));
}

int XmlPatchDocument::lineOf (int index) const
{
    return lineAt (nodes_[(size_t) index].start);
}

} // namespace pc
