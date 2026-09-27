#include "formats/playlistini/PlaylistIni.h"

namespace pc
{

namespace
{
const char* const beepSection = "BEEP";
const char* const affiliatesSection = "AFILIADAS";
} // namespace

juce::String sectionName (ScheduleKind kind)
{
    switch (kind)
    {
        case ScheduleKind::commercial:      return "BLOCO COMERCIAL";
        case ScheduleKind::musical:         return "BLOCO MUSICAL";
        case ScheduleKind::commercialClock: return "RELOGIO COMERCIAL";
        case ScheduleKind::musicalClock:    return "RELOGIO MUSICAL";
    }
    return {};
}

juce::String toDisplayString (ScheduleKind kind)
{
    switch (kind)
    {
        case ScheduleKind::commercial:      return "Mapa comercial";
        case ScheduleKind::musical:         return "Grade musical";
        case ScheduleKind::commercialClock: return L"Relógio comercial";
        case ScheduleKind::musicalClock:    return L"Relógio musical";
    }
    return {};
}

juce::String PlaylistIni::formatToIniValue (ScheduleFormat format)
{
    switch (format)
    {
        case ScheduleFormat::automatic: return "AUTO";
        case ScheduleFormat::txt1:      return "TXT1";
        case ScheduleFormat::missing:
        case ScheduleFormat::other:     break;
    }
    return {};
}

ScheduleFormat PlaylistIni::parseFormat (const juce::String& value)
{
    auto v = value.trim();
    if (v.isEmpty())                   return ScheduleFormat::missing;
    if (v.equalsIgnoreCase ("AUTO"))   return ScheduleFormat::automatic;
    if (v.equalsIgnoreCase ("TXT1"))   return ScheduleFormat::txt1;
    return ScheduleFormat::other;
}

bool PlaylistIni::isKnownSection (const juce::String& name)
{
    for (auto k : { ScheduleKind::commercial, ScheduleKind::musical, ScheduleKind::commercialClock, ScheduleKind::musicalClock })
        if (name.equalsIgnoreCase (sectionName (k)))
            return true;
    return name.equalsIgnoreCase (beepSection) || name.equalsIgnoreCase (affiliatesSection);
}

ScheduleSource PlaylistIni::source (ScheduleKind kind) const
{
    ScheduleSource s;
    s.kind = kind;
    auto section = sectionName (kind);
    s.sectionPresent = doc_.hasSection (section);
    s.formatRaw = doc_.getOr (section, "FORMATO");
    s.format = parseFormat (s.formatRaw);
    s.pattern = doc_.getOr (section, "ARQUIVO");
    return s;
}

void PlaylistIni::setSource (ScheduleKind kind, ScheduleFormat format, const juce::String& pattern)
{
    auto section = sectionName (kind);
    if (format == ScheduleFormat::automatic || format == ScheduleFormat::txt1)
        doc_.set (section, "FORMATO", formatToIniValue (format));
    if (pattern.trim().isNotEmpty())
        doc_.set (section, "ARQUIVO", pattern.trim());
    else
        doc_.remove (section, "ARQUIVO");
}

void PlaylistIni::removeSource (ScheduleKind kind)
{
    doc_.removeSection (sectionName (kind));
}

std::vector<Affiliate> PlaylistIni::affiliates() const
{
    std::vector<Affiliate> out;
    for (auto& [k, v] : doc_.entries (affiliatesSection))
        out.push_back ({ k, v });
    return out;
}

void PlaylistIni::setAffiliates (const std::vector<Affiliate>& list)
{
    if (list.empty())
    {
        doc_.removeSection (affiliatesSection);
        return;
    }
    for (auto& existing : affiliates())
    {
        bool keep = false;
        for (auto& a : list)
            keep = keep || a.id.equalsIgnoreCase (existing.id);
        if (! keep)
            doc_.remove (affiliatesSection, existing.id);
    }
    for (auto& a : list)
        doc_.set (affiliatesSection, a.id.trim(), a.address.trim());
}

BeepConfig PlaylistIni::beep() const
{
    BeepConfig b;
    b.present = doc_.hasSection (beepSection);
    b.file = doc_.getOr (beepSection, "ARQUIVO");
    b.minutesRaw = doc_.getOr (beepSection, "HORARIO");
    for (auto& token : juce::StringArray::fromTokens (b.minutesRaw, ",", {}))
    {
        auto t = token.trim();
        if (t.isEmpty())
            continue;
        if (! t.containsOnly ("0123456789") || t.getIntValue() > 59)
        {
            b.minutesValid = false;
            continue;
        }
        b.minutes.add (t.getIntValue());
    }
    return b;
}

void PlaylistIni::setBeep (const juce::String& file, const juce::String& minutes)
{
    doc_.set (beepSection, "ARQUIVO", file.trim());
    doc_.set (beepSection, "HORARIO", minutes.trim());
}

void PlaylistIni::removeBeep()
{
    doc_.removeSection (beepSection);
}

} // namespace pc
