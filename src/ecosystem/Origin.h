#pragma once

#include "ecosystem/Ecosystem.h"
#include "formats/playlistini/PlaylistIni.h"
#include "formats/schedule/ScheduleDocument.h"

namespace pc
{

enum class FileOrigin
{
    planner,    // Planner web, delivered by the Sync Service
    commercial, // Commercial.exe export
    maker,      // Maker / Playlist Server
    electoral,  // Horário Eleitoral (.merge)
    manual,     // edited by hand or by PlaylistControl
    unknown
};

juce::String toDisplayString (FileOrigin origin);

struct OriginAssessment
{
    FileOrigin origin = FileOrigin::unknown;
    bool confirmed = false;        // confirmed by the producer's own configuration
    juce::String reason;           // how the origin was determined
    bool rewrittenAutomatically = false;
    juce::String overwriteWarning; // what happens to local edits
};

struct OriginContext
{
    const EcosystemInfo* ecosystem = nullptr;
    juce::File pgm;
    Date today = Date::today();
};

// Date encoded in a daily file name "dd-mm-aaaa.txt" (also "Mapadd-mm-aaaa").
std::optional<Date> dateFromFileName (const juce::String& fileName);

OriginAssessment assessScheduleOrigin (const juce::File& file, const ScheduleDocument& doc, ScheduleKind kind,
                                       const OriginContext& context);

// Running programs that matter before writing into the installation.
struct PlaylistRuntime
{
    bool playlistRunning = false;
    juce::StringArray playlistProcesses;
    bool configManagerRunning = false;
    bool ligacaoRunning = false;
    bool commercialRunning = false;
    bool makerRunning = false;
};

PlaylistRuntime queryRuntime (const juce::File& pgm);

} // namespace pc
