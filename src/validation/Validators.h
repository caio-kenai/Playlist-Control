#pragma once

#include "catalog/CodeCatalog.h"
#include "core/Diagnostic.h"
#include "ecosystem/Origin.h"
#include "formats/configxml/ConfigXml.h"
#include "formats/montagem/MontagemFile.h"
#include "formats/playlistini/PlaylistIni.h"

namespace pc
{

struct ScheduleValidationContext
{
    const CodeCatalog* catalog = nullptr; // null: codes and files are not checked
    std::optional<Date> date;             // day the file is used for (validity of registrations)
    bool isClock = false;                 // relógio files carry times and parameters only
};

DiagnosticList validateSchedule (const ScheduleDocument& doc, const juce::File& file, ScheduleKind kind,
                                 const ScheduleValidationContext& context);

DiagnosticList validatePlaylistIni (const PlaylistIni& ini, const juce::File& file, const juce::File& pgm,
                                    const Date& today, int daysAhead = 7);

// Validates one CONFIG.XML value before it is written.
std::optional<Diagnostic> validateConfigValue (const ConfigEntry& entry, const juce::String& newValue,
                                               const juce::File& file);
DiagnosticList validateConfig (const ConfigXml& config, const juce::File& file);

DiagnosticList validateFolders (const FoldersXml& folders, const CodeCatalog& catalog, const juce::File& file,
                                const juce::File& pgm);

DiagnosticList validateMerge (const MontagemFile& merge, const juce::File& file, const FoldersXml* folders);

} // namespace pc
