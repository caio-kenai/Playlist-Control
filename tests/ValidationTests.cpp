#include "TestUtils.h"
#include "install/Installation.h"
#include "validation/Validators.h"

namespace pc::test
{

class ValidationTests : public juce::UnitTest
{
public:
    ValidationTests() : juce::UnitTest ("Catalog, origin and validation", "validation") {}

    static bool has (const DiagnosticList& list, const juce::String& code, int line = -1)
    {
        for (auto& d : list.items())
            if (d.code == code && (line < 0 || d.line == line))
                return true;
        return false;
    }

    static juce::MemoryBlock load (const juce::File& f)
    {
        juce::MemoryBlock m;
        f.loadFileAsData (m);
        return m;
    }

    void runTest() override
    {
        TempDir temp;
        auto pgm = copyInstallationFixture (temp);
        juce::String err;
        auto folders = FoldersXml::parse (load (pgm.getChildFile ("Folders.xml")), err);
        auto ligacao = DbfTable::parse (load (pgm.getChildFile ("Dados/LIGACAO.DBF")), err);
        CodeCatalog catalog;
        catalog.load (&*folders, &*ligacao);
        catalog.indexFolderFiles();

        beginTest ("Installation inspection");
        {
            auto info = inspectInstallation (pgm);
            expect (info.valid, info.summary());
            auto empty = inspectInstallation (temp.dir);
            expect (! empty.valid);
            expect (isPlaylistExecutableName ("Playlist 5.0.9.09.exe"));
            expect (isPlaylistExecutableName ("PLAYLIST.EXE"));
            expect (! isPlaylistExecutableName ("PlaylistDesk.exe"));
            expect (! isPlaylistExecutableName ("PlaylistServer.exe"));
        }

        beginTest ("Code catalog");
        {
            expectEquals (CodeCatalog::normalize (" 055 "), juce::String ("55"));
            expectEquals (CodeCatalog::normalize ("0"), juce::String ("0"));
            expectEquals (CodeCatalog::normalize ("vh"), juce::String ("VH"));
            expect (catalog.folderForCode ("com") != nullptr);
            expectEquals ((int) catalog.registrationsFor ("12").size(), 2);
            expect (catalog.registrationsFor ("77").empty()); // deleted record
            Date d { 2026, 10, 2 };
            expect (catalog.resolve (ScheduleItem::makeCode ("55"), d).status == ItemStatus::ok);
            expect (catalog.resolve (ScheduleItem::makeCode ("055"), d).status == ItemStatus::ok);
            expect (catalog.resolve (ScheduleItem::makeCode ("23"), d).status == ItemStatus::outOfValidity);
            expect (catalog.resolve (ScheduleItem::makeCode ("23"), Date { 2026, 1, 15 }).status == ItemStatus::ok);
            expect (catalog.resolve (ScheduleItem::makeCode ("62"), d).status == ItemStatus::fileMissing);
            expect (catalog.resolve (ScheduleItem::makeCode ("99"), d).status == ItemStatus::unknownCode);
            expect (catalog.resolve (ScheduleItem::makeCode ("VH"), d).status == ItemStatus::ok);
            auto rot = catalog.resolve (ScheduleItem::makeCode ("12"), d);
            expect (rot.status == ItemStatus::ok && rot.description.contains ("rod"));
            expect (catalog.resolve (ScheduleItem::makeFile ("trio sol - estrada.mp3"), d).status == ItemStatus::ok);
            expect (catalog.resolve (ScheduleItem::makeFile ("Musica Inexistente.mp3"), d).status == ItemStatus::fileMissing);
            expect (catalog.resolve (ScheduleItem::makeCodeAndFile ("A1", "spot_padaria.aac"), d).status == ItemStatus::ok);
            expect (catalog.resolve (ScheduleItem::makeCommand ("IANEWS"), d).status == ItemStatus::notChecked);
        }

        beginTest ("Commercial map validation");
        {
            auto file = pgm.getChildFile ("Mapas/02-10-2026.txt");
            auto doc = ScheduleDocument::fromBytes (load (file));
            ScheduleValidationContext ctx { &catalog, Date { 2026, 10, 2 }, false };
            auto diags = validateSchedule (doc, file, ScheduleKind::commercial, ctx);
            expect (has (diags, "schedule.item.validity", 1));   // 23 expired
            expect (has (diags, "schedule.item.file", 1));       // 62 file missing
            expect (has (diags, "schedule.item.unknown", 2));    // 99
            expect (has (diags, "schedule.time.order", 3));      // 06:15 after 06:30
            expect (! has (diags, "schedule.item.unknown", 4));  // MUSIC and COM are folders
            for (auto& d : diags.items())
                expect (d.reason.isNotEmpty() && d.fix.isNotEmpty(), d.code);
        }

        beginTest ("Maker grade and clocks");
        {
            auto file = pgm.getChildFile ("Grades/01-10-2026.txt");
            auto doc = ScheduleDocument::fromBytes (load (file));
            auto diags = validateSchedule (doc, file, ScheduleKind::musical, { &catalog, Date { 2026, 10, 1 }, false });
            expectEquals (diags.count (Severity::error), 1);
            expect (has (diags, "schedule.item.file", 6));

            auto clock = pgm.getChildFile ("Mapas/Relogio.txt");
            auto cd = validateSchedule (ScheduleDocument::fromBytes (load (clock)), clock, ScheduleKind::commercialClock, { nullptr, {}, true });
            expect (cd.empty(), cd.toText());

            auto bad = ScheduleDocument::fromBytes (fixture ("schedule/invalid.txt"));
            auto bd = validateSchedule (bad, juce::File ("invalid.txt"), ScheduleKind::commercial, {});
            expect (has (bd, "schedule.line", 2));
            expect (has (bd, "schedule.param.dur", 4));
            expect (has (bd, "schedule.time.order", 5));
            expect (has (bd, "schedule.time.duplicate", 6));
        }

        beginTest ("Origin of schedule files");
        {
            EcosystemInfo eco;
            OriginContext ctx { &eco, pgm, Date { 2026, 10, 1 } };
            auto planner = pgm.getChildFile ("Mapas/01-10-2026.txt");
            auto a = assessScheduleOrigin (planner, ScheduleDocument::fromBytes (load (planner)), ScheduleKind::commercial, ctx);
            expect (a.origin == FileOrigin::planner && ! a.confirmed);

            eco.sync.found = true;
            eco.sync.active = true;
            eco.sync.writesMaps = true;
            eco.sync.daysAhead = 7;
            eco.sync.mapsFolder = pgm.getChildFile ("Mapas").getFullPathName().toLowerCase();
            a = assessScheduleOrigin (planner, ScheduleDocument::fromBytes (load (planner)), ScheduleKind::commercial, ctx);
            expect (a.origin == FileOrigin::planner && a.confirmed && a.rewrittenAutomatically);

            auto grade = pgm.getChildFile ("Grades/01-10-2026.txt");
            auto g = assessScheduleOrigin (grade, ScheduleDocument::fromBytes (load (grade)), ScheduleKind::musical, ctx);
            expect (g.origin == FileOrigin::maker);

            auto mapa = pgm.getChildFile ("Mapas/Mapa.txt");
            auto m = assessScheduleOrigin (mapa, ScheduleDocument::fromBytes (load (mapa)), ScheduleKind::commercial, ctx);
            expect (m.origin == FileOrigin::manual && ! m.rewrittenAutomatically);

            expect (dateFromFileName ("Mapa31-12-2021.txt") == Date { 2021, 12, 31 });
            expect (! dateFromFileName ("Mapa.txt").has_value());
            expect (! dateFromFileName ("41-13-2021.txt").has_value());

            SyncServiceInfo s;
            readSyncServiceConfig ("{\"DaysLimit\":\"3\",\"SyncTime\":\"10\",\"SyncMapas\":\"1\",\"SyncActive\":\"1\","
                                   "\"MapasFolder\":\"c:\\\\Playlist\\\\pgm\\\\Mapas\"}", s);
            s.found = true;
            expect (s.daysAhead == 3 && s.intervalMinutes == 10);
            expect (s.managesDate (Date { 2026, 10, 4 }, Date { 2026, 10, 1 }));
            expect (! s.managesDate (Date { 2026, 10, 5 }, Date { 2026, 10, 1 }));
            expect (samePath (s.mapsFolder, juce::File ("C:\\Playlist\\pgm\\Mapas")));

            CommercialInfo c;
            readCommercialStation ("<Emissora><Pastas><PastaMapas>C:\\\\Playlist\\\\pgm\\\\Mapas\\\\</PastaMapas>"
                                   "<ExportaDataCompleta>1</ExportaDataCompleta></Pastas></Emissora>", c);
            expect (c.exportsFullDate && samePath (c.mapsFolder, juce::File ("C:\\Playlist\\pgm\\Mapas")));
        }

        beginTest ("PLAYLIST.ini validation");
        {
            auto file = pgm.getChildFile ("PLAYLIST.ini");
            PlaylistIni ini (IniDocument::fromBytes (load (file)));
            auto d = validatePlaylistIni (ini, file, pgm, Date { 2026, 10, 1 }, 1);
            expect (! d.hasErrors(), d.toText());
            expect (! has (d, "ini.files.missing"), d.toText()); // 01 and 02/10 exist

            ini.setSource (ScheduleKind::commercial, ScheduleFormat::txt1, "MAPAS\\%d%H.TXT");
            ini.document().set ("AFILIADAS", "NORTE", "semporta");
            ini.document().set ("BEEP", "HORARIO", "0,61");
            ini.document().set ("RELOGIO MUSICAL", "FORMATO", "DBF");
            auto bad = validatePlaylistIni (ini, file, pgm, Date { 2026, 10, 1 }, 1);
            expect (has (bad, "ini.pattern.variable"));
            expect (has (bad, "ini.affiliate"));
            expect (has (bad, "ini.beep.minutes"));
            expect (has (bad, "ini.format"));
            expect (has (bad, "ini.files.missing"));
        }

        beginTest ("CONFIG.XML values");
        {
            auto file = pgm.getChildFile ("CONFIG.XML");
            auto cfg = ConfigXml::parse (load (file), err);
            ConfigEntry perc;
            for (auto& e : cfg->entries())
                if (e.path == "nPercComprovacao")
                    perc = e;
            expect (validateConfigValue (perc, "101", file).has_value());
            expect (validateConfigValue (perc, "abc", file).has_value());
            expect (! validateConfigValue (perc, "70", file).has_value());
            expect (validateConfig (*cfg, file).empty(), validateConfig (*cfg, file).toText());
        }

        beginTest ("Folders and registrations");
        {
            auto d = validateFolders (*folders, catalog, pgm.getChildFile ("Folders.xml"), pgm);
            expect (has (d, "folders.target"), d.toText());   // Acervo Antigo does not exist
            expect (has (d, "folders.shortcut"), d.toText());
            expect (! has (d, "folders.code.duplicate"));
            expect (! has (d, "folders.code.unregistered"), d.toText());
        }

        beginTest ("Merge referencing a renamed folder");
        {
            auto file = pgm.getChildFile ("Montagem/01-10-2026 - POLITICO.merge");
            auto d = validateMerge (MontagemFile::parse (load (file)), file, &*folders);
            expectEquals ((int) d.size(), 1);
            expect (has (d, "merge.folder", 1));
            expect (d.items()[0].fix.contains (juce::String (juce::CharPointer_UTF8 ("Elei\xc3\xa7\xc3\xb5" "es"))), d.toText());
        }
    }
};

static ValidationTests validationTests;

} // namespace pc::test
