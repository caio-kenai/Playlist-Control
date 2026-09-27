#pragma once

#include "core/Diagnostic.h"
#include "formats/dbf/DbfTable.h"
#include "formats/folders/FoldersXml.h"
#include "platform/WindowsSystem.h"
#include "services/PlaylistProcess.h"
#include "storage/SafeWriter.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <mutex>

namespace pc
{

// The folder configuration of the Config Manager lives in three places that
// it updates together: Folders.xml, one shortcut per folder in pgm\Atalhos and,
// for folders with a code, a record in Dados\LIGACAO.DBF (TIPO=A, ARQUIVO =
// "<Título>.lnk") with its indexes LIGA_COD.NTX and LIGA_ARQ.NTX.

// Fills the values the Config Manager derives from the others: type letter,
// shortcut arguments, shortcut path and, for pause and command folders, the
// pgm folder as target.
void normalizeFolder (FolderEntry& folder, const juce::File& pgm);

// A new folder with the defaults of the Config Manager ("Nova pasta"): title
// from the folder name (or the type name for pause and command), the default
// icon of the type and a code from the first three letters of the title.
FolderEntry makeNewFolder (FolderKind kind, const juce::File& directory, const juce::File& pgm,
                           const std::vector<FolderEntry>& current, const DbfTable* ligacao, int nextId);

// Code suggested for a title: first three letters in upper case; when taken,
// three more random characters, as the Config Manager does.
juce::String suggestFolderCode (const juce::String& title, const std::vector<FolderEntry>& current, const DbfTable* ligacao,
                                juce::Random& random);

// Files to write for a new list of folders, and the problems found.
struct FolderPlan
{
    struct Shortcut
    {
        juce::File file;
        ShortcutInfo info;
    };

    juce::MemoryBlock foldersXml;
    std::optional<juce::MemoryBlock> ligacao, ligaCod, ligaArq;
    std::vector<Shortcut> shortcuts;     // written (new or changed folders)
    std::vector<juce::File> removed;     // shortcuts of removed or renamed folders
    juce::StringArray changes;           // what changed, for the operator and the history
    DiagnosticList problems;             // errors block the save

    bool hasChanges() const { return ! changes.isEmpty(); }
};

FolderPlan planFolderChanges (const juce::File& pgm, const FoldersXml& original, const std::vector<FolderEntry>& edited,
                              const juce::MemoryBlock* ligacaoBytes, juce::Time now);

// Writes a plan: Folders.xml, LIGACAO.DBF and its indexes through the safe
// writer (history kept), then the shortcuts. The files replaced are copied to
// 'backupFolder' first and put back if a step fails.
bool applyFolderPlan (const FolderPlan& plan, const juce::File& pgm, SafeWriter& writer, const juce::File& backupFolder,
                      juce::String& error);

// Saving the folders as the Config Manager asks ("reinicie o Playlist
// Digital"): close the Playlist through its window when it is open, copy the
// files, write them and open the Playlist again.
class FolderSave : private juce::Thread
{
public:
    FolderSave (FolderPlan plan, juce::File pgm, SafeWriter& writer, juce::File backupRoot, bool startPlaylistAtEnd);
    ~FolderSave() override;

    void start() { startThread(); }
    bool running() const { return isThreadRunning(); }
    bool finished() const { return finished_; }
    bool succeeded() const { return succeeded_; }
    std::vector<JobStep> steps() const;
    juce::String outcome() const;
    juce::File backupFolder() const { return backupFolder_; }

    std::function<void()> onChange; // message thread

private:
    enum StepId
    {
        stepCheck,
        stepClose,
        stepWrite,
        stepStart
    };

    void run() override;
    void set (int step, JobStep::State state, const juce::String& detail = {});
    void finish (bool ok, const juce::String& text);

    FolderPlan plan_;
    juce::File pgm_, backupRoot_, backupFolder_;
    SafeWriter& writer_;
    bool startAtEnd_;
    mutable std::mutex lock_;
    std::vector<JobStep> steps_;
    juce::String outcome_;
    std::atomic<bool> finished_ { false }, succeeded_ { false };

    JUCE_DECLARE_WEAK_REFERENCEABLE (FolderSave)
};

} // namespace pc
