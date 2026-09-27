#include "TestUtils.h"
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

} // namespace pc::test
