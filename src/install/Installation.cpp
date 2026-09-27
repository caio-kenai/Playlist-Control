#include "install/Installation.h"
#include "ecosystem/Ecosystem.h"
#include "platform/WindowsSystem.h"

namespace pc
{

bool isPlaylistExecutableName (const juce::String& fileName)
{
    auto n = fileName.toLowerCase();
    if (! n.endsWith (".exe") || ! n.startsWith ("playlist"))
        return false;
    auto middle = n.substring (8, n.length() - 4).trim();
    return middle.isEmpty() || middle.containsOnly ("0123456789.");
}

juce::Array<juce::File> playlistExecutables (const juce::File& pgm)
{
    juce::Array<juce::File> out;
    for (auto& f : pgm.findChildFiles (juce::File::findFiles, false, "*.exe"))
        if (isPlaylistExecutableName (f.getFileName()))
            out.add (f);
    return out;
}

juce::String InstallationInfo::summary() const
{
    if (valid)
        return L"Instalação do Playlist Digital em " + pgm.getFullPathName()
             + (playlistVersion.isNotEmpty() ? L" (versão " + playlistVersion + ")" : juce::String());
    juce::StringArray missing;
    for (auto& c : checks)
        if (c.required && ! c.ok)
            missing.add (c.item);
    return pgm.getFullPathName() + L" não parece ser uma pasta pgm do Playlist Digital. Faltando: "
         + missing.joinIntoString (", ") + ".";
}

InstallationInfo inspectInstallation (const juce::File& pgm)
{
    InstallationInfo info;
    info.pgm = pgm;

    auto add = [&] (const juce::String& item, bool ok, bool required, const juce::String& detail) {
        info.checks.push_back ({ item, ok, required, detail });
    };

    if (! pgm.isDirectory())
    {
        add ("Pasta", false, true, L"A pasta não existe.");
        return info;
    }

    auto exes = playlistExecutables (pgm);
    auto main = pgm.getChildFile ("Playlist.exe");
    info.playlistExe = main.existsAsFile() ? main : (exes.isEmpty() ? juce::File() : exes.getFirst());
    add ("Playlist.exe", info.playlistExe.existsAsFile(), true,
         info.playlistExe.existsAsFile() ? info.playlistExe.getFileName() : L"Executável do Playlist Digital não encontrado.");
    if (info.playlistExe.existsAsFile())
        info.playlistVersion = fileVersion (info.playlistExe);

    bool ini = pgm.getChildFile ("PLAYLIST.ini").existsAsFile();
    bool config = pgm.getChildFile ("CONFIG.XML").existsAsFile();
    bool folders = pgm.getChildFile ("Folders.xml").existsAsFile();
    add ("PLAYLIST.ini", ini, false, ini ? L"Encontrado" : L"Ausente (o Playlist usa a configuração padrão).");
    add ("CONFIG.XML", config, false, config ? L"Encontrado" : L"Ausente (criado pelo Playlist ao salvar as opções).");
    add ("Folders.xml", folders, false, folders ? "Encontrado" : "Ausente (criado pelo Config Manager).");
    add (L"Arquivos de configuração", ini || config || folders, true,
         "Pelo menos um de PLAYLIST.ini, CONFIG.XML ou Folders.xml.");

    for (auto* dir : { "Mapas", "Grades", "Dados" })
    {
        bool ok = pgm.getChildFile (dir).isDirectory();
        add (juce::String ("Pasta ") + dir, ok, false, ok ? "Encontrada" : "Ausente");
    }

    info.valid = true;
    for (auto& c : info.checks)
        info.valid = info.valid && (! c.required || c.ok);
    return info;
}

std::vector<InstallationCandidate> findInstallationCandidates()
{
    std::vector<InstallationCandidate> out;
    auto add = [&] (const juce::File& f, const juce::String& source) {
        if (f == juce::File())
            return;
        for (auto& c : out)
            if (c.pgm == f)
                return;
        out.push_back ({ f, source });
    };

    for (auto& p : installedPrograms())
        if (p.displayName.startsWithIgnoreCase ("Playlist Digital") && p.installLocation.isNotEmpty())
            add (juce::File (p.installLocation), "Programas instalados do Windows (" + p.displayName + ")");

    auto eco = scanEcosystem ({});
    if (eco.commercial.found && eco.commercial.playlistFolder.isNotEmpty())
        add (juce::File (eco.commercial.playlistFolder), L"Configuração do Commercial");
    if (eco.sync.found && eco.sync.pgmFolder.isNotEmpty())
        add (juce::File (eco.sync.pgmFolder), L"Configuração do Sync Service");

    for (char drive = 'C'; drive <= 'Z'; ++drive)
    {
        auto f = juce::File (juce::String::charToString (drive) + ":\\Playlist\\pgm");
        if (f.isDirectory())
            add (f, "Unidade " + juce::String::charToString (drive) + ":");
    }
    return out;
}

} // namespace pc
