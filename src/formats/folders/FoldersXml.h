#pragma once

#include "formats/xml/XmlPatchDocument.h"

namespace pc
{

// Folder types of the Config Manager (Folders.xml <Type>, first token of
// the shortcut arguments).
enum class FolderKind
{
    music,            // M
    commercial,       // $
    sweeper,          // V (vinhetas)
    timeAnnouncement, // H (hora certa)
    temperature,      // W
    text,             // X
    track,            // T (trilhas)
    voiceTrack,       // L (locuções)
    pause,            // P
    command,          // C
    generic,          // empty type (e.g. Institucional)
    unknown
};

FolderKind folderKindFromType (const juce::String& type);
juce::String toDisplayString (FolderKind kind);

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
    juce::String code;      // DBFId: the registered code of the folder
    int line = 0;

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

private:
    std::vector<FolderEntry> folders_;
    std::vector<SharedFolder> shared_;
    juce::String version_, sharedServer_;
    int declaredCount_ = -1;
};

} // namespace pc
