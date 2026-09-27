#include "formats/playlistini/FilePatternResolver.h"

namespace pc
{

juce::String weekdayAbbreviation (int d)
{
    static const char* const names[] = { "Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "S\xc3\xa1" "b" };
    return juce::String (juce::CharPointer_UTF8 (names[juce::jlimit (0, 6, d)]));
}

juce::String expandPattern (const juce::String& pattern, const Date& date, WeekdayNumbering numbering)
{
    juce::String out;
    for (int i = 0; i < pattern.length(); ++i)
    {
        auto c = pattern[i];
        if (c != '%' || i + 1 >= pattern.length())
        {
            out += c;
            continue;
        }
        auto v = pattern[i + 1];
        auto dow = date.dayOfWeek();
        switch (v)
        {
            case 'd': out << juce::String::formatted ("%02d", date.day); break;
            case 'm': out << juce::String::formatted ("%02d", date.month); break;
            case 'Y': out << juce::String (date.year); break;
            case 'y': out << juce::String::formatted ("%02d", date.year % 100); break;
            case 'a': out << weekdayAbbreviation (dow); break;
            case 'w': out << juce::String (numbering == WeekdayNumbering::sundaySeven && dow == 0 ? 7 : dow); break;
            default:  out << c << v; break;
        }
        ++i;
    }
    return out;
}

juce::StringArray unknownPatternVariables (const juce::String& pattern)
{
    juce::StringArray bad;
    for (int i = 0; i + 1 < pattern.length(); ++i)
    {
        if (pattern[i] != '%')
            continue;
        auto v = pattern[i + 1];
        if (juce::String ("dmYyaw").indexOfChar (v) < 0)
            bad.addIfNotAlreadyThere (pattern.substring (i, i + 2));
        ++i;
    }
    return bad;
}

namespace
{
juce::File resolveRelative (const juce::File& pgm, const juce::String& relative)
{
    auto normalized = relative.replaceCharacter ('/', '\\');
    if (juce::File::isAbsolutePath (normalized))
        return juce::File (normalized);
    return pgm.getChildFile (normalized);
}

void add (std::vector<ScheduleCandidate>& list, const juce::File& pgm, const juce::String& relative,
          const juce::String& rule, bool uncertain = false)
{
    auto f = resolveRelative (pgm, relative);
    for (auto& c : list)
        if (c.file == f)
            return;
    list.push_back ({ f, rule, f.existsAsFile(), uncertain });
}
} // namespace

std::vector<ScheduleCandidate> scheduleCandidates (const ScheduleSource& source, const Date& date, const juce::File& pgm)
{
    std::vector<ScheduleCandidate> list;
    auto dd = juce::String::formatted ("%02d", date.day);
    auto full = expandPattern ("%d-%m-%Y", date);
    auto dow = date.dayOfWeek();
    auto abbrev = weekdayAbbreviation (dow);

    if (source.format == ScheduleFormat::txt1 || (source.format == ScheduleFormat::other && source.pattern.isNotEmpty()))
    {
        if (source.pattern.isNotEmpty())
        {
            add (list, pgm, expandPattern (source.pattern, date), "ARQUIVO=" + source.pattern);
            if (source.pattern.contains ("%w") && dow == 0)
                add (list, pgm, expandPattern (source.pattern, date, WeekdayNumbering::sundaySeven),
                     "ARQUIVO=" + source.pattern + " (domingo = 7)", true);
        }
        return list;
    }

    if (source.format != ScheduleFormat::automatic)
    {
        // Without a [BLOCO ...] section the manual describes the AUTO search
        // as the installed default; clocks have no default.
        if (source.kind == ScheduleKind::commercialClock || source.kind == ScheduleKind::musicalClock)
            return list;
    }

    // A missing FORMATO is assumed to behave like AUTO, which is not confirmed.
    const bool assumed = source.format != ScheduleFormat::automatic;
    switch (source.kind)
    {
        case ScheduleKind::commercial:
            add (list, pgm, "Mapas\\Mapa" + full + ".txt", "AUTO: data completa", assumed);
            add (list, pgm, "Mapas\\Mapa" + dd + ".txt", "AUTO: dia do mês", assumed);
            add (list, pgm, "Mapas\\" + abbrev + ".txt", "AUTO: dia da semana", assumed);
            add (list, pgm, "Mapas\\" + juce::String (dow) + ".txt", "AUTO: número do dia da semana", assumed || dow == 0);
            if (dow == 0)
                add (list, pgm, "Mapas\\7.txt", "AUTO: número do dia da semana (domingo = 7)", true);
            add (list, pgm, "Mapas\\Mapa.txt", "AUTO: mapa padrão", assumed);
            break;
        case ScheduleKind::musical:
            add (list, pgm, "Grades\\" + full + ".txt", "AUTO: data completa", assumed);
            add (list, pgm, "Grades\\Grade" + dd + ".txt", "AUTO: dia do mês", assumed);
            add (list, pgm, "Grades\\" + abbrev + ".txt", "AUTO: dia da semana", assumed);
            add (list, pgm, "Grades\\Grade.txt", "AUTO: grade padrão", assumed);
            add (list, pgm, "Mapas\\Grade.txt", "AUTO: grade padrão na pasta Mapas", assumed);
            break;
        case ScheduleKind::commercialClock:
        case ScheduleKind::musicalClock:
            // FORMATO=AUTO for clocks is not documented.
            break;
    }
    return list;
}

std::optional<ScheduleCandidate> resolveScheduleFile (const ScheduleSource& source, const Date& date, const juce::File& pgm)
{
    for (auto& c : scheduleCandidates (source, date, pgm))
        if (c.exists)
            return c;
    return std::nullopt;
}

} // namespace pc
