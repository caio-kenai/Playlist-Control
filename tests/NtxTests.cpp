#include "TestUtils.h"
#include "formats/ntx/NtxIndex.h"

namespace pc::test
{

class NtxTests : public juce::UnitTest
{
public:
    NtxTests() : juce::UnitTest ("NTX indexes", "formats") {}

    void runTest() override
    {
        juce::String err;
        auto ligacao = DbfTable::parse (fixture ("installation/pgm/Dados/LIGACAO.DBF"), err);
        auto comprove = DbfTable::parse (fixture ("installation/pgm/Dados/COMPROVE.DBF"), err);

        beginTest ("Key expressions");
        {
            std::string key;
            expect (evaluateNtxKey ("CODIGO", *ligacao, 8, key));
            expect (key == std::string ("55").append (10, ' '));
            expect (evaluateNtxKey ("UPPER(ARQUIVO)", *ligacao, 6, key));
            expect (key.rfind ("ELEI\xC7\xD5" "ES.LNK", 0) == 0, juce::String (key.substr (0, 12)));
            expect (evaluateNtxKey ("CODIGO+DTOS(DATA)+BLOCO", *comprove, 0, key));
            expect (key.substr (0, 14) == std::string ("55").append (10, ' ') + "20");
            expect (evaluateNtxKey ("DESCEND(DTOS(DATA))", *comprove, 0, key));
            expectEquals ((int) (unsigned char) key[0], 256 - '2');
            expect (! evaluateNtxKey ("SUBSTR(CODIGO,1,3)", *ligacao, 0, key));
            expect (upper1252 (0xE7) == 0xC7 && upper1252 ('a') == 'A' && upper1252 (0xF7) == 0xF7);
        }

        beginTest ("Built indexes read back and match the table");
        for (auto [expr, keySize, table] : { std::tuple<const char*, int, const DbfTable*> { "CODIGO", 12, &*ligacao },
                                              { "UPPER(ARQUIVO)", 250, &*ligacao },
                                              { "CODIGO+DTOS(DATA)+BLOCO", 25, &*comprove },
                                              { "UPPER(ARQUIVO)+DESCEND(DTOS(DATA))+DESCEND(HORAFIM)", 116, &*comprove } })
        {
            NtxHeader h;
            h.signature = 6;
            h.keySize = keySize;
            h.itemSize = keySize + 8;
            h.maxItems = (1024 - 4) / (h.itemSize + 2) - 1;
            h.halfPage = h.maxItems / 2;
            h.keyExpression = expr;
            std::vector<NtxEntry> entries;
            expect (expectedNtxEntries (h, *table, entries, err), err);
            auto bytes = buildNtx (h, entries);
            auto read = readNtx (bytes);
            expect (read.problem.isEmpty(), read.problem);
            expect (read.entries == entries, expr);
            auto v = verifyNtx (bytes, *table);
            expect (v.consistent, v.summary);
        }

        beginTest ("Multi-level tree with small pages");
        {
            // Two keys per page forces several levels, like LIGA_ARQ.NTX.
            NtxHeader h;
            h.signature = 6;
            h.keySize = 250;
            h.itemSize = 258;
            h.maxItems = 2;
            h.halfPage = 1;
            h.keyExpression = "UPPER(ARQUIVO)";
            std::vector<NtxEntry> entries;
            for (int n = 1; n <= 200; ++n)
            {
                entries.clear();
                for (int i = 0; i < n; ++i)
                    entries.push_back ({ juce::String::formatted ("K%03d", i).toStdString().append (246, ' '), (juce::uint32) i + 1 });
                auto read = readNtx (buildNtx (h, entries));
                expect (read.problem.isEmpty(), juce::String (n) + ": " + read.problem);
                expect (read.entries == entries, "entries differ for " + juce::String (n));
            }
        }

        beginTest ("Damaged or stale indexes are reported");
        {
            expect (verifyNtx (fixture ("installation/pgm/Indices/LIGA_COD.NTX"), *ligacao).consistent);
            expect (verifyNtx (fixture ("installation/pgm/Indices/COMPROVE-C.NTX"), *comprove).consistent);
            expect (verifyNtx (fixture ("installation/pgm/Indices/COMPROVE-A.NTX"), *comprove).consistent);
            auto stale = fixture ("installation/pgm/Indices/LIGA_ARQ.NTX"); // empty root page on purpose
            auto v = verifyNtx (stale, *ligacao);
            expect (v.readable && ! v.consistent, v.summary);
            juce::MemoryBlock broken (stale);
            static_cast<juce::uint8*> (broken.getData())[4] = 0x10; // root outside the file
            expect (! verifyNtx (broken, *ligacao).readable);
        }
    }
};

static NtxTests ntxTests;

} // namespace pc::test
