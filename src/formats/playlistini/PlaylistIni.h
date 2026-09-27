#pragma once

#include "core/TimeOfDay.h"
#include "formats/ini/IniDocument.h"

namespace pc
{

// Kinds of schedule files the Playlist reads (manual: "Configurando leitura
// dos Mapas – Playlist.ini" and "Relógio Operacional").
enum class ScheduleKind
{
    commercial,     // [BLOCO COMERCIAL] -> mapas
    musical,        // [BLOCO MUSICAL]   -> grades
    commercialClock,// [RELOGIO COMERCIAL]
    musicalClock    // [RELOGIO MUSICAL]
};

juce::String sectionName (ScheduleKind kind);
juce::String toDisplayString (ScheduleKind kind);

enum class ScheduleFormat
{
    missing,  // section or FORMATO absent
    automatic,// FORMATO=AUTO
    txt1,     // FORMATO=TXT1
    other     // any other value, kept as written
};

struct ScheduleSource
{
    ScheduleKind kind = ScheduleKind::commercial;
    bool sectionPresent = false;
    ScheduleFormat format = ScheduleFormat::missing;
    juce::String formatRaw;
    juce::String pattern; // ARQUIVO=
};

struct Affiliate
{
    juce::String id;
    juce::String address; // host:port
};

struct BeepConfig
{
    bool present = false;
    juce::String file;
    juce::String minutesRaw;
    juce::Array<int> minutes;
    bool minutesValid = true;
};

// Typed view over PLAYLIST.ini. Everything it does not model stays in the
// underlying document untouched.
class PlaylistIni
{
public:
    PlaylistIni() = default;
    explicit PlaylistIni (IniDocument doc) : doc_ (std::move (doc)) {}

    IniDocument& document() noexcept { return doc_; }
    const IniDocument& document() const noexcept { return doc_; }

    ScheduleSource source (ScheduleKind kind) const;
    void setSource (ScheduleKind kind, ScheduleFormat format, const juce::String& pattern);
    void removeSource (ScheduleKind kind);

    std::vector<Affiliate> affiliates() const;
    void setAffiliates (const std::vector<Affiliate>& affiliates);

    BeepConfig beep() const;
    void setBeep (const juce::String& file, const juce::String& minutes);
    void removeBeep();

    // Sections the typed view knows about.
    static bool isKnownSection (const juce::String& name);

    static juce::String formatToIniValue (ScheduleFormat format);
    static ScheduleFormat parseFormat (const juce::String& value);

private:
    IniDocument doc_;
};

} // namespace pc
