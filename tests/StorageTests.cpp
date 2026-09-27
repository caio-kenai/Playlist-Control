#include "TestUtils.h"
#include "platform/WinInclude.h"
#include "storage/SafeWriter.h"

namespace pc::test
{

class StorageTests : public juce::UnitTest
{
public:
    StorageTests() : juce::UnitTest ("SafeWriter and history", "storage") {}

    void runTest() override
    {
        beginTest ("Writes with backup and history");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            auto target = temp.dir.getChildFile ("Mapa.txt");
            target.replaceWithData ("00:00 VH\r\n", 10);
            auto base = FileSnapshot::take (target);

            WriteRequest req;
            req.target = target;
            req.content = bytes ("00:00 VH, 55\r\n");
            req.operation = "Editar mapa";
            req.summary = "Bloco 00:00";
            req.expectedBase = base;
            auto r = writer.write (req);

            expect (r.status == WriteStatus::written, r.message);
            juce::MemoryBlock now;
            target.loadFileAsData (now);
            expect (now == req.content);

            auto entries = history.list();
            expectEquals ((int) entries.size(), 1);
            juce::MemoryBlock before;
            expect (history.readBefore (entries[0], before));
            expect (before == bytes ("00:00 VH\r\n"));
            expect (entries[0].operation == "Editar mapa");
            expect (entries[0].target == target);

            // No temporary files left behind.
            expectEquals (temp.dir.getNumberOfChildFiles (juce::File::findFiles, "*.pctmp-*"), 0);
        }

        beginTest ("Refuses to overwrite an external change");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            auto target = temp.dir.getChildFile ("Grade.txt");
            target.replaceWithText ("00:00 A");
            auto base = FileSnapshot::take (target);
            target.replaceWithText ("00:00 B"); // e.g. the Sync Service rewrote it

            WriteRequest req;
            req.target = target;
            req.content = bytes ("00:00 C");
            req.expectedBase = base;
            auto r = writer.write (req);
            expect (r.status == WriteStatus::conflict);
            expect (target.loadFileAsString() == "00:00 B");
            expect (history.list().empty());
        }

        beginTest ("Verification failure leaves the file untouched");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            auto target = temp.dir.getChildFile ("PLAYLIST.ini");
            target.replaceWithText ("[BLOCO COMERCIAL]");
            WriteRequest req;
            req.target = target;
            req.content = bytes ("broken");
            req.verify = [] (const juce::MemoryBlock&) { return juce::String ("invalid"); };
            auto r = writer.write (req);
            expect (r.status == WriteStatus::verifyFailed);
            expect (target.loadFileAsString() == "[BLOCO COMERCIAL]");
        }

        beginTest ("Read-only mode never writes");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            writer.setReadOnly (true);
            auto target = temp.dir.getChildFile ("x.txt");
            WriteRequest req;
            req.target = target;
            req.content = bytes ("x");
            expect (writer.write (req).status == WriteStatus::readOnly);
            expect (! target.exists());
        }

        beginTest ("Identical content is not rewritten");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            auto target = temp.dir.getChildFile ("x.txt");
            target.replaceWithText ("same");
            WriteRequest req;
            req.target = target;
            req.content = bytes ("same");
            expect (writer.write (req).status == WriteStatus::unchanged);
            expect (history.list().empty());
        }

        beginTest ("Creates a new file");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            auto target = temp.dir.getChildFile ("Relogio.txt");
            WriteRequest req;
            req.target = target;
            req.content = bytes ("00:00\r\n");
            req.expectedBase = FileSnapshot::take (target);
            auto r = writer.write (req);
            expect (r.status == WriteStatus::written, r.message);
            expect (target.existsAsFile());
            expect (! history.list()[0].hadBefore);
        }

        beginTest ("File held open without delete sharing reports a clear error");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            writer.setRetryWindow (300);
            auto target = temp.dir.getChildFile ("LIGACAO.DBF");
            target.replaceWithText ("old");
            HANDLE h = CreateFileW (target.getFullPathName().toWideCharPointer(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            expect (h != INVALID_HANDLE_VALUE);
            WriteRequest req;
            req.target = target;
            req.content = bytes ("new");
            auto r = writer.write (req);
            CloseHandle (h);
            expect (r.status == WriteStatus::ioError);
            expect (r.message.contains ("em uso"), r.message);
            expect (target.loadFileAsString() == "old");
            expect (history.list().empty(), "failed write must not leave history");
            expectEquals (temp.dir.getNumberOfChildFiles (juce::File::findFiles, "*.pctmp-*"), 0);
        }

        beginTest ("Reads files other programs keep open for writing");
        {
            TempDir temp;
            auto target = temp.dir.getChildFile ("COMPROVE.DBF");
            target.replaceWithText ("data");
            HANDLE h = CreateFileW (target.getFullPathName().toWideCharPointer(), GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            juce::MemoryBlock m;
            juce::String err;
            expect (readFileShared (target, m, err), err);
            CloseHandle (h);
            expect (m == bytes ("data"));
        }

        beginTest ("History prune keeps recent entries");
        {
            TempDir temp;
            HistoryStore history (temp.dir.getChildFile ("history"));
            SafeWriter writer (history);
            auto target = temp.dir.getChildFile ("a.txt");
            WriteRequest req;
            req.target = target;
            req.content = bytes ("1");
            writer.write (req);
            expectEquals (history.prune (30), 0);
            expectEquals ((int) history.list().size(), 1);
        }
    }
};

static StorageTests storageTests;

} // namespace pc::test
