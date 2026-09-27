#include "TestUtils.h"
#include "formats/playlistini/FilePatternResolver.h"

namespace pc::test
{

class IniTests : public juce::UnitTest
{
public:
    IniTests() : juce::UnitTest ("PLAYLIST.ini", "formats") {}

    static juce::MemoryBlock save (const IniDocument& d)
    {
        juce::MemoryBlock m;
        d.toBytes (m);
        return m;
    }

    void runTest() override
    {
        auto original = fixture ("ini/PLAYLIST.ini");

        beginTest ("Round trip is byte exact");
        {
            auto doc = IniDocument::fromBytes (original);
            expect (save (doc) == original);
        }

        beginTest ("Reads sections, keys and commented sections");
        {
            PlaylistIni ini (IniDocument::fromBytes (original));
            auto com = ini.source (ScheduleKind::commercial);
            expect (com.format == ScheduleFormat::txt1);
            expectEquals (com.pattern, juce::String ("MAPAS\\%d-%m-%Y.TXT"));
            expect (ini.source (ScheduleKind::musical).format == ScheduleFormat::automatic);
            expect (! ini.source (ScheduleKind::musicalClock).sectionPresent);
            expect (ini.document().commentedSectionNames().contains ("RDS"));
            expect (! ini.document().hasSection ("RDS"));
            auto aff = ini.affiliates();
            expectEquals ((int) aff.size(), 1);
            expectEquals (aff[0].address, juce::String ("192.168.0.10:3030"));
            expect (ini.document().get ("bloco comercial", "formato").value_or ("") == "TXT1");
        }

        beginTest ("Changing a value touches only that line");
        {
            PlaylistIni ini (IniDocument::fromBytes (original));
            ini.setSource (ScheduleKind::musical, ScheduleFormat::txt1, "GRADES\\%d-%m-%Y.TXT");
            auto text = ini.document().toString();
            auto expected = decodeText (original).text.replace ("[BLOCO MUSICAL]\r\nFORMATO=AUTO\r\n",
                                                                 "[BLOCO MUSICAL]\r\nFORMATO=TXT1\r\nARQUIVO=GRADES\\%d-%m-%Y.TXT\r\n");
            expectEquals (text, expected);
        }

        beginTest ("Adding to the last section keeps the missing final newline");
        {
            PlaylistIni ini (IniDocument::fromBytes (original));
            ini.setAffiliates ({ { "CENTRO", "192.168.0.10:3030" }, { "NORTE", "10.0.0.2:3030" } });
            auto text = ini.document().toString();
            expect (text.endsWith ("CENTRO=192.168.0.10:3030\r\nNORTE=10.0.0.2:3030"), text);
        }

        beginTest ("Creating and emptying the affiliates section");
        {
            PlaylistIni ini (IniDocument::fromBytes (bytes ("[BLOCO MUSICAL]\r\nFORMATO=AUTO\r\n")));
            expect (ini.affiliates().empty());
            ini.setAffiliates ({ { "TIMOTEO", "192.168.5.5:3030" } });
            expectEquals (ini.document().toString(),
                          juce::String ("[BLOCO MUSICAL]\r\nFORMATO=AUTO\r\n\r\n[AFILIADAS]\r\nTIMOTEO=192.168.5.5:3030\r\n"));
            ini.setAffiliates ({});
            expect (! ini.document().hasSection ("AFILIADAS"));
            expect (ini.document().toString().startsWith ("[BLOCO MUSICAL]\r\nFORMATO=AUTO"));
        }

        beginTest ("Adding a new section");
        {
            PlaylistIni ini (IniDocument::fromBytes (original));
            ini.setSource (ScheduleKind::musicalClock, ScheduleFormat::txt1, "Grades\\Relogio%a.txt");
            auto text = ini.document().toString();
            expect (text.endsWith ("CENTRO=192.168.0.10:3030\r\n\r\n[RELOGIO MUSICAL]\r\nFORMATO=TXT1\r\nARQUIVO=Grades\\Relogio%a.txt"), text);
            auto reread = PlaylistIni (IniDocument::fromBytes (save (ini.document())));
            expectEquals (reread.source (ScheduleKind::musicalClock).pattern, juce::String ("Grades\\Relogio%a.txt"));
        }

        beginTest ("Removing an affiliate and preserving spacing around '='");
        {
            auto doc = IniDocument::fromBytes (bytes ("[A]\r\nX = 1\r\nY=2\r\n"));
            doc.set ("A", "X", "9");
            doc.remove ("A", "Y");
            expectEquals (doc.toString(), juce::String ("[A]\r\nX = 9\r\n"));
        }

        beginTest ("Beep");
        {
            PlaylistIni ini (IniDocument::fromBytes (bytes ("[BEEP]\r\nARQUIVO=BEEP.MP3\r\nHORARIO=0,15,30,45\r\n")));
            auto b = ini.beep();
            expect (b.present && b.minutesValid);
            expectEquals (b.minutes.size(), 4);
            PlaylistIni bad (IniDocument::fromBytes (bytes ("[BEEP]\r\nHORARIO=0,75,x\r\n")));
            expect (! bad.beep().minutesValid);
        }

        beginTest ("Pattern expansion");
        {
            Date d { 2021, 12, 31 }; // Friday
            expectEquals (expandPattern ("MAPAS\\%d-%m-%Y.TXT", d), juce::String ("MAPAS\\31-12-2021.TXT"));
            expectEquals (expandPattern ("Mapa%d%m%y.txt", d), juce::String ("Mapa311221.txt"));
            expectEquals (expandPattern ("%a|%w", Date { 2021, 12, 29 }), juce::String ("Qua|3"));
            expectEquals (expandPattern ("%w", Date { 2026, 9, 27 }), juce::String ("0"));
            expectEquals (expandPattern ("%w", Date { 2026, 9, 27 }, WeekdayNumbering::sundaySeven), juce::String ("7"));
            expectEquals (expandPattern ("%a", Date { 2026, 9, 26 }), juce::String (juce::CharPointer_UTF8 ("S\xc3\xa1" "b")));
            expectEquals (expandPattern ("%d", Date { 2026, 9, 1 }), juce::String ("01"));
            expect (unknownPatternVariables ("MAPAS\\%d%H.txt").contains ("%H"));
            expect (unknownPatternVariables ("%d-%m-%Y").isEmpty());
        }

        beginTest ("AUTO search order and resolution");
        {
            TempDir temp;
            auto pgm = temp.dir;
            pgm.getChildFile ("Mapas").createDirectory();
            pgm.getChildFile ("Grades").createDirectory();
            pgm.getChildFile ("Mapas/Mapa.txt").create();
            ScheduleSource s;
            s.kind = ScheduleKind::commercial;
            s.format = ScheduleFormat::automatic;
            Date d { 2021, 12, 31 };
            auto c = scheduleCandidates (s, d, pgm);
            expectEquals (c.front().file.getFileName(), juce::String ("Mapa31-12-2021.txt"));
            expectEquals (c[1].file.getFileName(), juce::String ("Mapa31.txt"));
            expectEquals (c[2].file.getFileName(), juce::String ("Sex.txt"));
            expectEquals (c.back().file.getFileName(), juce::String ("Mapa.txt"));
            auto r = resolveScheduleFile (s, d, pgm);
            expect (r.has_value() && r->file.getFileName() == "Mapa.txt");

            pgm.getChildFile ("Mapas/Mapa31.txt").create();
            r = resolveScheduleFile (s, d, pgm);
            expect (r.has_value() && r->file.getFileName() == "Mapa31.txt");

            ScheduleSource g;
            g.kind = ScheduleKind::musical;
            g.format = ScheduleFormat::automatic;
            auto gc = scheduleCandidates (g, d, pgm);
            expectEquals (gc.front().file.getFullPathName(), pgm.getChildFile ("Grades/31-12-2021.txt").getFullPathName());
            expectEquals (gc.back().file.getFullPathName(), pgm.getChildFile ("Mapas/Grade.txt").getFullPathName());

            ScheduleSource t;
            t.kind = ScheduleKind::commercial;
            t.format = ScheduleFormat::txt1;
            t.pattern = "MAPAS\\%d-%m-%Y.TXT";
            auto tc = scheduleCandidates (t, d, pgm);
            expectEquals ((int) tc.size(), 1);
            expectEquals (tc[0].file.getFileName(), juce::String ("31-12-2021.TXT"));
        }
    }
};

static IniTests iniTests;

} // namespace pc::test
