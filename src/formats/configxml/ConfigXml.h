#pragma once

#include "formats/configxml/ConfigSchema.h"
#include "formats/xml/XmlPatchDocument.h"

namespace pc
{

struct ConfigEntry
{
    juce::String path;   // "nPortaDados" or "Saidas_DEV07/Programacao"
    juce::String value;
    const ConfigField* field = nullptr; // null when not documented
    int node = -1;
    int line = 0;

    bool editable() const noexcept
    {
        return field != nullptr && field->type != ConfigType::readOnly;
    }
    juce::String group() const { return field != nullptr ? juce::String (field->group) : juce::String ("Outras configurações"); }
    juce::String label() const { return field != nullptr ? juce::String (field->label) : path; }
};

// CONFIG.XML of the Playlist (Ferramentas > Opções > Configurações e Inserções).
class ConfigXml
{
public:
    static std::optional<ConfigXml> parse (const juce::MemoryBlock& bytes, juce::String& error);

    const XmlPatchDocument& document() const noexcept { return doc_; }
    bool toBytes (juce::MemoryBlock& out) const { return doc_.toBytes (out); }

    std::vector<ConfigEntry> entries() const;
    std::optional<juce::String> get (const juce::String& path) const;
    bool set (const juce::String& path, const juce::String& value);

    // Machine-specific group, e.g. "Saidas_DEV07".
    juce::StringArray outputGroups() const;

private:
    explicit ConfigXml (XmlPatchDocument doc) : doc_ (std::move (doc)) {}
    XmlPatchDocument doc_;
};

} // namespace pc
