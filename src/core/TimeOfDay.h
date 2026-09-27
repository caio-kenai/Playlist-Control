#pragma once

#include <juce_core/juce_core.h>
#include <optional>

namespace pc
{

// Block time "HH:MM" as used by maps, grades and clocks.
class TimeOfDay
{
public:
    TimeOfDay() = default;
    static std::optional<TimeOfDay> fromMinutes (int minutes);

    struct ParseResult;

    // Accepts "H:MM" and "HH:MM" at the start of the text.
    static ParseResult parsePrefix (const juce::String& text);

    int minutes() const noexcept { return minutes_; }
    int hour() const noexcept { return minutes_ / 60; }
    int minute() const noexcept { return minutes_ % 60; }
    juce::String toString() const; // "HH:MM"

    bool operator== (const TimeOfDay& o) const noexcept { return minutes_ == o.minutes_; }
    bool operator!= (const TimeOfDay& o) const noexcept { return minutes_ != o.minutes_; }
    bool operator< (const TimeOfDay& o) const noexcept { return minutes_ < o.minutes_; }

private:
    explicit TimeOfDay (int m) : minutes_ (m) {}
    int minutes_ = 0;
};

struct TimeOfDay::ParseResult
{
    std::optional<TimeOfDay> time;
    bool canonical = false; // exactly "HH:MM" with two digits each
    int length = 0;         // characters consumed
};

// Calendar date without time zone, used to resolve daily file names.
struct Date
{
    int year = 2000, month = 1, day = 1;

    static Date today();
    static Date fromJuce (const juce::Time& t);
    juce::Time toJuceMidnight() const;
    Date addDays (int days) const;
    int dayOfWeek() const; // 0 = Sunday
    bool operator== (const Date& o) const noexcept { return year == o.year && month == o.month && day == o.day; }
    juce::String toString() const; // dd/mm/yyyy
};

} // namespace pc
