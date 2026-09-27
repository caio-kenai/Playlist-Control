#pragma once

#include <juce_core/juce_core.h>

namespace pc
{

enum class ConfigType
{
    flag,     // 0 / 1
    integer,
    text,
    secret,   // passwords: never logged, masked in the interface
    readOnly  // shown, never edited (device GUIDs, values the Playlist manages)
};

// One CONFIG.XML element and the option of Ferramentas > Opções it controls.
struct ConfigField
{
    const char* key;        // element name; "Saidas_*/X" for the per-machine outputs group
    const char* group;
    const char* label;
    const char* help;
    ConfigType type;
    int minValue = 0;
    int maxValue = 0;       // 0 = no range check
    const char* unit = "";
    bool confirmed = true;  // false when the key-to-option mapping is inferred from the name only
};

const std::vector<ConfigField>& configSchema();
const ConfigField* findConfigField (const juce::String& key);

// Groups in the order the Playlist shows them.
juce::StringArray configGroups();

} // namespace pc
