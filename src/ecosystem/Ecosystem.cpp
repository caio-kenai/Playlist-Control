#include "ecosystem/Ecosystem.h"
#include "formats/ini/IniDocument.h"
#include "formats/xml/XmlPatchDocument.h"
#include "storage/FileIO.h"

namespace pc
{

bool samePath (const juce::String& a, const juce::File& b)
{
    if (a.isEmpty() || b == juce::File())
        return false;
    auto normalize = [] (juce::String s) {
        s = s.replace ("\\\\", "\\").replaceCharacter ('/', '\\').trimCharactersAtEnd ("\\").toLowerCase();
        return s;
    };
    return normalize (a) == normalize (b.getFullPathName());
}

bool SyncServiceInfo::managesDate (const Date& date, const Date& today) const
{
    if (! found || ! active || ! writesMaps)
        return false;
    for (int i = 0; i <= daysAhead; ++i)
        if (today.addDays (i) == date)
            return true;
    return false;
}

void readCommercialStation (const juce::String& xml, CommercialInfo& info)
{
    juce::String err;
    auto doc = XmlPatchDocument::parseText (xml, TextEncoding::utf8, err);
    if (! doc.has_value())
        return;
    auto pastas = doc->childNamed (doc->root(), "Pastas");
    auto value = [&] (int parent, const char* name) {
        auto n = doc->childNamed (parent, name);
        // Commercial writes doubled backslashes in paths.
        return n >= 0 ? doc->value (n).replace ("\\\\", "\\") : juce::String();
    };
    info.playlistFolder = value (pastas, "PastaPlaylist");
    info.mapsFolder = value (pastas, "PastaMapas");
    auto full = value (pastas, "ExportaDataCompleta");
    if (full.isEmpty())
        full = value (doc->root(), "ExportaDataCompleta");
    info.exportsFullDate = full == "1";
}

void readSyncServiceConfig (const juce::String& json, SyncServiceInfo& info)
{
    auto v = juce::JSON::parse (json);
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return;
    auto str = [&] (const char* key) { return o->getProperty (key).toString(); };
    info.active = str ("SyncActive") == "1";
    info.writesMaps = str ("SyncMapas") == "1";
    if (str ("DaysLimit").isNotEmpty()) info.daysAhead = juce::jlimit (0, 60, str ("DaysLimit").getIntValue());
    if (str ("SyncTime").isNotEmpty()) info.intervalMinutes = juce::jmax (1, str ("SyncTime").getIntValue());
    info.pgmFolder = str ("pgmFolder");
    info.mapsFolder = str ("MapasFolder");
    info.stationName = str ("StationName");
    info.lastSync = str ("lastSync");
}

EcosystemInfo scanEcosystem (const juce::File&)
{
    EcosystemInfo e;
    for (auto& p : installedPrograms())
    {
        if (! p.publisher.containsIgnoreCase ("Playlist"))
            continue;
        e.programs.push_back (p);

        if (p.displayName.startsWithIgnoreCase ("Commercial") && ! e.commercial.found)
        {
            e.commercial.found = true;
            e.commercial.version = p.version;
            e.commercial.folder = juce::File (p.installLocation);
            juce::MemoryBlock m;
            juce::String err;
            if (readFileShared (e.commercial.folder.getChildFile ("Dados").getChildFile ("Emissora.xml"), m, err))
                readCommercialStation (decodeText (m).text, e.commercial);
        }
        else if (p.displayName.startsWithIgnoreCase ("Sync Service") && ! e.sync.found)
        {
            auto config = juce::File (p.installLocation).getChildFile ("SyncService");
            juce::MemoryBlock m;
            juce::String err;
            if (readFileShared (config, m, err))
            {
                e.sync.found = true;
                e.sync.version = p.version;
                e.sync.configFile = config;
                readSyncServiceConfig (decodeText (m).text, e.sync);
            }
        }
        else if (p.displayName.startsWithIgnoreCase ("Playlist Server"))
        {
            e.playlistServer.found = true;
            e.playlistServer.version = p.version;
        }
        else if (p.displayName.equalsIgnoreCase ("Maker"))
        {
            e.maker.found = true;
            e.maker.version = p.version;
        }
    }

    auto serverIni = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                         .getChildFile ("Playlist Software/Playlist Server/SERVER.INI");
    if (serverIni.existsAsFile())
    {
        juce::MemoryBlock m;
        juce::String err;
        if (readFileShared (serverIni, m, err))
            e.playlistServer.port = IniDocument::fromBytes (m).getOr ("SERVER", "PORT").getIntValue();
    }

    e.services = findServices ({ "Playlist", "SyncService" });
    for (auto& s : e.services)
    {
        if (s.name.equalsIgnoreCase ("PlaylistServer"))
            e.playlistServer.service = s.state;
        if (s.name.containsIgnoreCase ("SyncService"))
            e.sync.service = s.state;
    }
    return e;
}

} // namespace pc
