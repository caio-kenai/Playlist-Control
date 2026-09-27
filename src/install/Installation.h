#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

struct InstallationCheck
{
    juce::String item;
    bool ok = false;
    bool required = false;
    juce::String detail;
};

// Result of inspecting a folder that may be a Playlist "pgm" folder. Nothing
// is created or changed while inspecting.
struct InstallationInfo
{
    juce::File pgm;
    bool valid = false;
    std::vector<InstallationCheck> checks;
    juce::File playlistExe;
    juce::String playlistVersion;

    juce::File root() const { return pgm.getParentDirectory(); }
    juce::File file (const juce::String& relative) const { return pgm.getChildFile (relative); }
    juce::String summary() const;
};

InstallationInfo inspectInstallation (const juce::File& pgm);

struct InstallationCandidate
{
    juce::File pgm;
    juce::String source; // where the path came from
};

// Candidates from the Playlist Digital uninstall entry, the Commercial and
// Sync Service configuration, and X:\Playlist\pgm on every drive.
std::vector<InstallationCandidate> findInstallationCandidates();

// Executables of the Playlist in the folder: Playlist.exe and versioned
// copies such as "Playlist 5.0.9.09.exe".
juce::Array<juce::File> playlistExecutables (const juce::File& pgm);
bool isPlaylistExecutableName (const juce::String& fileName);

} // namespace pc
