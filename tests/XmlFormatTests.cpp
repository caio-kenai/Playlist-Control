#include "TestUtils.h"
#include "formats/configxml/ConfigXml.h"
#include "formats/folders/FoldersXml.h"
#include "formats/operators/OperatorProfile.h"

namespace pc::test
{

class XmlFormatTests : public juce::UnitTest
{
public:
    XmlFormatTests() : juce::UnitTest ("CONFIG.XML, Folders.xml and operators", "formats") {}

    static juce::MemoryBlock pgmFile (const char* rel)
    {
        return fixture (juce::String ("installation/pgm/") + juce::String (juce::CharPointer_UTF8 (rel)));
    }

    void runTest() override
    {
        auto configBytes = pgmFile ("CONFIG.XML");

        beginTest ("CONFIG.XML round trip and values");
        {
            juce::String err;
            auto cfg = ConfigXml::parse (configBytes, err);
            expect (cfg.has_value(), err);
            juce::MemoryBlock out;
            cfg->toBytes (out);
            expect (out == configBytes);
            expectEquals (cfg->get ("nPortaDados").value_or (""), juce::String ("3030"));
            expectEquals (cfg->get ("sFtpServer").value_or ("x"), juce::String());
            expectEquals (cfg->get ("sRDSDefaultText").value_or (""), juce::String ("Radio & Cia"));
            expectEquals (cfg->get ("Saidas_ESTUDIO/Programacao").value_or (""), juce::String ("Alto-falantes (Placa Principal)"));
            expect (cfg->outputGroups().contains ("Saidas_ESTUDIO"));
        }

        beginTest ("CONFIG.XML entries are classified by the schema");
        {
            juce::String err;
            auto cfg = ConfigXml::parse (configBytes, err);
            int unknown = 0;
            for (auto& e : cfg->entries())
            {
                if (e.path == "Saidas_ESTUDIO/Programacao")
                    expect (e.field != nullptr && ! e.editable());
                if (e.path == "sVLCPwd")
                    expect (e.field != nullptr && e.field->type == ConfigType::secret);
                if (e.path == "novaChaveDesconhecida")
                {
                    expect (e.field == nullptr && ! e.editable());
                    ++unknown;
                }
                if (e.path == "nPercComprovacao")
                    expect (e.editable() && e.field->maxValue == 100);
            }
            expectEquals (unknown, 1);
        }

        beginTest ("CONFIG.XML patching changes only the element content");
        {
            juce::String err;
            auto cfg = ConfigXml::parse (configBytes, err);
            auto original = decodeText (configBytes).text;

            expect (cfg->set ("nPercComprovacao", "60"));
            expect (cfg->set ("sFtpServer", "ftp.exemplo.local"));
            expect (cfg->set ("sRDSDefaultText", "A < B & C"));
            expect (cfg->set ("sVLCAddress", ""));
            auto text = cfg->document().text();
            auto expected = original.replace ("<nPercComprovacao>50<", "<nPercComprovacao>60<")
                                .replace ("<sFtpServer>\r\n\t</sFtpServer>", "<sFtpServer>ftp.exemplo.local</sFtpServer>")
                                .replace ("Radio &amp; Cia", "A &lt; B &amp; C")
                                .replace ("<sVLCAddress>localhost:8080</sVLCAddress>", "<sVLCAddress>\r\n\t</sVLCAddress>");
            expectEquals (text, expected);

            // Empty value inside the machine group uses its deeper indentation.
            expect (cfg->set ("Saidas_ESTUDIO/UrlSource", ""));
            expect (cfg->document().text().contains ("<UrlSource>\r\n\t\t</UrlSource>"));

            // Setting an empty value that was already empty is a no-op.
            auto before = cfg->document().text();
            expect (cfg->set ("sPlaylistServerPassword", ""));
            expectEquals (cfg->document().text(), before);

            // Groups cannot be overwritten as a value.
            expect (! cfg->set ("Saidas_ESTUDIO", "x"));
        }

        beginTest ("Malformed XML is rejected with a line number");
        {
            juce::String err;
            expect (! XmlPatchDocument::parse (bytes ("<Config>\r\n<a>1</b>\r\n</Config>"), err).has_value());
            expect (err.contains ("linha 2"), err);
            expect (! XmlPatchDocument::parse (bytes ("<Config><a>"), err).has_value());
            expect (! ConfigXml::parse (bytes ("<Other/>"), err).has_value());
        }

        beginTest ("Folders.xml");
        {
            juce::String err;
            auto bytes = pgmFile ("Folders.xml");
            auto f = FoldersXml::parse (bytes, err);
            expect (f.has_value(), err);
            expectEquals ((int) f->folders().size(), 8);
            expectEquals (f->declaredCount(), 8);
            expectEquals (f->version(), juce::String ("1.2"));
            expectEquals (f->sharedServer(), juce::String ("ESTUDIO"));
            auto* com = f->findByCode ("com");
            expect (com != nullptr && com->kind == FolderKind::commercial);
            auto* stream = f->findByTitle ("Streaming");
            expect (stream != nullptr && stream->kind == FolderKind::command);
            expectEquals (stream->commandLine(), juce::String ("URL https://stream.exemplo.local/radio"));
            expect (f->findByTitle (juce::String (juce::CharPointer_UTF8 ("Elei\xc3\xa7\xc3\xb5" "es"))) != nullptr);
            expect (folderKindFromType ("") == FolderKind::generic);
            expect (folderKindFromType ("W") == FolderKind::temperature);
        }

        beginTest ("Operator profiles");
        {
            juce::String err;
            auto padrao = parseOperatorProfile (pgmFile ("Operadores/Padr\xc3\xa3o/Config.xml"), "Padrao", err);
            expect (padrao.has_value(), err);
            expect (padrao->isTemplate && ! padrao->admin && ! padrao->hasPassword);
            auto ana = parseOperatorProfile (pgmFile ("Operadores/Ana/Config.xml"), "Ana", err);
            expect (ana.has_value(), err);
            expect (! ana->isTemplate && ana->admin && ana->hasPassword);
            expect (ana->find ("Geral", "InsAdd")->permission == Permission::inherit);
            expect (ana->find ("Geral", "InsDel")->permission == Permission::no);
            expect (ana->visibleFolders.contains ("Musicas"));
            expect (ana->hiddenFolders.contains ("Comerciais"));
            expectEquals (permissionLabel ("Geral", "InsAdd"), juce::String (juce::CharPointer_UTF8 ("Adiciona inser\xc3\xa7\xc3\xb5" "es")));
            expectEquals (permissionLabel ("InsMus", "bMusMoveEntreBlocos"), juce::String ("Move entre blocos"));
        }
    }
};

static XmlFormatTests xmlFormatTests;

} // namespace pc::test
