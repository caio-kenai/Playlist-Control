#include "formats/xml/XmlPatchDocument.h"

namespace pc
{

juce::String XmlPatchDocument::escape (const juce::String& s)
{
    return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;");
}

juce::String XmlPatchDocument::unescape (const juce::String& s)
{
    if (! s.containsChar ('&'))
        return s;
    juce::String out;
    for (int i = 0; i < s.length(); ++i)
    {
        if (s[i] != '&')
        {
            out += s[i];
            continue;
        }
        auto semi = s.indexOfChar (i, ';');
        if (semi < 0)
        {
            out += s[i];
            continue;
        }
        auto ent = s.substring (i + 1, semi);
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
            out += s[i];
            continue;
        }
        out += c;
        i = semi;
    }
    return out;
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
    const auto& t = text_;
    const int n = t.length();
    std::vector<int> stack;
    int i = 0;

    auto fail = [&] (const juce::String& message, int pos) {
        int line = 1;
        for (int k = 0; k < pos && k < n; ++k)
            if (t[k] == '\n')
                ++line;
        error = message + " (linha " + juce::String (line) + ")";
        return false;
    };

    while (i < n)
    {
        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (t.substring (i, i + 4) == "<!--")
        {
            auto e = t.indexOf (i + 4, "-->");
            if (e < 0) return fail ("Comentário sem fim", i);
            i = e + 3;
            continue;
        }
        if (t.substring (i, i + 9) == "<![CDATA[")
        {
            auto e = t.indexOf (i + 9, "]]>");
            if (e < 0) return fail ("CDATA sem fim", i);
            i = e + 3;
            continue;
        }
        if (i + 1 < n && (t[i + 1] == '?' || t[i + 1] == '!'))
        {
            auto e = t.indexOfChar (i, '>');
            if (e < 0) return fail ("Declaração sem fim", i);
            i = e + 1;
            continue;
        }
        if (i + 1 < n && t[i + 1] == '/')
        {
            auto e = t.indexOfChar (i, '>');
            if (e < 0) return fail ("Tag de fechamento sem fim", i);
            auto name = t.substring (i + 2, e).trim();
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
        juce::juce_wchar quote = 0;
        while (j < n)
        {
            auto c = t[j];
            if (quote != 0) { if (c == quote) quote = 0; }
            else if (c == '"' || c == '\'') quote = c;
            else if (c == '>') break;
            ++j;
        }
        if (j >= n)
            return fail ("Tag sem fim", i);

        Node node;
        node.start = i;
        auto inner = t.substring (i + 1, j);
        node.selfClosing = inner.endsWithChar ('/');
        if (node.selfClosing)
            inner = inner.dropLastCharacters (1);
        int k = 0;
        while (k < inner.length() && ! juce::CharacterFunctions::isWhitespace (inner[k]))
            ++k;
        node.name = inner.substring (0, k);
        if (node.name.isEmpty())
            return fail ("Elemento sem nome", i);

        // Attributes: name="value" pairs.
        auto rest = inner.substring (k);
        int p = 0;
        while (p < rest.length())
        {
            while (p < rest.length() && juce::CharacterFunctions::isWhitespace (rest[p])) ++p;
            int eq = rest.indexOfChar (p, '=');
            if (eq < 0) break;
            auto attrName = rest.substring (p, eq).trim();
            int q = eq + 1;
            while (q < rest.length() && juce::CharacterFunctions::isWhitespace (rest[q])) ++q;
            if (q >= rest.length()) break;
            auto qc = rest[q];
            if (qc != '"' && qc != '\'') break;
            auto close = rest.indexOfChar (q + 1, qc);
            if (close < 0) break;
            node.attributes.set (attrName, unescape (rest.substring (q + 1, close)));
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
        return fail ("Elemento <" + nodes_[(size_t) stack.back()].name + "> não foi fechado", nodes_[(size_t) stack.back()].start);
    if (nodes_.empty())
        return fail ("Nenhum elemento encontrado", 0);
    return true;
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
    auto raw = text_.substring (nd.contentStart, nd.contentEnd);
    if (raw.trim().isEmpty())
        return {};
    return unescape (raw);
}

juce::String XmlPatchDocument::indentationOf (int position) const
{
    int lineStart = position;
    while (lineStart > 0 && text_[lineStart - 1] != '\n' && text_[lineStart - 1] != '\r')
        --lineStart;
    juce::String indent;
    for (int k = lineStart; k < position && (text_[k] == ' ' || text_[k] == '\t'); ++k)
        indent += text_[k];
    return indent;
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

    juce::String content;
    if (newValue.isEmpty())
    {
        // Keep the original empty form if it was empty already; otherwise use
        // the form the Playlist writes: newline + the element's indentation.
        content = dominantEol() + indentationOf (nd.start);
    }
    else
    {
        content = escape (newValue);
    }
    text_ = text_.substring (0, nd.contentStart) + content + text_.substring (nd.contentEnd);
    juce::String error;
    auto ok = scan (error);
    jassert (ok);
    return ok;
}

int XmlPatchDocument::lineOf (int index) const
{
    int line = 1;
    auto pos = nodes_[(size_t) index].start;
    for (int k = 0; k < pos; ++k)
        if (text_[k] == '\n')
            ++line;
    return line;
}

} // namespace pc
