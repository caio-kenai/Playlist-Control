#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

enum class Severity
{
    info,
    warning,
    error
};

juce::String toDisplayString (Severity severity);

// A problem found while reading or validating a file. Every diagnostic must
// tell the operator what is wrong, where, why it matters and what to do.
struct Diagnostic
{
    Severity severity = Severity::error;
    juce::String code;      // stable identifier, e.g. "schedule.order"
    juce::String message;   // what is wrong
    juce::String reason;    // why it matters for the Playlist
    juce::String fix;       // what the operator can do
    juce::File file;
    int line = 0;           // 1-based, 0 when not tied to a line
    juce::String excerpt;   // offending text, when useful

    juce::String locationText() const;
};

class DiagnosticList
{
public:
    void add (Diagnostic d);
    void addAll (const DiagnosticList& other);

    const std::vector<Diagnostic>& items() const noexcept { return items_; }
    int count (Severity severity) const noexcept;
    bool hasErrors() const noexcept { return count (Severity::error) > 0; }
    bool empty() const noexcept { return items_.empty(); }
    size_t size() const noexcept { return items_.size(); }

    // Human-readable report used by logs and by the save-refused dialog.
    juce::String toText (int maxItems = 20) const;

private:
    std::vector<Diagnostic> items_;
};

} // namespace pc
