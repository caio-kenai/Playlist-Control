#include "TestUtils.h"
#include "formats/dbf/DbfTable.h"
#include "formats/montagem/MontagemFile.h"
#include "formats/ntx/NtxHeader.h"

namespace pc::test
{

class DataFormatTests : public juce::UnitTest
{
public:
    DataFormatTests() : juce::UnitTest ("DBF, NTX and montagem", "formats") {}

    void runTest() override
    {
        beginTest ("LIGACAO.DBF");
        {
            juce::String err;
            auto t = DbfTable::parse (fixture ("installation/pgm/Dados/LIGACAO.DBF"), err);
            expect (t.has_value(), err);
            expectEquals (t->recordCount(), 14);
            expectEquals (t->recordLength(), 496);
            expectEquals (t->headerLength(), 545);
            expect (t->hasEofMarker() && ! t->truncated() && ! t->hasMemo());
            expectEquals ((int) t->fields().size(), 16);
            expectEquals (t->getString (0, "CODIGO"), juce::String ("MUSIC"));
            expectEquals (t->getString (6, "ARQUIVO"), juce::String (juce::CharPointer_UTF8 ("Elei\xc3\xa7\xc3\xb5" "es.lnk")));
            auto fim = t->getDate (9, "DATAFIM");
            expect (fim.has_value() && *fim == Date { 2026, 1, 31 });
            expect (! t->getDate (0, "DATAFIM").has_value());
            expect (t->isDeleted (13) && ! t->isDeleted (12));
            expectEquals (t->lastUpdate().year, 2026);
        }

        beginTest ("Truncated and invalid DBF");
        {
            auto bytes = fixture ("installation/pgm/Dados/LIGACAO.DBF");
            juce::MemoryBlock cut (bytes.getData(), bytes.getSize() - 600);
            juce::String err;
            auto t = DbfTable::parse (cut, err);
            expect (t.has_value(), err);
            expect (t->truncated());
            expectEquals (t->recordCount(), 12);
            expect (! DbfTable::parse (pc::test::bytes ("not a dbf file, clearly not one at all"), err).has_value());
        }

        beginTest ("NTX headers");
        {
            auto cod = NtxHeader::parse (fixture ("installation/pgm/Indices/LIGA_COD.NTX"));
            expect (cod.valid, cod.problem);
            expectEquals (cod.keyExpression, juce::String ("CODIGO"));
            expectEquals (cod.keySize, 12);
            expectEquals (cod.itemSize, 20);
            auto a = NtxHeader::parse (fixture ("installation/pgm/Indices/COMPROVE-A.NTX"));
            expect (a.valid);
            expectEquals (a.version, 2);
            expect (a.keyExpression.contains ("DESCEND"));
            expect (! NtxHeader::parse (pc::test::bytes ("short")).valid);
        }

        beginTest ("Montagem and merge files");
        {
            auto m = MontagemFile::parse (fixture ("installation/pgm/Montagem/30-09-2026.TXT"));
            expectEquals ((int) m.entries.size(), 3);
            expect (m.invalidLines.empty());
            expect (m.entries[2].position == -1 && m.entries[2].type == 'M');
            expectEquals (m.entries[2].file, juce::String (juce::CharPointer_UTF8 ("Banda Azul - Cora\xc3\xa7\xc3\xa3o.mp3")));

            auto merge = MontagemFile::parse (fixture ("installation/pgm/Montagem/01-10-2026 - POLITICO.merge"));
            expectEquals ((int) merge.entries.size(), 2);
            expectEquals (merge.entries[0].folder, juce::String ("Eleicoes"));

            auto bad = MontagemFile::parse (pc::test::bytes ("13:20 C, x, \"A\", \"b.mp3\"\r\nlixo\r\n14:00 C, 1, \"A\"\r\n"));
            expectEquals ((int) bad.invalidLines.size(), 3);
        }
    }
};

static DataFormatTests dataFormatTests;

} // namespace pc::test
