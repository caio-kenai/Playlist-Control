#include "logging/Logger.h"

namespace pc
{

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

juce::String Logger::toString (Level level)
{
    switch (level)
    {
        case Level::debug:   return "debug";
        case Level::info:    return "info";
        case Level::warning: return "warning";
        case Level::error:   return "error";
    }
    return {};
}

void Logger::open (const juce::File& directory, int keepDays)
{
    const juce::ScopedLock sl (lock_);
    directory_ = directory;
    directory_.createDirectory();
    stream_.reset();
    streamDate_.clear();
    pruneOld (keepDays);
}

void Logger::close()
{
    const juce::ScopedLock sl (lock_);
    if (stream_ != nullptr)
        stream_->flush();
    stream_.reset();
}

juce::File Logger::directory() const
{
    const juce::ScopedLock sl (lock_);
    return directory_;
}

juce::File Logger::currentFile() const
{
    const juce::ScopedLock sl (lock_);
    if (directory_ == juce::File())
        return {};
    return directory_.getChildFile ("PlaylistControl-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d") + ".jsonl");
}

void Logger::setListener (std::function<void (const Record&)> listener)
{
    const juce::ScopedLock sl (lock_);
    listener_ = std::move (listener);
}

juce::String Logger::formatLine (const Record& r)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("time", r.time.toISO8601 (true));
    obj->setProperty ("level", toString (r.level));
    obj->setProperty ("event", r.event);
    obj->setProperty ("message", r.message);
    for (auto& f : r.fields)
        obj->setProperty (f.name, f.value);
    return juce::JSON::toString (juce::var (obj), juce::JSON::FormatOptions{}.withSpacing (juce::JSON::Spacing::none));
}

void Logger::log (Level level, const juce::String& event, const juce::String& message,
                  const juce::NamedValueSet& fields)
{
    Record r { juce::Time::getCurrentTime(), level, event, message, fields };
    std::function<void (const Record&)> listener;
    {
        const juce::ScopedLock sl (lock_);
        if (directory_ != juce::File())
        {
            auto date = r.time.formatted ("%Y-%m-%d");
            if (stream_ == nullptr || date != streamDate_)
            {
                stream_ = std::make_unique<juce::FileOutputStream> (
                    directory_.getChildFile ("PlaylistControl-" + date + ".jsonl"));
                streamDate_ = date;
                if (stream_->failedToOpen())
                    stream_.reset();
            }
            if (stream_ != nullptr)
            {
                stream_->writeText (formatLine (r) + "\n", false, false, nullptr);
                stream_->flush();
            }
        }
        listener = listener_;
    }
    if (listener)
        listener (r);
}

void Logger::pruneOld (int keepDays)
{
    auto limit = juce::Time::getCurrentTime() - juce::RelativeTime::days (keepDays);
    for (auto& f : directory_.findChildFiles (juce::File::findFiles, false, "PlaylistControl-*.jsonl"))
        if (f.getLastModificationTime() < limit)
            f.deleteFile();
}

} // namespace pc
