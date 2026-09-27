#include "TestUtils.h"
#include "formats/configxml/ConfigXml.h"
#include "formats/folders/FoldersXml.h"
#include "formats/ntx/NtxIndex.h"
#include "formats/operators/OperatorProfile.h"
#include "install/Installation.h"
#include "storage/FileIO.h"
#include "validation/Validators.h"

#include <map>

namespace pc::test
{
extern juce::File installationUnderTest;

// Read-only checks against a real installation:
//   playlistcontrol_tests --installation C:\Playlist\pgm
// Nothing is written: files are read with full sharing and every
// serialisation happens in memory.
class RealInstallationTests : public juce::UnitTest
{
public:
    RealInstallationTests() : juce::UnitTest ("Real installation (read-only)", "installation") {}

    juce::MemoryBlock read (const juce::File& f)
    {
        juce::MemoryBlock m;
        juce::String err;
        expect (readFileShared (f, m, err), f.getFullPathName() + ": " + err);
        return m;
    }

    void runTest() override
    {
        auto pgm = installationUnderTest;
        if (pgm == juce::File())
            return;

        beginTest ("Installation is recognised");
        auto info = inspectInstallation (pgm);
        expect (info.valid, info.summary());
        logMessage (info.summary());

        beginTest ("Schedules round-trip byte for byte");
        int schedules = 0, lines = 0;
        for (auto* dir : { "Mapas", "Grades", "Modelos" })
        {
            for (auto& f : pgm.getChildFile (dir).findChildFiles (juce::File::findFiles, false, "*.txt"))
            {
                auto bytes = read (f);
                auto doc = ScheduleDocument::fromBytes (bytes);
                juce::MemoryBlock back;
                doc.toBytes (back);
                expect (back == bytes, f.getFullPathName());
                ++schedules;
                lines += (int) doc.lines().size();
            }
        }
        logMessage (juce::String (schedules) + " arquivos de mapa/grade/modelo, " + juce::String (lines) + " linhas.");

        beginTest ("PLAYLIST.ini round-trip");
        {
            auto bytes = read (pgm.getChildFile ("PLAYLIST.ini"));
            juce::MemoryBlock back;
            IniDocument::fromBytes (bytes).toBytes (back);
            expect (back == bytes);
        }

        beginTest ("XML files round-trip");
        {
            juce::Array<juce::File> xmls { pgm.getChildFile ("CONFIG.XML"), pgm.getChildFile ("Folders.xml") };
            for (auto& f : pgm.getChildFile ("Operadores").findChildFiles (juce::File::findFiles, true, "*.xml"))
                xmls.add (f);
            for (auto& f : xmls)
            {
                if (! f.existsAsFile())
                    continue;
                auto bytes = read (f);
                juce::String err;
                auto doc = XmlPatchDocument::parse (bytes, err);
                expect (doc.has_value(), f.getFullPathName() + ": " + err);
                if (! doc.has_value())
                    continue;
                juce::MemoryBlock back;
                doc->toBytes (back);
                expect (back == bytes, f.getFullPathName());
            }
            juce::String err;
            auto cfg = ConfigXml::parse (read (pgm.getChildFile ("CONFIG.XML")), err);
            expect (cfg.has_value(), err);
            int documented = 0, total = 0;
            for (auto& e : cfg->entries())
            {
                ++total;
                documented += e.field != nullptr ? 1 : 0;
            }
            logMessage ("CONFIG.XML: " + juce::String (documented) + " de " + juce::String (total) + " chaves documentadas.");

            // The folder editor writes Folders.xml back unchanged when nothing changed.
            auto foldersBytes = read (pgm.getChildFile ("Folders.xml"));
            auto folders = FoldersXml::parse (foldersBytes, err);
            expect (folders.has_value(), err);
            if (folders.has_value())
            {
                juce::MemoryBlock rendered;
                expect (folders->renderBytes (folders->folders(), rendered));
                expect (rendered == foldersBytes, "Folders.xml");
                for (auto& f : folders->folders())
                    expect (f.kind != FolderKind::unknown, f.title + ": " + f.type);
            }
        }

        beginTest ("Data files");
        {
            juce::String err;
            auto lig = DbfTable::parse (read (pgm.getChildFile ("Dados/LIGACAO.DBF")), err);
            expect (lig.has_value(), err);
            if (lig.has_value())
                logMessage ("LIGACAO.DBF: " + juce::String (lig->recordCount()) + " registros.");
            auto comprove = DbfTable::parse (read (pgm.getChildFile ("Dados/COMPROVE.DBF")), err);
            for (auto& f : pgm.getChildFile ("Indices").findChildFiles (juce::File::findFiles, false, "*.NTX"))
            {
                auto bytes = read (f);
                auto h = NtxHeader::parse (bytes);
                expect (h.valid, f.getFileName() + ": " + h.problem);
                auto* table = f.getFileName().startsWithIgnoreCase ("LIGA") ? (lig ? &*lig : nullptr) : (comprove ? &*comprove : nullptr);
                if (table == nullptr)
                    continue;
                auto v = verifyNtx (bytes, *table);
                logMessage (f.getFileName() + ": " + h.keyExpression + " -> " + v.summary);

                // Rebuilt in memory, compared with the file the Playlist wrote.
                std::vector<NtxEntry> entries;
                juce::String e2;
                if (expectedNtxEntries (h, *table, entries, e2))
                {
                    auto rebuilt = buildNtx (h, entries);
                    int differing = 0;
                    auto n = juce::jmin (rebuilt.getSize(), bytes.getSize());
                    for (size_t i = 0; i < n; ++i)
                        differing += static_cast<const char*> (rebuilt.getData())[i] != static_cast<const char*> (bytes.getData())[i] ? 1 : 0;
                    logMessage ("   reconstruído: " + juce::String ((int) rebuilt.getSize()) + " bytes, " + juce::String (differing)
                                + " bytes diferentes do arquivo original"
                                + (rebuilt.getSize() == bytes.getSize() ? juce::String() : juce::String (" (tamanho diferente)")));
                    expect (readNtx (rebuilt).entries == readNtx (bytes).entries || ! v.consistent);
                }
            }
        }

        beginTest ("Validation runs on every schedule");
        {
            juce::String err;
            auto folders = FoldersXml::parse (read (pgm.getChildFile ("Folders.xml")), err);
            auto lig = DbfTable::parse (read (pgm.getChildFile ("Dados/LIGACAO.DBF")), err);
            CodeCatalog catalog;
            catalog.load (folders.has_value() ? &*folders : nullptr, lig.has_value() ? &*lig : nullptr);
            catalog.indexFolderFiles();
            int errors = 0, warnings = 0;
            std::map<juce::String, int> byCode;
            std::map<juce::String, juce::String> example;
            std::map<juce::String, int> missingFiles;
            for (auto* dir : { "Mapas", "Grades" })
            {
                auto kind = juce::String (dir) == "Mapas" ? ScheduleKind::commercial : ScheduleKind::musical;
                for (auto& f : pgm.getChildFile (dir).findChildFiles (juce::File::findFiles, false, "*.txt"))
                {
                    ScheduleValidationContext ctx;
                    ctx.catalog = &catalog;
                    ctx.date = dateFromFileName (f.getFileName());
                    ctx.isClock = f.getFileName().startsWithIgnoreCase ("Relogio");
                    auto d = validateSchedule (ScheduleDocument::fromBytes (read (f)), f, kind, ctx);
                    errors += d.count (Severity::error);
                    warnings += d.count (Severity::warning);
                    for (auto& item : d.items())
                    {
                        if (item.code == "schedule.item.file")
                            ++missingFiles[item.message.fromFirstOccurrenceOf (": ", false, false)];
                        ++byCode[item.code];
                        if (example[item.code].isEmpty())
                            example[item.code] = item.locationText() + ": " + item.message;
                    }
                }
            }
            for (auto& f : pgm.getChildFile ("Montagem").findChildFiles (juce::File::findFiles, false, "*.merge"))
            {
                auto d = validateMerge (MontagemFile::parse (read (f)), f, folders.has_value() ? &*folders : nullptr);
                errors += d.count (Severity::error);
                if (! d.empty())
                    logMessage (d.toText (3));
            }
            logMessage (L"Diagnósticos: " + juce::String (errors) + " erros, " + juce::String (warnings) + " avisos.");
            for (auto& [code, n] : byCode)
                logMessage ("  " + code + ": " + juce::String (n) + "  (ex.: " + example[code] + ")");
            for (auto& [missingName, n] : missingFiles)
                logMessage ("    ausente " + juce::String (n) + "x: " + missingName);
        }
    }
};

static RealInstallationTests realInstallationTests;

} // namespace pc::test
