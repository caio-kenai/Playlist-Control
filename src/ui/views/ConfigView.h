#pragma once

#include "ui/View.h"

namespace pc::ui
{

// CONFIG.XML: options of Ferramentas > Opções grouped as in the Playlist.
// Editing requires the Playlist to be closed, because it rewrites the file.
class ConfigView : public View, private juce::ListBoxModel
{
public:
    explicit ConfigView (AppContext& context);
    ~ConfigView() override;

    juce::String title() const override { return L"Opções do Playlist Digital"; }
    juce::String subtitle() const override { return L"CONFIG.XML — Ferramentas > Opções > Configurações e Inserções"; }
    void refresh() override;
    bool hasUnsavedChanges() const override { return ! changes_.empty(); }
    void save() override;
    void discard() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class FieldRow;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int row) override;

    void load();
    void showGroup (const juce::String& group);
    bool editable() const;
    void valueChanged (const ConfigEntry& entry, const juce::String& value);
    void updateHeader();

    FileSession session_;
    std::optional<ConfigXml> config_;
    std::vector<ConfigEntry> entries_;
    juce::StringArray groups_;
    std::map<juce::String, juce::String> changes_; // path -> new value
    juce::String currentGroup_;

    juce::ListBox groupList_;
    juce::Viewport viewport_;
    juce::Component rowsHolder_;
    juce::OwnedArray<FieldRow> rows_;
    juce::TextButton saveButton_ { "Salvar" }, discardButton_ { "Descartar" };
    juce::Label fileTitle_, fileInfo_;
    Banner banner_;
};

} // namespace pc::ui
