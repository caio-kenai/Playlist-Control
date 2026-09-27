#include "storage/HistoryStore.h"
#include "storage/FileIO.h"

namespace pc
{

namespace
{
const char* const manifestName = "entry.json";

juce::String makeId (const juce::Time& t)
{
    return t.formatted ("%Y%m%d-%H%M%S") + juce::String::formatted ("-%03d-", t.getMilliseconds())
         + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()).paddedLeft ('0', 8);
}
} // namespace

HistoryStore::HistoryStore (juce::File root) : root_ (std::move (root)) {}

void HistoryStore::writeManifest (const HistoryEntry& e, const juce::String& state)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", e.id);
    o->setProperty ("state", state);
    o->setProperty ("time", e.time.toISO8601 (true));
    o->setProperty ("target", e.target.getFullPathName());
    o->setProperty ("operation", e.operation);
    o->setProperty ("summary", e.summary);
    o->setProperty ("user", e.user);
    o->setProperty ("machine", e.machine);
    o->setProperty ("hadBefore", e.hadBefore);
    o->setProperty ("beforeSize", e.beforeSize);
    o->setProperty ("afterSize", e.afterSize);
    o->setProperty ("beforeSha256", e.beforeSha256);
    o->setProperty ("afterSha256", e.afterSha256);
    o->setProperty ("restoredFrom", e.restoredFrom);
    e.folder.getChildFile (manifestName).replaceWithText (juce::JSON::toString (juce::var (o)), false, false, "\n");
}

bool HistoryStore::loadEntry (const juce::File& folder, HistoryEntry& e, juce::String& state)
{
    auto json = juce::JSON::parse (folder.getChildFile (manifestName));
    auto* o = json.getDynamicObject();
    if (o == nullptr)
        return false;
    e.folder = folder;
    e.id = o->getProperty ("id").toString();
    state = o->getProperty ("state").toString();
    e.time = juce::Time::fromISO8601 (o->getProperty ("time").toString());
    e.target = juce::File (o->getProperty ("target").toString());
    e.operation = o->getProperty ("operation").toString();
    e.summary = o->getProperty ("summary").toString();
    e.user = o->getProperty ("user").toString();
    e.machine = o->getProperty ("machine").toString();
    e.hadBefore = (bool) o->getProperty ("hadBefore");
    e.beforeSize = (juce::int64) o->getProperty ("beforeSize");
    e.afterSize = (juce::int64) o->getProperty ("afterSize");
    e.beforeSha256 = o->getProperty ("beforeSha256").toString();
    e.afterSha256 = o->getProperty ("afterSha256").toString();
    e.restoredFrom = o->getProperty ("restoredFrom").toString();
    return e.id.isNotEmpty();
}

juce::String HistoryStore::begin (HistoryEntry& e, const juce::MemoryBlock* before, const juce::MemoryBlock& after)
{
    e.time = juce::Time::getCurrentTime();
    e.id = makeId (e.time);
    e.user = juce::SystemStats::getLogonName();
    e.machine = juce::SystemStats::getComputerName();
    e.hadBefore = before != nullptr;
    e.beforeSize = before != nullptr ? (juce::int64) before->getSize() : 0;
    e.beforeSha256 = before != nullptr ? sha256Hex (*before) : juce::String();
    e.afterSize = (juce::int64) after.getSize();
    e.afterSha256 = sha256Hex (after);
    e.folder = root_.getChildFile (e.time.formatted ("%Y-%m")).getChildFile (e.id);

    if (! e.folder.createDirectory())
        return L"Não foi possível criar a pasta de histórico " + e.folder.getFullPathName() + ".";
    if (before != nullptr && ! e.beforeFile().replaceWithData (before->getData(), before->getSize()))
        return L"Não foi possível salvar a cópia de segurança em " + e.beforeFile().getFullPathName() + ".";
    if (! e.afterFile().replaceWithData (after.getData(), after.getSize()))
        return L"Não foi possível salvar o novo conteúdo no histórico.";

    // The backup must be readable before the original is touched.
    if (before != nullptr)
    {
        juce::MemoryBlock check;
        if (! e.beforeFile().loadFileAsData (check) || check != *before)
            return L"A cópia de segurança não pôde ser verificada.";
    }
    writeManifest (e, "pending");
    return {};
}

void HistoryStore::commit (HistoryEntry& e)
{
    writeManifest (e, "done");
}

void HistoryStore::abandon (HistoryEntry& e)
{
    if (e.folder.isDirectory())
        e.folder.deleteRecursively();
}

std::vector<HistoryEntry> HistoryStore::list (int maxEntries) const
{
    std::vector<HistoryEntry> result;
    if (! root_.isDirectory())
        return result;

    auto months = root_.findChildFiles (juce::File::findDirectories, false);
    std::sort (months.begin(), months.end(), [] (auto& a, auto& b) { return a.getFileName() > b.getFileName(); });
    for (auto& month : months)
    {
        auto folders = month.findChildFiles (juce::File::findDirectories, false);
        std::sort (folders.begin(), folders.end(), [] (auto& a, auto& b) { return a.getFileName() > b.getFileName(); });
        for (auto& f : folders)
        {
            HistoryEntry e;
            juce::String state;
            if (loadEntry (f, e, state) && state == "done")
            {
                result.push_back (e);
                if ((int) result.size() >= maxEntries)
                    return result;
            }
        }
    }
    return result;
}

std::vector<HistoryEntry> HistoryStore::listFor (const juce::File& target, int maxEntries) const
{
    std::vector<HistoryEntry> result;
    for (auto& e : list (100000))
    {
        if (e.target == target)
            result.push_back (e);
        if ((int) result.size() >= maxEntries)
            break;
    }
    return result;
}

bool HistoryStore::readBefore (const HistoryEntry& e, juce::MemoryBlock& out) const
{
    return e.hadBefore && e.beforeFile().loadFileAsData (out) && sha256Hex (out) == e.beforeSha256;
}

bool HistoryStore::readAfter (const HistoryEntry& e, juce::MemoryBlock& out) const
{
    return e.afterFile().loadFileAsData (out) && sha256Hex (out) == e.afterSha256;
}

int HistoryStore::prune (int keepDays)
{
    int removed = 0;
    if (! root_.isDirectory())
        return 0;
    auto limit = juce::Time::getCurrentTime() - juce::RelativeTime::days (keepDays);
    auto stale = juce::Time::getCurrentTime() - juce::RelativeTime::hours (1);
    for (auto& month : root_.findChildFiles (juce::File::findDirectories, false))
    {
        for (auto& f : month.findChildFiles (juce::File::findDirectories, false))
        {
            HistoryEntry e;
            juce::String state;
            bool ok = loadEntry (f, e, state);
            bool remove = ok ? ((state == "done" && e.time < limit) || (state != "done" && e.time < stale))
                             : f.getLastModificationTime() < stale;
            if (remove && f.deleteRecursively())
                ++removed;
        }
        if (month.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
            month.deleteFile();
    }
    return removed;
}

} // namespace pc
