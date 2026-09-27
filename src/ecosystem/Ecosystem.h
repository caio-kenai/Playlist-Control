#pragma once

#include "core/TimeOfDay.h"
#include "platform/WindowsSystem.h"

namespace pc
{

// What the other Playlist programs installed on this machine are configured
// to do with the pgm folder. Everything here is read-only.
struct CommercialInfo
{
    bool found = false;
    juce::String version;
    juce::File folder;
    juce::String playlistFolder; // Emissora.xml <PastaPlaylist>
    juce::String mapsFolder;     // <PastaMapas>
    bool exportsFullDate = false;// <ExportaDataCompleta>
};

struct SyncServiceInfo
{
    bool found = false;
    juce::String version;
    juce::File configFile;
    bool active = false;       // SyncActive
    bool writesMaps = false;   // SyncMapas
    int daysAhead = 7;         // DaysLimit
    int intervalMinutes = 5;   // SyncTime
    juce::String pgmFolder;
    juce::String mapsFolder;
    juce::String stationName;
    juce::String lastSync;
    ServiceState service = ServiceState::notInstalled;

    // Maps for these days are rewritten by the Sync Service.
    bool managesDate (const Date& date, const Date& today) const;
};

struct PlaylistServerInfo
{
    bool found = false;
    juce::String version;
    ServiceState service = ServiceState::notInstalled;
    int port = 0;
};

struct MakerInfo
{
    bool found = false;
    juce::String version;
};

struct EcosystemInfo
{
    CommercialInfo commercial;
    SyncServiceInfo sync;
    PlaylistServerInfo playlistServer;
    MakerInfo maker;
    std::vector<ServiceInfo> services;
    std::vector<InstalledProgram> programs; // Playlist Software products
};

// pgm is used to check whether each program points at this installation.
EcosystemInfo scanEcosystem (const juce::File& pgm);

// Parsing helpers, exposed for tests.
void readCommercialStation (const juce::String& xml, CommercialInfo& info);
void readSyncServiceConfig (const juce::String& json, SyncServiceInfo& info);

bool samePath (const juce::String& a, const juce::File& b);

} // namespace pc
