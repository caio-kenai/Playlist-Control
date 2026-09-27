#pragma once

#include "core/TextCodec.h"

#include <optional>
#include <string>

namespace pc
{

// XML document read by a small scanner that records where every element's
// content starts and ends. Writing replaces only the content of the changed
// element, so indentation, attribute order, the declaration and the way the
// Playlist writes empty values ("<x>\r\n\t</x>") stay as they were.
class XmlPatchDocument
{
public:
    struct Node
    {
        juce::String name;
        juce::StringPairArray attributes;
        int start = 0;        // '<' of the start tag
        int contentStart = 0; // after '>' of the start tag
        int contentEnd = 0;   // '<' of the end tag
        int end = 0;          // after '>' of the end tag
        bool selfClosing = false;
        int parent = -1;
        std::vector<int> children;
    };

    static std::optional<XmlPatchDocument> parse (const juce::MemoryBlock& bytes, juce::String& error);
    static std::optional<XmlPatchDocument> parseText (const juce::String& text, TextEncoding encoding, juce::String& error);

    TextEncoding encoding() const noexcept { return encoding_; }
    const juce::String& text() const noexcept { return text_; }
    bool toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad = nullptr) const;

    int root() const noexcept { return nodes_.empty() ? -1 : 0; }
    const Node& node (int index) const { return nodes_[(size_t) index]; }
    int childNamed (int parent, const juce::String& name) const;
    int find (const juce::String& slashPath) const; // relative to the root, e.g. "Saidas_DEV07/Programacao"

    // Element text with entities decoded; whitespace-only content is empty.
    juce::String value (int index) const;
    bool hasChildElements (int index) const { return ! nodes_[(size_t) index].children.empty(); }

    // Replaces the text of a leaf element. Returns false for elements with
    // children or self-closing elements that cannot be patched.
    bool setValue (int index, const juce::String& newValue);

    // 1-based line of an element.
    int lineOf (int index) const;

    static juce::String escape (const juce::String& s);
    static juce::String unescape (const juce::String& s);

private:
    bool scan (juce::String& error);
    juce::String indentationOf (int position) const;
    juce::String dominantEol() const;
    int lineAt (int position) const;

    juce::String text_;
    std::u32string w_;
    std::vector<int> lineStarts_;
    TextEncoding encoding_ = TextEncoding::utf8;
    std::vector<Node> nodes_;
};

} // namespace pc
