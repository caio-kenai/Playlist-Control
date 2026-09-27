#include "core/TimeOfDay.h"

namespace pc
{

std::optional<TimeOfDay> TimeOfDay::fromMinutes (int minutes)
{
    if (minutes < 0 || minutes >= 24 * 60)
        return std::nullopt;
    return TimeOfDay (minutes);
}

TimeOfDay::ParseResult TimeOfDay::parsePrefix (const juce::String& text)
{
    ParseResult r;
    int i = 0;
    const int n = text.length();
    int hourDigits = 0, h = 0;
    while (i < n && hourDigits < 2 && juce::CharacterFunctions::isDigit (text[i]))
    {
        h = h * 10 + (text[i] - '0');
        ++i;
        ++hourDigits;
    }
    if (hourDigits == 0 || i >= n || text[i] != ':')
        return r;
    ++i;
    if (i + 2 > n)
        return r;
    if (! juce::CharacterFunctions::isDigit (text[i]) || ! juce::CharacterFunctions::isDigit (text[i + 1]))
        return r;
    int m = (text[i] - '0') * 10 + (text[i + 1] - '0');
    i += 2;
    // "12:345" is not a time.
    if (i < n && juce::CharacterFunctions::isDigit (text[i]))
        return r;
    if (h > 23 || m > 59)
        return r;

    r.time = TimeOfDay (h * 60 + m);
    r.canonical = hourDigits == 2;
    r.length = i;
    return r;
}

juce::String TimeOfDay::toString() const
{
    return juce::String::formatted ("%02d:%02d", hour(), minute());
}

Date Date::today()
{
    return fromJuce (juce::Time::getCurrentTime());
}

Date Date::fromJuce (const juce::Time& t)
{
    return { t.getYear(), t.getMonth() + 1, t.getDayOfMonth() };
}

juce::Time Date::toJuceMidnight() const
{
    return juce::Time (year, month - 1, day, 0, 0, 0, 0, true);
}

Date Date::addDays (int days) const
{
    // Noon avoids daylight-saving edges when adding whole days.
    auto t = juce::Time (year, month - 1, day, 12, 0, 0, 0, true) + juce::RelativeTime::days (days);
    return fromJuce (t);
}

int Date::dayOfWeek() const
{
    return juce::Time (year, month - 1, day, 12, 0, 0, 0, true).getDayOfWeek();
}

juce::String Date::toString() const
{
    return juce::String::formatted ("%02d/%02d/%04d", day, month, year);
}

} // namespace pc
