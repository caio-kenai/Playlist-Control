#pragma once

#include "ui/View.h"

namespace pc::ui
{

// PLAYLIST.ini: how the Playlist finds maps, grades and clocks, plus
// affiliates and beep. Sections it does not model are kept untouched.
class PlaylistIniView : public View
{
public:
    explicit PlaylistIniView (AppContext& context);
    ~PlaylistIniView() override;

    juce::String title() const override { return "Leitura de mapas e grades"; }
    juce::String subtitle() const override { return L"PLAYLIST.ini — onde o Playlist Digital procura a programação de cada dia"; }
    void refresh() override;
    bool hasUnsavedChanges() const override { return dirty_; }
    void save() override;
    void discard() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct SourceEditor;

    void load();
    void pushToControls();
    void edited (const juce::String& what);
    void updatePreview();
    void layoutContent();

    FileSession session_;
    std::optional<PlaylistIni> ini_;
    bool dirty_ = false;
    juce::StringArray changes_;
    DiagnosticList diagnostics_;

    juce::Viewport viewport_;
    juce::Component content_;
    juce::OwnedArray<SourceEditor> sources_;
    juce::TextEditor affiliates_, beepFile_, beepMinutes_, others_, problems_;
    juce::ToggleButton beepEnabled_ { L"Beep ativo" };
    juce::Label affiliatesTitle_, beepTitle_, othersTitle_, problemsTitle_, affiliatesHelp_, beepHelp_;
    juce::TextButton saveButton_ { "Salvar" }, discardButton_ { "Descartar" }, createButton_ { L"Criar PLAYLIST.ini" };
    juce::Label fileTitle_, fileInfo_;
    Banner banner_;
};

} // namespace pc::ui
