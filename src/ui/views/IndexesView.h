#pragma once

#include "ui/Table.h"
#include "ui/View.h"

namespace pc::ui
{

// pgm\Indices: state of each index compared with its table, and the
// recreation procedure (close Playlist, delete, SeparaComprove, reopen).
class IndexesView : public View, private juce::Timer
{
public:
    explicit IndexesView (AppContext& context);
    ~IndexesView() override;

    juce::String title() const override { return L"Índices"; }
    juce::String subtitle() const override { return L"pgm\\Indices — conferência e recriação dos índices do Playlist Digital"; }
    void refresh() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void inspect();
    void updateCheck();
    void startRebuild();
    void attach();
    bool autoplayConfigured (bool& known) const;

    // Transparent layer that draws the texts of the recreation card.
    struct TextLayer : public juce::Component
    {
        TextLayer() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            if (draw)
                draw (g);
        }
        std::function<void (juce::Graphics&)> draw;
    };

    Table table_;
    TextLayer textLayer_;
    Card statusCard_ { L"Situação dos índices" }, rebuildCard_ { L"Recriar índices" };
    ActionButton verifyButton_ { L"Verificar novamente", Icon::refresh, ActionButton::Style::secondary };
    ActionButton rebuildButton_ { L"Recriar índices agora", Icon::database, ActionButton::Style::danger };
    ActionButton openBackupButton_ { L"Abrir cópia de segurança", Icon::folderOpen, ActionButton::Style::secondary };
    juce::ToggleButton startAtEnd_ { L"Abrir o Playlist Digital ao final (necessário para ele criar os índices)" };
    IndexRebuildCheck check_;
    int problems_ = 0;
    juce::Rectangle<int> textArea_, stepsArea_;
};

} // namespace pc::ui
