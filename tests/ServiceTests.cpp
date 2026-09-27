#include "TestUtils.h"
#include "services/Workspace.h"

namespace pc::test
{

class ServiceTests : public juce::UnitTest
{
public:
    ServiceTests() : juce::UnitTest ("Workspace and sessions", "services") {}

    void runTest() override
    {
        TempDir temp;
        auto pgm = copyInstallationFixture (temp);

        beginTest ("Workspace loads the installation");
        Workspace ws (temp.dir.getChildFile ("history"));
        expect (ws.open (pgm), ws.installation().summary());
        expect (ws.ini().has_value() && ws.config().has_value() && ws.folders().has_value() && ws.ligacao().has_value());
        expectEquals ((int) ws.operators().size(), 2);
        expect (ws.loadProblems().empty(), ws.loadProblems().toText());
        expect (ws.readOnly(), "a new workspace starts read-only");

        beginTest ("Schedule files and the active file of a day");
        {
            auto maps = ws.scheduleFiles (false, ScheduleKind::commercial);
            expectEquals ((int) maps.size(), 3); // two dated maps and Mapa.txt
            expect (maps.front().date == Date { 2026, 10, 2 });
            expectEquals ((int) ws.scheduleFiles (true, ScheduleKind::commercial).size(), 1);
            auto active = ws.activeFile (ScheduleKind::commercial, Date { 2026, 10, 2 });
            expect (active.has_value() && active->file.getFileName() == "02-10-2026.txt");
            auto grade = ws.activeFile (ScheduleKind::musical, Date { 2026, 10, 1 });
            expect (grade.has_value() && grade->file.getFileName() == "01-10-2026.txt");
            expect (! ws.activeFile (ScheduleKind::musical, Date { 2026, 10, 9 }).has_value());
        }

        beginTest ("Full diagnostics include the merge problem");
        {
            auto all = ws.runFullDiagnostics();
            bool merge = false;
            for (auto& d : all.items())
                merge = merge || d.code == "merge.folder";
            expect (merge, all.toText (50));
        }

        beginTest ("File session detects external changes and refuses to overwrite");
        {
            FileSession s (pgm.getChildFile ("Mapas/Mapa.txt"));
            juce::String err;
            expect (s.load (err), err);
            expect (! s.changedOnDisk());

            ws.setReadOnly (false);
            auto ok = s.save (ws.writer(), bytes ("00:00 VH\r\n"), "Editar mapa", "teste");
            expect (ok.status == WriteStatus::written, ok.message);
            expect (! s.changedOnDisk());

            juce::Thread::sleep (20);
            pgm.getChildFile ("Mapas/Mapa.txt").replaceWithText ("00:00 OUTRO\r\n");
            expect (s.changedOnDisk());
            auto refused = s.save (ws.writer(), bytes ("00:00 VH, 55\r\n"), "Editar mapa", "teste");
            expect (refused.status == WriteStatus::conflict);
            expect (pgm.getChildFile ("Mapas/Mapa.txt").loadFileAsString() == "00:00 OUTRO\r\n");
            ws.setReadOnly (true);
        }

        beginTest ("Watcher relevance");
        {
            expect (DirectoryWatcher::isRelevant ("PLAYLIST.ini"));
            expect (DirectoryWatcher::isRelevant ("Mapas\\01-10-2026.txt"));
            expect (DirectoryWatcher::isRelevant ("Dados\\LIGACAO.DBF"));
            expect (DirectoryWatcher::isRelevant ("Operadores\\Ana\\Config.xml"));
            expect (! DirectoryWatcher::isRelevant ("Eventos\\2026-09-30.log"));
            expect (! DirectoryWatcher::isRelevant ("Dados\\COMPROVE.DBF"));
            expect (! DirectoryWatcher::isRelevant ("Mapas\\.01-10-2026.txt.pctmp-1a2b"));
            expect (! DirectoryWatcher::isRelevant ("Operadores\\Ana\\Layout.xml"));
        }
        ws.close();
    }
};

static ServiceTests serviceTests;

} // namespace pc::test
