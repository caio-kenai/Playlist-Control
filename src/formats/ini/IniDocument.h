#pragma once

#include "core/TextCodec.h"

#include <optional>

namespace pc
{

// INI file edited in place. Every line keeps its original text; only the
// value part of a changed key is rewritten. Comments, blank lines, order,
// case, unknown sections, commented-out sections (";[RDS]"), line endings and
// a missing final newline are preserved.
//
// Lookup follows GetPrivateProfileString: section and key names are
// case-insensitive, the first occurrence wins and values are trimmed.
class IniDocument
{
public:
    enum class LineKind
    {
        blank,
        comment,
        section,
        keyValue,
        other
    };

    struct Line
    {
        LineKind kind = LineKind::other;
        juce::String raw;
        juce::String eol;
        juce::String name;  // section name or key
        juce::String value; // trimmed value for keyValue lines
        int valueStart = 0; // index in raw where the value text begins
        int section = -1;   // index of the owning section line, -1 before the first section
    };

    IniDocument() = default;
    static IniDocument fromLines (TextLines lines);
    static IniDocument fromBytes (const juce::MemoryBlock& bytes);

    bool toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad = nullptr) const;
    juce::String toString() const;

    TextEncoding encoding() const noexcept { return encoding_; }
    const std::vector<Line>& lines() const noexcept { return lines_; }

    juce::StringArray sectionNames() const;
    juce::StringArray commentedSectionNames() const;
    bool hasSection (const juce::String& section) const;

    std::optional<juce::String> get (const juce::String& section, const juce::String& key) const;
    juce::String getOr (const juce::String& section, const juce::String& key, const juce::String& fallback = {}) const;

    // Keys of a section in file order (first occurrence of each key).
    std::vector<std::pair<juce::String, juce::String>> entries (const juce::String& section) const;

    // Updates the key in place, or adds it at the end of the section, or adds
    // the section at the end of the file.
    void set (const juce::String& section, const juce::String& key, const juce::String& value);
    bool remove (const juce::String& section, const juce::String& key);
    bool removeSection (const juce::String& section);

    // 1-based line number of a key or section header, 0 when absent.
    int lineOf (const juce::String& section, const juce::String& key = {}) const;

private:
    void reindex();
    int findSection (const juce::String& section) const;
    int findKey (int sectionLine, const juce::String& key) const;
    int lastContentLineOf (int sectionLine) const;
    void insertLine (int index, const juce::String& raw);
    static Line classify (const juce::String& raw, const juce::String& eol);

    std::vector<Line> lines_;
    TextEncoding encoding_ = TextEncoding::ascii;
    juce::String eol_ = "\r\n";
};

} // namespace pc
