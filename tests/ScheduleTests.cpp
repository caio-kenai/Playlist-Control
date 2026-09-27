#include "TestUtils.h"
#include "formats/schedule/ScheduleDocument.h"

namespace pc::test
{

class ScheduleTests : public juce::UnitTest
{
public:
    ScheduleTests() : juce::UnitTest ("TXT1 schedules", "formats") {}

    static juce::MemoryBlock save (const ScheduleDocument& d)
    {
        juce::MemoryBlock m;
        d.toBytes (m);
        return m;
    }

    const ScheduleLine& blockAt (const ScheduleDocument& d, const char* time)
    {
        auto t = TimeOfDay::parsePrefix (time).time;
        return d.lines()[(size_t) d.findBlock (*t)];
    }

    void runTest() override
    {
        beginTest ("Every fixture round-trips byte for byte");
        for (auto* fixtureName : { "planner-map.txt", "maker-grade.txt", "manual-map.txt", "relogio.txt",
                            "mixed-grade.txt", "utf8-map.txt", "invalid.txt" })
        {
            auto original = fixture (juce::String ("schedule/") + fixtureName);
            expect (original.getSize() > 0, fixtureName);
            expect (save (ScheduleDocument::fromBytes (original)) == original, fixtureName);
        }

        beginTest ("Planner map: parameters and code|file items");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/planner-map.txt"));
            expectEquals ((int) doc.blockIndexes().size(), 72);
            auto& b = blockAt (doc, "00:20").block;
            expect (b.hasParams);
            expectEquals (b.params.value ("DUR").value_or (""), juce::String ("300"));
            expectEquals ((int) b.items.size(), 2);
            expect (b.items[0].kind == ItemKind::codeAndFile);
            expectEquals (b.items[0].code, juce::String ("A1B2C3D4E5F6"));
            expectEquals (b.items[0].file, juce::String ("spot_padaria.aac"));
            auto& p = blockAt (doc, "12:00").block;
            expectEquals (p.params.value ("ID").value_or (""), juce::String ("Programa Manha"));
            expect (p.items.empty());
            expectEquals (doc.style().trailing, juce::String (", "));
            expectEquals (doc.style().emptyTail, juce::String (" "));
        }

        beginTest ("Maker grade: Windows-1252 quoted files");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/maker-grade.txt"));
            expect (doc.encoding() == TextEncoding::windows1252);
            auto& b = blockAt (doc, "00:02").block;
            expect (b.items[1].kind == ItemKind::quotedFile);
            expectEquals (b.items[1].file, juce::String (juce::CharPointer_UTF8 ("Banda Azul - Cora\xc3\xa7\xc3\xa3o.mp3")));
        }

        beginTest ("Manual map: codes, chorus, non-canonical time, empty blocks");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/manual-map.txt"));
            auto& l = blockAt (doc, "06:15");
            expect (! l.block.canonicalTime);
            expectEquals (blockAt (doc, "06:30").separator, juce::String (","));
            expect (blockAt (doc, "07:15").block.items.empty());
            auto& chorus = blockAt (doc, "08:00").block.items[0];
            expect (chorus.chorus);
            expectEquals (chorus.baseCode(), juce::String ("MUS1"));
            expect (doc.lines().back().eol.isEmpty());
        }

        beginTest ("Clock parameters");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/relogio.txt"));
            expect (blockAt (doc, "09:30").block.params.has ("FIXO"));
            expect (blockAt (doc, "10:00").block.params.has ("SAT"));
            expectEquals (blockAt (doc, "09:15").block.params.value ("ID").value_or (""), juce::String ("OUVINTE"));
            auto d = parseDuration (*blockAt (doc, "09:45").block.params.value ("DUR"));
            expect (d.valid && d.seconds == 180 && ! d.numeric);
            expect (! BlockParams::isKnown ("XYZ"));
            auto s = parseDuration ("300");
            expect (s.valid && s.seconds == 300 && s.numeric);
            expect (! parseDuration ("3:5").valid);
            expect (! parseDuration ("abc").valid);
            expectEquals (formatDuration (780), juce::String ("13:00"));
        }

        beginTest ("Odd shapes: commands, empty items, bare text");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/mixed-grade.txt"));
            auto& a = blockAt (doc, "10:02").block;
            expect (a.items[0].kind == ItemKind::command && a.items[0].code == "IALOC");
            auto& b = blockAt (doc, "10:17").block;
            expect (b.items[0].kind == ItemKind::empty);
            expectEquals ((int) b.items.size(), 4);
            auto& c = blockAt (doc, "10:32").block;
            expect (c.items[2].kind == ItemKind::empty);
            auto& d = blockAt (doc, "10:47").block;
            expect (d.items[1].kind == ItemKind::bareText);
        }

        beginTest ("Invalid lines are kept and flagged");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/invalid.txt"));
            int invalid = 0;
            for (auto& l : doc.lines())
                invalid += l.kind == ScheduleLine::Kind::invalid ? 1 : 0;
            expectEquals (invalid, 3); // 25:00, text, 12:00x
        }

        beginTest ("Editing one block rewrites only that line, in the file's style");
        {
            auto original = fixture ("schedule/planner-map.txt");
            auto doc = ScheduleDocument::fromBytes (original);
            auto idx = doc.findBlock (*TimeOfDay::parsePrefix ("00:00").time); // empty block
            auto& line = doc.lines()[(size_t) idx];
            line.block.items.push_back (ScheduleItem::makeCodeAndFile ("NEW1", "novo.mp3"));
            doc.markDirty (idx);
            expectEquals (line.raw, juce::String ("00:00 (DUR=300) \"NEW1|novo.mp3\", "));

            auto before = decodeText (original).text;
            auto after = doc.toString();
            expectEquals (after, before.replaceFirstOccurrenceOf ("00:00 (DUR=300) \r\n", "00:00 (DUR=300) \"NEW1|novo.mp3\", \r\n"));
        }

        beginTest ("Editing keeps a line's own separator and trailing style");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/manual-map.txt"));
            auto idx = doc.findBlock (*TimeOfDay::parsePrefix ("06:30").time);
            auto& line = doc.lines()[(size_t) idx];
            line.block.items.erase (line.block.items.begin());
            doc.markDirty (idx);
            expectEquals (line.raw, juce::String ("06:30 23,62"));
        }

        beginTest ("Parameters are rebuilt, unknown ones kept");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/relogio.txt"));
            auto idx = doc.findBlock (*TimeOfDay::parsePrefix ("11:00").time);
            auto& line = doc.lines()[(size_t) idx];
            line.block.params.setFlag ("LOCKED", true);
            line.block.params.setValue ("ID", "Jornal");
            doc.markDirty (idx);
            expectEquals (line.raw, juce::String ("11:00 (XYZ=1, LOCKED, ID=Jornal)"));
        }

        beginTest ("Inserting a block keeps order and the missing final newline");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/manual-map.txt"));
            ScheduleBlock b;
            b.time = *TimeOfDay::fromMinutes (7 * 60 + 20);
            b.items.push_back (ScheduleItem::makeCode ("VH"));
            doc.insertBlock (b);
            ScheduleBlock last;
            last.time = *TimeOfDay::fromMinutes (23 * 60);
            doc.insertBlock (last);
            auto text = doc.toString();
            expect (text.contains ("07:15\r\n07:20 VH\r\n07:30 \r\n"), text);
            expect (text.endsWith ("08:00 MUS1-R, VHP, 2\r\n23:00"), text);
        }

        beginTest ("Non-canonical time is normalised only when the line is edited");
        {
            auto doc = ScheduleDocument::fromBytes (fixture ("schedule/manual-map.txt"));
            auto idx = doc.findBlock (*TimeOfDay::parsePrefix ("06:15").time);
            doc.markDirty (idx);
            expect (doc.lines()[(size_t) idx].raw.startsWith ("06:15 VH, 45"));
        }

        beginTest ("Item constructors sanitise input");
        {
            expect (ScheduleItem::makeCode (" vh, ").raw == "vh");
            expect (ScheduleItem::makeFile ("a \"b\".mp3").raw == "\"a b.mp3\"");
            expect (ScheduleItem::makeCommand ("<IANEWS>").raw == "<IANEWS>");
        }
    }
};

static ScheduleTests scheduleTests;

} // namespace pc::test
