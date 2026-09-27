#pragma once

#include "core/TextCodec.h"
#include "core/TimeOfDay.h"

#include <optional>

namespace pc
{

// One entry of a block in the TXT1 format.
enum class ItemKind
{
    code,        // VH, 55, MUS1, MUS1-R (registered code or folder code)
    quotedFile,  // "Artista - Musica.mp3" (Maker grades)
    codeAndFile, // "H6D26P73FUHF|rec_1767811356019.aac" (Planner / Sync Service)
    command,     // <IANEWS>
    bareText,    // text with spaces and no quotes (hand-edited files)
    empty        // nothing between two commas
};

juce::String toDisplayString (ItemKind kind);

struct ScheduleItem
{
    ItemKind kind = ItemKind::empty;
    juce::String raw;  // as written, without surrounding spaces
    juce::String code; // code / command name
    juce::String file; // file name for quoted items
    bool chorus = false; // code ending in -R (manual: programs the chorus)

    static ScheduleItem parse (const juce::String& text);
    static ScheduleItem makeCode (const juce::String& code);
    static ScheduleItem makeFile (const juce::String& file);
    static ScheduleItem makeCodeAndFile (const juce::String& code, const juce::String& file);
    static ScheduleItem makeCommand (const juce::String& name);

    // Code without the -R suffix.
    juce::String baseCode() const;
    juce::String displayText() const;
};

// Parameter inside the parentheses after the time: ID=..., DUR=..., FIXO,
// LOCAL, SAT, LOCKED, DESCARTE, or anything else (kept as written).
struct BlockParam
{
    juce::String name;
    juce::String value;
    bool hasValue = false;

    juce::String toString() const { return hasValue ? name + "=" + value : name; }
};

class BlockParams
{
public:
    static BlockParams parse (const juce::String& inner);
    juce::String toString() const;

    bool empty() const noexcept { return items_.empty(); }
    const std::vector<BlockParam>& items() const noexcept { return items_; }

    bool has (const juce::String& name) const;
    std::optional<juce::String> value (const juce::String& name) const;
    void setFlag (const juce::String& name, bool on);
    void setValue (const juce::String& name, const juce::String& value); // empty value removes
    void remove (const juce::String& name);

    static bool isKnown (const juce::String& name);

private:
    std::vector<BlockParam> items_;
};

// Block duration from DUR. "3:00" is minutes:seconds (manual); a bare number
// is seconds (the Planner writes DUR=300 for its 5-minute blocks).
struct BlockDuration
{
    bool valid = false;
    int seconds = 0;
    bool numeric = false; // bare number of seconds, as the Planner writes
};
BlockDuration parseDuration (const juce::String& value);
juce::String formatDuration (int seconds); // "m:ss"

struct ScheduleBlock
{
    TimeOfDay time;
    juce::String timeText;
    bool canonicalTime = true;
    bool hasParams = false; // parentheses were present
    BlockParams params;
    std::vector<ScheduleItem> items;
};

struct ScheduleLine
{
    enum class Kind
    {
        block,
        blank,
        invalid
    };

    Kind kind = Kind::invalid;
    juce::String raw;
    juce::String eol;
    ScheduleBlock block;
    bool dirty = false;

    // Formatting of the original line, reused when it is rebuilt.
    juce::String separator = ", ";
    juce::String trailing;     // after the last item: ", ", "," or ""
    juce::String emptyTail;    // text after time/params of an empty block
    bool separatorKnown = false, trailingKnown = false, emptyTailKnown = false;
};

// Style of generated lines for this file, inferred from its content.
struct ScheduleStyle
{
    juce::String separator = ", ";
    juce::String trailing;
    juce::String emptyTail;
    juce::String eol = "\r\n";
};

// Maps, grades, clocks and programming models. Unchanged lines are written
// back exactly as read.
class ScheduleDocument
{
public:
    static ScheduleDocument fromLines (TextLines lines);
    static ScheduleDocument fromBytes (const juce::MemoryBlock& bytes);

    juce::String toString() const;
    bool toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad = nullptr) const;

    TextEncoding encoding() const noexcept { return encoding_; }
    const ScheduleStyle& style() const noexcept { return style_; }

    std::vector<ScheduleLine>& lines() noexcept { return lines_; }
    const std::vector<ScheduleLine>& lines() const noexcept { return lines_; }

    // Indexes of block lines, in file order.
    std::vector<int> blockIndexes() const;
    int findBlock (TimeOfDay time) const;

    // Rebuilds the text of a line from its block after edits.
    void markDirty (int index);

    // Inserts a block keeping the file ordered by time; returns its index.
    int insertBlock (const ScheduleBlock& block);
    void removeLine (int index);

    static juce::String renderBlock (const ScheduleBlock& block, const juce::String& separator,
                                     const juce::String& trailing, const juce::String& emptyTail);
    static ScheduleLine parseLine (const juce::String& raw, const juce::String& eol);

private:
    void inferStyle();

    std::vector<ScheduleLine> lines_;
    TextEncoding encoding_ = TextEncoding::ascii;
    ScheduleStyle style_;
};

} // namespace pc
