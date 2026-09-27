#pragma once

#include "formats/playlistini/PlaylistIni.h"

namespace pc
{

// The manual contradicts itself on %w: the text says Sunday = 0, the table
// lists 1..7 with Sunday = 7. Both are offered until confirmed.
enum class WeekdayNumbering
{
    sundayZero,
    sundaySeven
};

// "Dom", "Seg", ..., "Sáb" — the names the Playlist expects (with accent).
juce::String weekdayAbbreviation (int dayOfWeekSundayZero);

// Expands %d %m %Y %y %a %w. Unknown variables are kept as written.
juce::String expandPattern (const juce::String& pattern, const Date& date,
                            WeekdayNumbering numbering = WeekdayNumbering::sundayZero);

// Variables in the pattern that the Playlist does not document (e.g. "%H").
juce::StringArray unknownPatternVariables (const juce::String& pattern);

struct ScheduleCandidate
{
    juce::File file;
    juce::String rule;   // which rule of the search produced this name
    bool exists = false;
    bool uncertain = false; // depends on an unconfirmed rule
};

// Files the Playlist looks for, in order, for a given day.
std::vector<ScheduleCandidate> scheduleCandidates (const ScheduleSource& source, const Date& date,
                                                   const juce::File& pgmFolder);

// First existing candidate, if any.
std::optional<ScheduleCandidate> resolveScheduleFile (const ScheduleSource& source, const Date& date,
                                                      const juce::File& pgmFolder);

} // namespace pc
