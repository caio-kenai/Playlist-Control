#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

// Encodings found in Playlist files: plain ASCII (most generated files),
// Windows-1252 (Maker grades, DBF text), UTF-8 with or without BOM (files
// edited in Notepad, Folders.xml).
enum class TextEncoding
{
    ascii,
    utf8,
    utf8Bom,
    windows1252
};

juce::String toDisplayString (TextEncoding encoding);

struct DecodedText
{
    juce::String text;
    TextEncoding encoding = TextEncoding::ascii;
};

// Detection order: UTF-8 BOM, pure ASCII, valid UTF-8, otherwise Windows-1252.
// Windows-1252 decoding is bijective (the five undefined bytes map to the
// matching C1 code points), so decode + encode always returns the same bytes.
DecodedText decodeText (const void* data, size_t size);
DecodedText decodeText (const juce::MemoryBlock& block);

// Returns false when the text contains characters that the encoding cannot
// represent; 'firstBad' receives the first such character.
bool encodeText (const juce::String& text, TextEncoding encoding, juce::MemoryBlock& out,
                 juce::juce_wchar* firstBad = nullptr);

juce::String decodeWindows1252 (const void* data, size_t size);
bool isRepresentableIn1252 (juce::juce_wchar c) noexcept;

// A text file split into lines that keep their own terminators, so a file with
// mixed CRLF/LF or without a final newline is written back unchanged.
struct TextLine
{
    juce::String content;
    juce::String eol; // "\r\n", "\n", "\r" or "" for the last line without terminator
};

struct TextLines
{
    std::vector<TextLine> lines;
    TextEncoding encoding = TextEncoding::ascii;

    static TextLines split (const DecodedText& decoded);
    static TextLines fromBytes (const juce::MemoryBlock& block);

    juce::String join() const;
    bool toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad = nullptr) const;

    // Terminator used by most lines; CRLF when the file has none.
    juce::String dominantEol() const;
};

} // namespace pc
