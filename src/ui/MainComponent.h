#pragma once

#include "ui/View.h"

namespace pc::ui
{

// Frame of the application: header with installation and program status,
// navigation on the left, the current view, read-only banner and status bar.
class MainComponent : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent (Workspace& workspace, AppSettings& settings);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

    void showView (ViewId id, const juce::File& file = {}, int line = 0);
    ViewId currentView() const noexcept { return current_; }
    View* view() const { return view_.get(); }

    // Asks before leaving a view with unsaved changes.
    void guardUnsaved (std::function<void()> proceed);

    void chooseInstallation();

private:
    struct NavItem
    {
        ViewId id;
        juce::String label;
        juce::String group;
        juce::Rectangle<int> area;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void setStatus (const juce::String& text);
    void updateBanner();
    std::unique_ptr<View> createView (ViewId id);
    int problemCount (ViewId id) const;
    void paintHeader (juce::Graphics& g, juce::Rectangle<int> r);
    void paintSidebar (juce::Graphics& g, juce::Rectangle<int> r);

    Workspace& workspace_;
    AppSettings& settings_;
    AppContext context_;
    std::vector<NavItem> nav_;
    ViewId current_ = ViewId::dashboard;
    std::unique_ptr<View> view_;
    Banner banner_;
    juce::TextButton modeButton_, installButton_, reloadButton_;
    juce::String status_;
    juce::Time statusTime_;
    int hoverNav_ = -1;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::TooltipWindow tooltips_ { this, 600 };
};

} // namespace pc::ui
