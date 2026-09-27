#include "TestUtils.h"
#include "formats/ntx/NtxIndex.h"
#include "services/FolderConfig.h"
#include "services/Workspace.h"

namespace pc::test
{

namespace
{
FoldersXml loadFolders (const juce::File& pgm)
{
    juce::MemoryBlock m;
    pgm.getChildFile ("Folders.xml").loadFileAsData (m);
    juce::String error;
    auto f = FoldersXml::parse (m, error);
    jassert (f.has_value());
    return *f;
}

DbfTable table (const juce::MemoryBlock& m)
{
    juce::String error;
    return *DbfTable::parse (m, error);
}

int recordWithCode (const DbfTable& t, const juce::String& code)
{
    for (int r = 0; r < t.recordCount(); ++r)
        if (t.getString (r, "CODIGO").trim() == code)
            return r;
    return -1;
}
} // namespace

// The folders of the Config Manager: Folders.xml, shortcuts and LIGACAO.DBF.
class FolderConfigTests : public juce::UnitTest
{
public:
    FolderConfigTests() : juce::UnitTest ("Config Manager folders", "folders") {}

    void runTest() override
    {
        TempDir temp;
        auto pgm = copyInstallationFixture (temp);
        auto folders = loadFolders (pgm);
        juce::MemoryBlock original;
        pgm.getChildFile ("Folders.xml").loadFileAsData (original);
        juce::MemoryBlock ligacao;
        pgm.getChildFile ("Dados/LIGACAO.DBF").loadFileAsData (ligacao);

        beginTest ("Types of the Config Manager");
        {
            expectEquals ((int) folderTypes().size(), 13);
            expect (folderKindFromType ("A") == FolderKind::random);
            expect (folderKindFromType ("S") == FolderKind::sequential);
            expect (folderKindFromType ("") == FolderKind::generic);
            expectEquals (toDisplayString (FolderKind::generic), juce::String ("Outras"));
            expectEquals (folderTypeInfo (FolderKind::music).defaultIconIndex, 331);
            expectEquals (folderGroupName (folderTypeInfo (FolderKind::timeAnnouncement).group), juce::String ("Comandos"));
        }

        beginTest ("Rendering the same folders keeps the file byte for byte");
        {
            juce::MemoryBlock again;
            expect (folders.renderBytes (folders.folders(), again));
            expect (again == original);
        }

        beginTest ("A changed value touches only its element");
        {
            auto list = folders.folders();
            list[1].title = "Comerciais 2";
            normalizeFolder (list[1], pgm);
            auto before = juce::StringArray::fromLines (folders.render (folders.folders()));
            auto after = juce::StringArray::fromLines (folders.render (list));
            expectEquals (after.size(), before.size());
            int different = 0;
            for (int i = 0; i < before.size(); ++i)
                different += before[i] != after[i] ? 1 : 0;
            expectEquals (different, 2); // Title and ShortcutPathName
        }

        beginTest ("A new folder is appended as the Config Manager writes it");
        {
            auto list = folders.folders();
            auto added = makeNewFolder (FolderKind::random, pgm.getParentDirectory().getChildFile ("Chamadas"), pgm, list, nullptr,
                                        folders.nextId());
            expectEquals (added.title, juce::String ("Chamadas"));
            expectEquals (added.code, juce::String ("CHA"));
            expectEquals (added.type, juce::String ("A"));
            expectEquals (added.iconIndex, 6);
            list.push_back (added);
            auto text = folders.render (list);
            auto count = (int) list.size() - 1;
            juce::String block;
            block << "  <Folder" << count << ">\r\n"
                  << "    <ID>26</ID>\r\n"
                  << "    <Title>Chamadas</Title>\r\n"
                  << "    <Type>A</Type>\r\n"
                  << "    <Target>" << added.target << "</Target>\r\n"
                  << "    <IconLocation>" << added.iconLocation << "</IconLocation>\r\n"
                  << "    <ShortcutArguments>A</ShortcutArguments>\r\n"
                  << "    <ShortcutPathName>" << pgm.getChildFile ("Atalhos\\Chamadas.lnk").getFullPathName() << "</ShortcutPathName>\r\n"
                  << "    <IconIndex>6</IconIndex>\r\n"
                  << "    <Output>-1</Output>\r\n"
                  << "    <TotalFiles>0</TotalFiles>\r\n"
                  << "    <DBFId>CHA</DBFId>\r\n"
                  << "  </Folder" << count << ">\r\n</Folders>";
            expect (text.trimEnd().endsWith (block), text.getLastCharacters (900));
            expect (text.contains ("<Folders>" + juce::String ((int) list.size()) + "</Folders>"));
            juce::MemoryBlock m;
            expect (folders.renderBytes (list, m));
            juce::String error;
            auto reread = FoldersXml::parse (m, error);
            expect (reread.has_value() && reread->folders().size() == list.size());
        }

        beginTest ("Removing a folder renumbers the elements");
        {
            auto list = folders.folders();
            list.erase (list.begin());
            juce::MemoryBlock m;
            expect (folders.renderBytes (list, m));
            juce::String error;
            auto reread = FoldersXml::parse (m, error);
            expect (reread.has_value());
            expectEquals ((int) reread->folders().size(), (int) list.size());
            expectEquals (reread->declaredCount(), (int) list.size());
            expectEquals (reread->folders().front().element, juce::String ("Folder0"));
            expectEquals (reread->folders().front().title, list.front().title);
        }

        beginTest ("DBF editing");
        {
            auto t = table (ligacao);
            auto n = t.recordCount();
            auto r = t.appendRecord();
            expectEquals (r, n);
            expect (t.setString (r, "CODIGO", "NOVO"));
            expect (t.setString (r, "ARQUIVO", L"Chamadas ção.lnk"));
            expect (! t.setString (r, "CODIGO", "CODIGOMUITOLONGO"));
            auto reread = table (t.bytes());
            expectEquals (reread.recordCount(), n + 1);
            expect (reread.hasEofMarker());
            expectEquals (reread.getString (r, "CODIGO"), juce::String ("NOVO"));
            expectEquals (reread.getString (r, "ARQUIVO"), juce::String (L"Chamadas ção.lnk"));
            expectEquals (reread.getString (0, "CODIGO"), t.getString (0, "CODIGO"));
        }

        beginTest ("Code suggestion");
        {
            juce::Random random (7);
            auto t = table (ligacao);
            expectEquals (suggestFolderCode ("Chamadas", folders.folders(), &t, random), juce::String ("CHA"));
            auto taken = suggestFolderCode ("Comunicados", folders.folders(), &t, random); // COM is a folder code
            expect (taken.startsWith ("COM") && taken.length() == 6, taken);
        }

        beginTest ("Plan for a new folder registers its code and updates the indexes");
        {
            auto list = folders.folders();
            auto dir = pgm.getParentDirectory().getChildFile ("Chamadas");
            dir.createDirectory();
            auto t = table (ligacao);
            list.push_back (makeNewFolder (FolderKind::random, dir, pgm, list, &t, folders.nextId()));
            auto plan = planFolderChanges (pgm, folders, list, &ligacao, juce::Time (2026, 8, 27, 10, 5));
            expect (! plan.problems.hasErrors(), plan.problems.toText());
            expect (plan.ligacao.has_value());
            auto updated = table (*plan.ligacao);
            expectEquals (updated.recordCount(), t.recordCount() + 1);
            auto r = recordWithCode (updated, "CHA");
            expect (r >= 0);
            expectEquals (updated.getString (r, "ARQUIVO"), juce::String ("Chamadas.lnk"));
            expectEquals (updated.getString (r, "TIPO"), juce::String ("A"));
            expectEquals (updated.getString (r, "DATAREG"), juce::String ("20260927"));
            expectEquals (updated.getString (r, "HORAREG"), juce::String ("10:05"));
            expect (plan.ligaCod.has_value() && plan.ligaArq.has_value());
            expect (verifyNtx (*plan.ligaCod, updated).consistent);
            expect (verifyNtx (*plan.ligaArq, updated).consistent);
            // The new folder, plus shortcuts missing on disk (Acervo Antigo in the fixture).
            const FolderPlan::Shortcut* created = nullptr;
            for (auto& sc : plan.shortcuts)
                if (sc.file.getFileName() == "Chamadas.lnk")
                    created = &sc;
            expect (created != nullptr && created->info.arguments == "A");
            expectEquals ((int) plan.shortcuts.size(), 2);
        }

        beginTest ("Renaming a folder updates its record in place");
        {
            auto list = folders.folders();
            for (auto& f : list)
                if (f.code == "VH")
                {
                    f.title = "Vinhetas Novas";
                    f.code = "VHN";
                    normalizeFolder (f, pgm);
                }
            auto plan = planFolderChanges (pgm, folders, list, &ligacao, juce::Time::getCurrentTime());
            expect (! plan.problems.hasErrors(), plan.problems.toText());
            auto before = table (ligacao);
            auto after = table (*plan.ligacao);
            expectEquals (after.recordCount(), before.recordCount());
            auto r = recordWithCode (after, "VHN");
            expectEquals (r, recordWithCode (before, "VH"));
            expectEquals (after.getString (r, "ARQUIVO"), juce::String ("Vinhetas Novas.lnk"));
            bool removesOld = false;
            for (auto& f : plan.removed)
                removesOld = removesOld || f.getFileName() == "Vinhetas.lnk";
            expect (removesOld);
        }

        beginTest ("Conflicts are refused");
        {
            auto list = folders.folders();
            list[0].code = "55"; // registered for Padaria Pao Quente.mp3
            auto plan = planFolderChanges (pgm, folders, list, &ligacao, juce::Time::getCurrentTime());
            expect (plan.problems.hasErrors());
            list = folders.folders();
            list[1].title = list[0].title;
            normalizeFolder (list[1], pgm);
            expect (planFolderChanges (pgm, folders, list, &ligacao, juce::Time::getCurrentTime()).problems.hasErrors());
            list = folders.folders();
            list[1].title = "A/B";
            expect (planFolderChanges (pgm, folders, list, &ligacao, juce::Time::getCurrentTime()).problems.hasErrors());
        }

        beginTest ("Shortcuts are written and read back");
        {
            auto lnk = temp.dir.getChildFile ("Teste.lnk");
            juce::String error;
            ShortcutInfo info { pgm.getFullPathName(), "C UDP {PLAY}", "Teste", pgm.getChildFile ("Icones\\Icones5.dll").getFullPathName(), 117 };
            expect (writeShortcut (lnk, info, error), error);
            auto back = readShortcut (lnk);
            expect (back.has_value());
            expectEquals (back->arguments, info.arguments);
            expectEquals (back->description, info.description);
            expectEquals (back->iconIndex, 117);
            expect (back->target.equalsIgnoreCase (info.target), back->target);
        }

        beginTest ("Applying a plan writes every file and keeps a copy");
        {
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            auto list = folders.folders();
            auto dir = pgm.getParentDirectory().getChildFile ("Chamadas");
            dir.createDirectory();
            list.push_back (makeNewFolder (FolderKind::random, dir, pgm, list, nullptr, folders.nextId()));
            list.erase (list.begin() + 4); // Pausa
            auto plan = planFolderChanges (pgm, folders, list, &ligacao, juce::Time::getCurrentTime());
            expect (! plan.problems.hasErrors(), plan.problems.toText());
            juce::String error;
            auto backup = temp.dir.getChildFile ("backup");
            expect (applyFolderPlan (plan, pgm, writer, backup, error), error);
            expect (pgm.getChildFile ("Atalhos/Chamadas.lnk").existsAsFile());
            expect (! pgm.getChildFile ("Atalhos/Pausa.lnk").existsAsFile());
            expect (backup.getChildFile ("Atalhos/Pausa.lnk").existsAsFile());
            expect (backup.getChildFile ("Folders.xml").existsAsFile());
            auto reread = loadFolders (pgm);
            expectEquals ((int) reread.folders().size(), (int) list.size());
            expect (reread.findByCode ("CHA") != nullptr);
            juce::MemoryBlock dbf;
            pgm.getChildFile ("Dados/LIGACAO.DBF").loadFileAsData (dbf);
            expect (recordWithCode (table (dbf), "CHA") >= 0);
            expect (history.list().size() >= 2);
        }
    }
};

static FolderConfigTests folderConfigTests;

} // namespace pc::test
