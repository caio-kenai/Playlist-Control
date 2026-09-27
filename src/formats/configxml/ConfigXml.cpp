#include "formats/configxml/ConfigXml.h"

namespace pc
{

std::optional<ConfigXml> ConfigXml::parse (const juce::MemoryBlock& bytes, juce::String& error)
{
    auto doc = XmlPatchDocument::parse (bytes, error);
    if (! doc.has_value())
        return std::nullopt;
    if (doc->node (doc->root()).name != "Config")
    {
        error = "O elemento raiz não é <Config>.";
        return std::nullopt;
    }
    return ConfigXml (std::move (*doc));
}

std::vector<ConfigEntry> ConfigXml::entries() const
{
    std::vector<ConfigEntry> out;
    std::function<void (int, const juce::String&)> walk = [&] (int index, const juce::String& prefix) {
        for (auto c : doc_.node (index).children)
        {
            auto& n = doc_.node (c);
            auto path = prefix.isEmpty() ? n.name : prefix + "/" + n.name;
            if (! n.children.empty())
            {
                walk (c, path);
                continue;
            }
            ConfigEntry e;
            e.path = path;
            e.value = doc_.value (c);
            e.field = findConfigField (path);
            e.node = c;
            e.line = doc_.lineOf (c);
            out.push_back (e);
        }
    };
    walk (doc_.root(), {});
    return out;
}

std::optional<juce::String> ConfigXml::get (const juce::String& path) const
{
    auto n = doc_.find (path);
    if (n < 0)
        return std::nullopt;
    return doc_.value (n);
}

bool ConfigXml::set (const juce::String& path, const juce::String& value)
{
    auto n = doc_.find (path);
    return n >= 0 && doc_.setValue (n, value);
}

juce::StringArray ConfigXml::outputGroups() const
{
    juce::StringArray groups;
    for (auto c : doc_.node (doc_.root()).children)
        if (doc_.node (c).name.startsWith ("Saidas_"))
            groups.add (doc_.node (c).name);
    return groups;
}

} // namespace pc
