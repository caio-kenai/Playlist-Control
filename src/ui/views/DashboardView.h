#pragma once

#include "ui/View.h"

namespace pc::ui
{

// Overview: installation, programs, what the Playlist reads today and in the
// next days, problems and recent activity.
class DashboardView : public View
{
public:
    explicit DashboardView (AppContext& context);

    juce::String title() const override { return "Painel"; }
    juce::String subtitle() const override;
    void refresh() override;
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    struct TodayRow
    {
        ScheduleKind kind;
        juce::String label;
        juce::File file;
        juce::String rule;
        FileOrigin origin = FileOrigin::unknown;
        bool rewritten = false;
        int blocks = 0, errors = 0, warnings = 0;
        juce::Rectangle<int> area;
    };
    struct DayColumn
    {
        Date date;
        bool map = false, grade = false;
        int errors = 0;
    };

    void paintInstallation (juce::Graphics& g, juce::Rectangle<int> r);
    void paintPrograms (juce::Graphics& g, juce::Rectangle<int> r);
    void paintSummary (juce::Graphics& g, juce::Rectangle<int> r);
    void paintToday (juce::Graphics& g, juce::Rectangle<int> r);
    void paintWeek (juce::Graphics& g, juce::Rectangle<int> r);
    void paintAlerts (juce::Graphics& g, juce::Rectangle<int> r);
    void paintActivity (juce::Graphics& g, juce::Rectangle<int> r);
    static void paintCard (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent);

    std::vector<TodayRow> today_;
    std::vector<DayColumn> week_;
    DiagnosticList diagnostics_;
    std::vector<std::pair<juce::Rectangle<int>, Diagnostic>> alertAreas_;
    juce::Rectangle<int> installationArea_, programsArea_, summaryArea_, todayArea_, weekArea_, alertsArea_, activityArea_;
    juce::TextButton diagnosticsButton_ { L"Ver diagnóstico completo" };
    int hoverRow_ = -1;
};

} // namespace pc::ui
