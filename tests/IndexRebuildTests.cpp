#include "TestUtils.h"
#include "services/FolderConfig.h"
#include "services/IndexMaintenance.h"

namespace pc::test
{

extern juce::File rebuildDemoUnderTest;

// End-to-end recreation of the indexes. It closes and starts programs and
// deletes files, so it runs only on a demonstration copy given explicitly:
//   playlistcontrol_tests --category=rebuild --rebuild-demo=<pgm of a copy>
class IndexRebuildTests : public juce::UnitTest
{
public:
    IndexRebuildTests() : juce::UnitTest ("Index recreation (demo installation)", "rebuild") {}

    void runTest() override
    {
        auto pgm = rebuildDemoUnderTest;
        if (pgm == juce::File())
            return;

        beginTest ("Check before the recreation");
        auto check = checkIndexRebuild (pgm);
        expect (check.blockers.isEmpty(), check.blockers.joinIntoString ("\n"));
        logMessage ("Playlist aberto: " + juce::String ((int) check.playlist.size()) + ", executavel: " + check.playlistExe.getFullPathName());

        beginTest ("Recreation runs every step");
        auto backups = pgm.getParentDirectory().getChildFile ("rebuild-backups");
        IndexRebuild rebuild (pgm, backups, {});
        rebuild.start();
        for (int i = 0; i < 1200 && ! rebuild.finished(); ++i)
            juce::Thread::sleep (100);
        expect (rebuild.finished(), "the recreation did not finish in 2 minutes");
        for (auto& s : rebuild.steps())
            logMessage ("  [" + juce::String ((int) s.state) + "] " + s.title + " - " + s.detail);
        logMessage ("Resultado: " + rebuild.outcome());
        expect (rebuild.backupFolder().getChildFile ("Indices").getNumberOfChildFiles (juce::File::findFiles) > 0);
        for (auto& s : rebuild.steps())
            expect (s.state != IndexRebuild::StepState::pending && s.state != IndexRebuild::StepState::running, s.title);
    }
};

static IndexRebuildTests indexRebuildTests;

// Saving the Config Manager folders on a demonstration copy, closing and
// reopening its Playlist (same option as above).
class FolderSaveDemoTests : public juce::UnitTest
{
public:
    FolderSaveDemoTests() : juce::UnitTest ("Folder save (demo installation)", "rebuild") {}

    void runTest() override
    {
        auto pgm = rebuildDemoUnderTest;
        if (pgm == juce::File())
            return;

        beginTest ("A new folder is saved and the Playlist reopened");
        juce::MemoryBlock foldersBytes, dbf;
        pgm.getChildFile ("Folders.xml").loadFileAsData (foldersBytes);
        pgm.getChildFile ("Dados/LIGACAO.DBF").loadFileAsData (dbf);
        juce::String error;
        auto folders = FoldersXml::parse (foldersBytes, error);
        expect (folders.has_value(), error);
        auto list = folders->folders();
        auto dir = pgm.getParentDirectory().getChildFile ("Chamadas");
        dir.createDirectory();
        list.push_back (makeNewFolder (FolderKind::random, dir, pgm, list, nullptr, folders->nextId()));
        auto plan = planFolderChanges (pgm, *folders, list, &dbf, juce::Time::getCurrentTime());
        expect (! plan.problems.hasErrors(), plan.problems.toText());

        HistoryStore history (pgm.getParentDirectory().getChildFile ("demo-history"));
        SafeWriter writer (history);
        FolderSave save (std::move (plan), pgm, writer, pgm.getParentDirectory().getChildFile ("folder-backups"), false);
        save.start();
        for (int i = 0; i < 1200 && ! save.finished(); ++i)
            juce::Thread::sleep (100);
        for (auto& s : save.steps())
            logMessage ("  [" + juce::String ((int) s.state) + "] " + s.title + " - " + s.detail);
        logMessage ("Resultado: " + save.outcome());
        expect (save.succeeded());
        expect (pgm.getChildFile ("Atalhos/Chamadas.lnk").existsAsFile());
        juce::MemoryBlock after;
        pgm.getChildFile ("Folders.xml").loadFileAsData (after);
        auto reread = FoldersXml::parse (after, error);
        expect (reread.has_value() && reread->findByCode ("CHA") != nullptr);
    }
};

static FolderSaveDemoTests folderSaveDemoTests;

} // namespace pc::test
