#pragma once

#include <juce_core/juce_core.h>
#include <functional>

namespace pc
{

// Structured log: one JSON object per line, one file per day. Never receives
// file contents or configuration values that may hold passwords.
class Logger
{
public:
    enum class Level
    {
        debug,
        info,
        warning,
        error
    };

    struct Record
    {
        juce::Time time;
        Level level = Level::info;
        juce::String event;
        juce::String message;
        juce::NamedValueSet fields;
    };

    static Logger& instance();

    void open (const juce::File& directory, int keepDays = 60);
    void close();
    juce::File directory() const;
    juce::File currentFile() const;

    void log (Level level, const juce::String& event, const juce::String& message,
              const juce::NamedValueSet& fields = {});

    void debug (const juce::String& e, const juce::String& m, const juce::NamedValueSet& f = {}) { log (Level::debug, e, m, f); }
    void info (const juce::String& e, const juce::String& m, const juce::NamedValueSet& f = {}) { log (Level::info, e, m, f); }
    void warning (const juce::String& e, const juce::String& m, const juce::NamedValueSet& f = {}) { log (Level::warning, e, m, f); }
    void error (const juce::String& e, const juce::String& m, const juce::NamedValueSet& f = {}) { log (Level::error, e, m, f); }

    // Called on the logging thread; receivers must hand work to the message thread.
    void setListener (std::function<void (const Record&)> listener);

    static juce::String toString (Level level);
    static juce::String formatLine (const Record& r);

private:
    Logger() = default;
    void pruneOld (int keepDays);

    mutable juce::CriticalSection lock_;
    juce::File directory_;
    std::unique_ptr<juce::FileOutputStream> stream_;
    juce::String streamDate_;
    std::function<void (const Record&)> listener_;
};

} // namespace pc
