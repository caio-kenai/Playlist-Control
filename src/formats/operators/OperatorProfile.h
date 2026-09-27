#pragma once

#include "formats/xml/XmlPatchDocument.h"

namespace pc
{

// Permission value in Operadores\<nome>\Config.xml. The template operator
// ("Padrão", TipoOperador = 1) holds the defaults of Ferramentas > Opções >
// Geral with 0/1; the other operators use -1 for "Padrão" (inherit).
enum class Permission
{
    inherit,
    yes,
    no,
    other
};

Permission parsePermission (const juce::String& value);

struct PermissionValue
{
    juce::String group; // Geral, Blocos, BlocoComercial, ...
    juce::String key;
    juce::String raw;
    Permission permission = Permission::other;
};

struct OperatorProfile
{
    juce::String folderName;
    juce::String id;
    juce::String name;
    bool isTemplate = false; // "Padrão"
    bool admin = false;
    bool removed = false;
    bool hasPassword = false; // only whether a password exists is kept
    std::vector<PermissionValue> permissions;
    juce::StringArray visibleFolders;
    juce::StringArray hiddenFolders;

    const PermissionValue* find (const juce::String& group, const juce::String& key) const;
};

std::optional<OperatorProfile> parseOperatorProfile (const juce::MemoryBlock& bytes, const juce::String& folderName,
                                                     juce::String& error);

// Human label for permission keys documented in the manual.
juce::String permissionLabel (const juce::String& group, const juce::String& key);

} // namespace pc
