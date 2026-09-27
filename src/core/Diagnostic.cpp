#include "core/Diagnostic.h"

namespace pc
{

juce::String toDisplayString (Severity severity)
{
    switch (severity)
    {
        case Severity::info:    return "Informação";
        case Severity::warning: return "Aviso";
        case Severity::error:   return "Erro";
    }
    return {};
}

juce::String Diagnostic::locationText() const
{
    juce::String s = file.getFileName();
    if (line > 0)
        s << ", linha " << line;
    return s;
}

void DiagnosticList::add (Diagnostic d)
{
    items_.push_back (std::move (d));
}

void DiagnosticList::addAll (const DiagnosticList& other)
{
    items_.insert (items_.end(), other.items_.begin(), other.items_.end());
}

int DiagnosticList::count (Severity severity) const noexcept
{
    int n = 0;
    for (auto& d : items_)
        if (d.severity == severity)
            ++n;
    return n;
}

juce::String DiagnosticList::toText (int maxItems) const
{
    juce::String out;
    int shown = 0;
    for (auto& d : items_)
    {
        if (shown++ >= maxItems)
        {
            out << "... e mais " << (int) items_.size() - maxItems << " item(ns).\n";
            break;
        }
        out << toDisplayString (d.severity) << " [" << d.locationText() << "] " << d.message << "\n";
        if (d.excerpt.isNotEmpty()) out << "    Trecho: " << d.excerpt << "\n";
        if (d.reason.isNotEmpty())  out << "    Por quê: " << d.reason << "\n";
        if (d.fix.isNotEmpty())     out << "    Como resolver: " << d.fix << "\n";
    }
    return out;
}

} // namespace pc
