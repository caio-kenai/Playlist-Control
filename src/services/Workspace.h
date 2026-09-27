#pragma once

#include "formats/operators/OperatorProfile.h"
#include "formats/playlistini/FilePatternResolver.h"
#include "install/Installation.h"
#include "services/DirectoryWatcher.h"
#include "services/FileSession.h"
#include "services/IndexMaintenance.h"
#include "validation/Validators.h"

#include <juce_events/juce_events.h>

namespace pc
{

// A schedule file (map, grade or clock) present in the installation.
struct ScheduleFileInfo
{
    juce::File file;
    ScheduleKind kind = ScheduleKind::commercial;
    std::optional<Date> date;
    bool isClock = false;
};

// One line of the activity feed shown to the operator.
struct ActivityEntry
{
    juce::Time time;
    juce::String text;
    juce::File file;
    bool external = false; // changed by another program
};

// The installation loaded in memory: parsed configuration, catalog,
// ecosystem state, activity and the writer. Lives on the message thread.
class Workspace : public juce::ChangeBroadcaster
{
public:
    Workspace (juce::File historyFolder);
    ~Workspace() override;

    bool open (const juce::File& pgm);
    void close();
    void reload();
    bool isOpen() const noexcept { return installation_.valid; }

    const InstallationInfo& installation() const noexcept { return installation_; }
    juce::File pgm() const { return installation_.pgm; }

    const std::optional<PlaylistIni>& ini() const noexcept { return ini_; }
    const std::optional<ConfigXml>& config() const noexcept { return config_; }
    const std::optional<FoldersXml>& folders() const noexcept { return folders_; }
    const std::optional<DbfTable>& ligacao() const noexcept { return ligacao_; }
    const std::vector<OperatorProfile>& operators() const noexcept { return operators_; }
    const CodeCatalog& catalog() const noexcept { return catalog_; }
    const EcosystemInfo& ecosystem() const noexcept { return ecosystem_; }
    const PlaylistRuntime& runtime() const noexcept { return runtime_; }
    const DiagnosticList& loadProblems() const noexcept { return loadProblems_; }

    void refreshRuntime();

    std::vector<ScheduleFileInfo> scheduleFiles (bool clocks, ScheduleKind kind) const;
    ScheduleSource source (ScheduleKind kind) const;
    std::optional<ScheduleCandidate> activeFile (ScheduleKind kind, const Date& date) const;

    OriginAssessment originOf (const juce::File& file, const ScheduleDocument& doc, ScheduleKind kind) const;
    ScheduleValidationContext validationContext (const ScheduleFileInfo& info) const;

    // Every check the application knows, over every file.
    DiagnosticList runFullDiagnostics() const;

    // Indices\*.NTX compared with the tables they index (read only).
    DiagnosticList verifyIndexes() const;

    // Recreation of the indexes (kept here so it outlives the view that started it).
    IndexRebuild* indexRebuild() const noexcept { return indexRebuild_.get(); }
    IndexRebuild* startIndexRebuild (const IndexRebuild::Options& options);

    SafeWriter& writer() noexcept { return writer_; }
    HistoryStore& history() noexcept { return history_; }
    void setReadOnly (bool readOnly);
    bool readOnly() const noexcept { return writer_.isReadOnly(); }

    const std::vector<ActivityEntry>& activity() const noexcept { return activity_; }
    void addActivity (const juce::String& text, const juce::File& file = {}, bool external = false);

    // Called with the files other programs changed.
    std::function<void (const std::vector<DirectoryWatcher::Change>&)> onExternalChange;

private:
    void loadAll();
    void handleChanges (const std::vector<DirectoryWatcher::Change>& changes);

    InstallationInfo installation_;
    std::optional<PlaylistIni> ini_;
    std::optional<ConfigXml> config_;
    std::optional<FoldersXml> folders_;
    std::optional<DbfTable> ligacao_;
    std::vector<OperatorProfile> operators_;
    CodeCatalog catalog_;
    EcosystemInfo ecosystem_;
    PlaylistRuntime runtime_;
    DiagnosticList loadProblems_;
    HistoryStore history_;
    std::unique_ptr<IndexRebuild> indexRebuild_;
    SafeWriter writer_;
    DirectoryWatcher watcher_;
    std::vector<ActivityEntry> activity_;
    std::map<juce::String, juce::Time> ownWrites_;
};

} // namespace pc
