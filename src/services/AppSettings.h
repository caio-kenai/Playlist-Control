#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace pc
{

// Settings of PlaylistControl itself (%APPDATA%\PlaylistControl).
class AppSettings
{
public:
    AppSettings();

    juce::File pgmFolder() const;
    void setPgmFolder (const juce::File& folder);

    // The operator confirmed that this folder is the installation to control.
    bool installationConfirmed() const;
    void setInstallationConfirmed (bool confirmed);

    bool readOnly() const;
    void setReadOnly (bool readOnly);

    int historyKeepDays() const;

    static juce::File dataFolder();    // %LOCALAPPDATA%\PlaylistControl
    static juce::File historyFolder();
    static juce::File logFolder();

private:
    std::unique_ptr<juce::PropertiesFile> props_;
};

} // namespace pc
