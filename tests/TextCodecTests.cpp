#include "TestUtils.h"
#include "core/TextCodec.h"
#include "core/TimeOfDay.h"

namespace pc::test
{

class TextCodecTests : public juce::UnitTest
{
public:
    TextCodecTests() : juce::UnitTest ("TextCodec", "core") {}

    void expectRoundTrip (const juce::MemoryBlock& original, TextEncoding expected)
    {
        auto lines = TextLines::fromBytes (original);
        expect (lines.encoding == expected, "encoding " + toDisplayString (lines.encoding));
        juce::MemoryBlock back;
        expect (lines.toBytes (back));
        expect (back == original, "bytes changed after round trip");
    }

    void runTest() override
    {
        beginTest ("ASCII with CRLF and no final newline");
        expectRoundTrip (bytes ("00:00 VH, 55\r\n00:15 \r\n00:30"), TextEncoding::ascii);

        beginTest ("Mixed terminators are kept per line");
        {
            auto original = bytes ("a\r\nb\nc\rd\r\n");
            auto lines = TextLines::fromBytes (original);
            expectEquals ((int) lines.lines.size(), 4);
            expect (lines.lines[1].eol == "\n");
            expect (lines.lines[2].eol == "\r");
            expectRoundTrip (original, TextEncoding::ascii);
        }

        beginTest ("Windows-1252 accents");
        // "Cabeça" in Windows-1252
        expectRoundTrip (bytes ({ 'C', 'a', 'b', 'e', 0xE7, 'a', '\r', '\n' }), TextEncoding::windows1252);
        {
            auto d = decodeText (bytes ({ 'C', 'a', 'b', 'e', 0xE7, 'a' }));
            expect (d.text == juce::String (juce::CharPointer_UTF8 ("Cabe\xc3\xa7" "a")));
        }

        beginTest ("Windows-1252 undefined bytes survive");
        expectRoundTrip (bytes ({ 0x81, 0x8D, 0x8F, 0x90, 0x9D, 0x80, 0x99 }), TextEncoding::windows1252);

        beginTest ("UTF-8 without BOM");
        expectRoundTrip (bytes ("00:00 #C\xc3\x93" "DIGOEX1,\r\n"), TextEncoding::utf8);

        beginTest ("UTF-8 with BOM");
        expectRoundTrip (bytes ("\xef\xbb\xbf<?xml version=\"1.0\"?>\r\n<Folders/>"), TextEncoding::utf8Bom);

        beginTest ("Invalid UTF-8 falls back to Windows-1252");
        expectRoundTrip (bytes ({ 'a', 0xC3, 0x28, 'b' }), TextEncoding::windows1252);

        beginTest ("ASCII file receiving an accent is written as Windows-1252");
        {
            juce::MemoryBlock out;
            expect (encodeText (juce::String (juce::CharPointer_UTF8 ("S\xc3\xa1" "b")), TextEncoding::ascii, out));
            expect (out == bytes ({ 'S', 0xE1, 'b' }));
        }

        beginTest ("Unrepresentable characters are reported");
        {
            juce::MemoryBlock out;
            juce::juce_wchar bad = 0;
            expect (! encodeText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93")), TextEncoding::windows1252, out, &bad));
            expect (bad == 0x2713);
        }

        beginTest ("Dominant line ending");
        expect (TextLines::fromBytes (bytes ("a\nb\nc\r\n")).dominantEol() == "\n");
        expect (TextLines::fromBytes (bytes ("abc")).dominantEol() == "\r\n");

        beginTest ("TimeOfDay parsing");
        {
            auto t = TimeOfDay::parsePrefix ("06:15 VH");
            expect (t.time.has_value() && t.time->minutes() == 375 && t.canonical && t.length == 5);
            auto s = TimeOfDay::parsePrefix ("6:15 VH");
            expect (s.time.has_value() && ! s.canonical && s.length == 4);
            expect (! TimeOfDay::parsePrefix ("24:00").time.has_value());
            expect (! TimeOfDay::parsePrefix ("12:60").time.has_value());
            expect (! TimeOfDay::parsePrefix ("12:345").time.has_value());
            expect (! TimeOfDay::parsePrefix ("ab:cd").time.has_value());
            expect (! TimeOfDay::parsePrefix ("").time.has_value());
            expectEquals (TimeOfDay::fromMinutes (1439)->toString(), juce::String ("23:59"));
        }

        beginTest ("Date arithmetic");
        {
            Date d { 2026, 12, 31 };
            auto n = d.addDays (1);
            expect (n == Date { 2027, 1, 1 });
            expectEquals (Date { 2026, 9, 27 }.dayOfWeek(), 0); // Sunday
        }
    }
};

static TextCodecTests textCodecTests;

} // namespace pc::test
