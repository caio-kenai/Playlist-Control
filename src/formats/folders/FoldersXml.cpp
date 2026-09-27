#include "formats/folders/FoldersXml.h"

namespace pc
{

FolderKind folderKindFromType (const juce::String& type)
{
    auto t = type.trim().toUpperCase();
    if (t.isEmpty()) return FolderKind::generic;
    switch (t[0])
    {
        case 'M': return FolderKind::music;
        case '$': return FolderKind::commercial;
        case 'V': return FolderKind::sweeper;
        case 'H': return FolderKind::timeAnnouncement;
        case 'W': return FolderKind::temperature;
        case 'X': return FolderKind::text;
        case 'T': return FolderKind::track;
        case 'L': return FolderKind::voiceTrack;
        case 'P': return FolderKind::pause;
        case 'C': return FolderKind::command;
        default:  return FolderKind::unknown;
    }
}

juce::String toDisplayString (FolderKind kind)
{
    switch (kind)
    {
        case FolderKind::music:            return L"Músicas";
        case FolderKind::commercial:       return "Comerciais";
        case FolderKind::sweeper:          return "Vinhetas";
        case FolderKind::timeAnnouncement: return "Hora certa";
        case FolderKind::temperature:      return "Temperatura";
        case FolderKind::text:             return "Textos";
        case FolderKind::track:            return "Trilhas";
        case FolderKind::voiceTrack:       return L"Locuções";
        case FolderKind::pause:            return "Pausa";
        case FolderKind::command:          return "Comando";
        case FolderKind::generic:          return L"Genérica";
        case FolderKind::unknown:          return "Desconhecido";
    }
    return {};
}

juce::String FolderEntry::commandLine() const
{
    if (kind != FolderKind::command)
        return {};
    auto a = shortcutArguments.trim();
    return a.startsWithIgnoreCase ("C ") ? a.substring (2).trim() : a.substring (1).trim();
}

std::optional<FoldersXml> FoldersXml::parse (const juce::MemoryBlock& bytes, juce::String& error)
{
    auto doc = XmlPatchDocument::parse (bytes, error);
    if (! doc.has_value())
        return std::nullopt;
    auto root = doc->root();
    if (doc->node (root).name != "Folders")
    {
        error = L"O elemento raiz não é <Folders>.";
        return std::nullopt;
    }

    FoldersXml f;
    auto text = [&] (int parent, const char* name) {
        auto c = doc->childNamed (parent, name);
        return c >= 0 ? doc->value (c) : juce::String();
    };

    f.version_ = text (root, "Version");
    auto count = text (root, "Folders");
    f.declaredCount_ = count.isNotEmpty() ? count.getIntValue() : -1;

    auto shared = doc->childNamed (root, "Shared");
    if (shared >= 0)
    {
        f.sharedServer_ = text (shared, "Server");
        for (auto c : doc->node (shared).children)
        {
            auto& n = doc->node (c);
            if (n.name.startsWith ("Folder") && n.name != "Folders" && ! n.children.empty())
                f.shared_.push_back ({ text (c, "Name"), text (c, "Path") });
        }
    }

    for (auto c : doc->node (root).children)
    {
        auto& n = doc->node (c);
        if (! n.name.startsWith ("Folder") || n.name == "Folders" || n.children.empty())
            continue;
        FolderEntry e;
        e.element = n.name;
        e.id = text (c, "ID").getIntValue();
        e.title = text (c, "Title");
        e.type = text (c, "Type");
        e.kind = folderKindFromType (e.type);
        e.target = text (c, "Target");
        e.iconLocation = text (c, "IconLocation");
        e.iconIndex = text (c, "IconIndex").getIntValue();
        e.shortcutArguments = text (c, "ShortcutArguments");
        e.shortcutPath = text (c, "ShortcutPathName");
        auto output = text (c, "Output");
        e.output = output.isNotEmpty() ? output.getIntValue() : -1;
        e.code = text (c, "DBFId");
        e.line = doc->lineOf (c);
        f.folders_.push_back (e);
    }
    return f;
}

const FolderEntry* FoldersXml::findByTitle (const juce::String& title) const
{
    for (auto& f : folders_)
        if (f.title.equalsIgnoreCase (title))
            return &f;
    return nullptr;
}

const FolderEntry* FoldersXml::findByCode (const juce::String& code) const
{
    for (auto& f : folders_)
        if (f.code.isNotEmpty() && f.code.equalsIgnoreCase (code))
            return &f;
    return nullptr;
}

} // namespace pc
