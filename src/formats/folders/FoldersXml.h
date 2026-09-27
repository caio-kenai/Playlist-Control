#pragma once

#include "formats/xml/XmlPatchDocument.h"

namespace pc
{

// Folder types of the Config Manager (Folders.xml <Type>, first token of
// the shortcut arguments), in the order of its "Tipo" list.
enum class FolderKind
{
    music,            // M
    commercial,       // $
    sweeper,          // V (vinhetas)
    voiceTrack,       // L (locuções)
    text,             // X
    track,            // T (trilhas)
    timeAnnouncement, // H (hora certa)
    pause,            // P
    command,          // C
    random,           // A (aleatórias)
    sequential,       // S (sequenciais)
    temperature,      // W
    generic,          // empty type ("Outras", e.g. Institucional)
    unknown
};

FolderKind folderKindFromType (const juce::String& type);
juce::String toDisplayString (FolderKind kind);

// How the Config Manager presents and creates each type.
struct FolderTypeInfo
{
    FolderKind kind;
    const char* letter;      // <Type> and shortcut arguments
    const char* name;        // UTF-8, as in the "Tipo" list
    const char* description; // UTF-8, text of the "Nova pasta" dialog
    int defaultIconIndex;    // in pgm\Icones\Icones5.dll
    int group;               // 0 Gerais, 1 Comandos, 2 Vinhetas, 3 Músicas
    bool needsDirectory;     // false for pause and command (they point to pgm)
};

// The thirteen types, in the order of the Config Manager.
const std::vector<FolderTypeInfo>& folderTypes();
const FolderTypeInfo& folderTypeInfo (FolderKind kind);
juce::String folderGroupName (int group);

struct FolderEntry
{
    juce::String element;   // Folder0, Folder1...
    int id = 0;
    juce::String title;
    juce::String type;
    FolderKind kind = FolderKind::unknown;
    juce::String target;
    juce::String iconLocation;
    int iconIndex = 0;
    juce::String shortcutArguments;
    juce::String shortcutPath;
    int output = -1;
    int totalFiles = 0;
    juce::String code;      // DBFId: the registered code of the folder
    int line = 0;
    int node = -1;          // element in the parsed document; -1 for a new folder

    // "UDP {PLAY}", "URL https://...", "COM4: P" for command folders.
    juce::String commandLine() const;
};

struct SharedFolder
{
    juce::String name;
    juce::String path;
};

// Folders.xml written by the Config Manager.
class FoldersXml
{
public:
    static std::optional<FoldersXml> parse (const juce::MemoryBlock& bytes, juce::String& error);

    const std::vector<FolderEntry>& folders() const noexcept { return folders_; }
    const std::vector<SharedFolder>& shared() const noexcept { return shared_; }
    juce::String version() const { return version_; }
    int declaredCount() const noexcept { return declaredCount_; }
    juce::String sharedServer() const { return sharedServer_; }

    const FolderEntry* findByTitle (const juce::String& title) const;
    const FolderEntry* findByCode (const juce::String& code) const;

    // Next free <ID>, as the Config Manager numbers new folders.
    int nextId() const;

    // The file with another list of folders, written the way the Config
    // Manager writes it: folders that did not change keep their text, changed
    // values are replaced in place, new folders (node == -1) are appended,
    // removed ones are dropped, the <FolderN> elements are renumbered and the
    // count is updated. Everything else (declaration, <Shared>, spacing) stays.
    juce::String render (const std::vector<FolderEntry>& folders) const;
    bool renderBytes (const std::vector<FolderEntry>& folders, juce::MemoryBlock& out) const;

private:
    std::optional<XmlPatchDocument> doc_;
    std::vector<FolderEntry> folders_;
    std::vector<SharedFolder> shared_;
    juce::String version_, sharedServer_;
    int declaredCount_ = -1;
};

} // namespace pc
