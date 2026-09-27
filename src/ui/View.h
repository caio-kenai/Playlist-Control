#pragma once

#include "services/AppSettings.h"
#include "services/Workspace.h"
#include "ui/Widgets.h"

namespace pc::ui
{

enum class ViewId
{
    dashboard,
    maps,
    grades,
    clocks,
    playlistIni,
    config,
    configManager,
    folders,
    operators,
    diagnostics,
    indexes,
    history
};

struct AppContext
{
    Workspace& workspace;
    AppSettings& settings;
    std::function<void (const juce::String&)> status;
    std::function<void (ViewId, const juce::File&, int line)> navigate;
};

class View : public juce::Component
{
public:
    explicit View (AppContext& context) : ctx (context) {}

    virtual juce::String title() const = 0;
    virtual juce::String subtitle() const { return {}; }

    // The workspace changed (reload, external change, mode switch).
    virtual void refresh() {}

    virtual bool hasUnsavedChanges() const { return false; }
    virtual void save() {}
    virtual void discard() {}

    // Opens a file (and line) inside the view, when it supports it.
    virtual void openFile (const juce::File&, int /*line*/) {}

protected:
    AppContext& ctx;
};

} // namespace pc::ui
