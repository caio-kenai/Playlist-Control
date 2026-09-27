#include "core/TextCodec.h"

namespace pc
{

namespace
{
// 0x80..0x9F. Undefined positions map to the C1 control with the same value.
constexpr juce::juce_wchar cp1252High[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

int encode1252 (juce::juce_wchar c) noexcept
{
    if (c < 0x80 || (c >= 0xA0 && c <= 0xFF))
        return (int) c;
    for (int i = 0; i < 32; ++i)
        if (cp1252High[i] == c)
            return 0x80 + i;
    return -1;
}

bool isValidUtf8 (const juce::uint8* p, size_t n) noexcept
{
    size_t i = 0;
    while (i < n)
    {
        auto c = p[i];
        int extra;
        juce::uint32 cp;
        if (c < 0x80)                { ++i; continue; }
        else if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
        else return false;

        if (i + (size_t) extra >= n)
            return false;
        for (int k = 1; k <= extra; ++k)
        {
            if ((p[i + (size_t) k] & 0xC0) != 0x80)
                return false;
            cp = (cp << 6) | (p[i + (size_t) k] & 0x3F);
        }
        // Reject overlong forms and surrogates so the round trip is exact.
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)
            || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF)
            return false;
        i += (size_t) extra + 1;
    }
    return true;
}
} // namespace

juce::String toDisplayString (TextEncoding encoding)
{
    switch (encoding)
    {
        case TextEncoding::ascii:       return "ASCII";
        case TextEncoding::utf8:        return "UTF-8";
        case TextEncoding::utf8Bom:     return "UTF-8 com BOM";
        case TextEncoding::windows1252: return "Windows-1252";
    }
    return {};
}

bool isRepresentableIn1252 (juce::juce_wchar c) noexcept
{
    return encode1252 (c) >= 0;
}

juce::String decodeWindows1252 (const void* data, size_t size)
{
    auto* p = static_cast<const juce::uint8*> (data);
    juce::String s;
    s.preallocateBytes (size + 16);
    for (size_t i = 0; i < size; ++i)
    {
        auto b = p[i];
        s += (juce::juce_wchar) (b >= 0x80 && b < 0xA0 ? cp1252High[b - 0x80] : (juce::juce_wchar) b);
    }
    return s;
}

DecodedText decodeText (const void* data, size_t size)
{
    auto* p = static_cast<const juce::uint8*> (data);
    DecodedText d;

    if (size >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
    {
        d.encoding = TextEncoding::utf8Bom;
        d.text = juce::String::fromUTF8 (reinterpret_cast<const char*> (p + 3), (int) (size - 3));
        return d;
    }

    bool ascii = true;
    for (size_t i = 0; i < size && ascii; ++i)
        ascii = p[i] < 0x80;

    if (ascii)
    {
        d.encoding = TextEncoding::ascii;
        d.text = juce::String (reinterpret_cast<const char*> (p), size);
        return d;
    }

    if (isValidUtf8 (p, size))
    {
        d.encoding = TextEncoding::utf8;
        d.text = juce::String::fromUTF8 (reinterpret_cast<const char*> (p), (int) size);
        return d;
    }

    d.encoding = TextEncoding::windows1252;
    d.text = decodeWindows1252 (p, size);
    return d;
}

DecodedText decodeText (const juce::MemoryBlock& block)
{
    return decodeText (block.getData(), block.getSize());
}

bool encodeText (const juce::String& text, TextEncoding encoding, juce::MemoryBlock& out,
                 juce::juce_wchar* firstBad)
{
    out.reset();
    switch (encoding)
    {
        case TextEncoding::utf8Bom:
        {
            const juce::uint8 bom[] = { 0xEF, 0xBB, 0xBF };
            out.append (bom, 3);
            [[fallthrough]];
        }
        case TextEncoding::utf8:
        {
            auto utf8 = text.toUTF8();
            out.append (utf8.getAddress(), utf8.sizeInBytes() - 1);
            return true;
        }
        case TextEncoding::ascii:
        case TextEncoding::windows1252:
        {
            // An ASCII file that receives an accented character becomes
            // Windows-1252, the encoding the Playlist tools write.
            bool ok = true;
            juce::MemoryOutputStream mo (out, false);
            for (auto t = text.getCharPointer(); ! t.isEmpty();)
            {
                auto c = t.getAndAdvance();
                auto b = encode1252 (c);
                if (b < 0)
                {
                    if (ok && firstBad != nullptr) *firstBad = c;
                    ok = false;
                    b = '?';
                }
                mo.writeByte ((char) b);
            }
            mo.flush();
            return ok;
        }
    }
    return false;
}

TextLines TextLines::split (const DecodedText& decoded)
{
    TextLines t;
    t.encoding = decoded.encoding;

    auto s = decoded.text.getCharPointer();
    juce::String current;
    while (! s.isEmpty())
    {
        auto c = s.getAndAdvance();
        if (c == '\r')
        {
            if (*s == '\n')
            {
                ++s;
                t.lines.push_back ({ current, "\r\n" });
            }
            else
            {
                t.lines.push_back ({ current, "\r" });
            }
            current.clear();
        }
        else if (c == '\n')
        {
            t.lines.push_back ({ current, "\n" });
            current.clear();
        }
        else
        {
            current += c;
        }
    }
    if (current.isNotEmpty())
        t.lines.push_back ({ current, {} });
    return t;
}

TextLines TextLines::fromBytes (const juce::MemoryBlock& block)
{
    return split (decodeText (block));
}

juce::String TextLines::join() const
{
    juce::String s;
    for (auto& l : lines)
        s << l.content << l.eol;
    return s;
}

bool TextLines::toBytes (juce::MemoryBlock& out, juce::juce_wchar* firstBad) const
{
    return encodeText (join(), encoding, out, firstBad);
}

juce::String TextLines::dominantEol() const
{
    int crlf = 0, lf = 0, cr = 0;
    for (auto& l : lines)
    {
        if (l.eol == "\r\n") ++crlf;
        else if (l.eol == "\n") ++lf;
        else if (l.eol == "\r") ++cr;
    }
    if (lf > crlf && lf >= cr) return "\n";
    if (cr > crlf && cr > lf) return "\r";
    return "\r\n";
}

} // namespace pc
