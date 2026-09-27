#include <juce_gui_basics/juce_gui_basics.h>

#include "Version.h"

class PlaylistControlApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "PlaylistControl"; }
    const juce::String getApplicationVersion() override { return PLAYLISTCONTROL_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override {}
    void shutdown() override {}
    void systemRequestedQuit() override { quit(); }
};

START_JUCE_APPLICATION (PlaylistControlApplication)
