#pragma once

#include "ui/View.h"

namespace pc::ui
{

// The folders of the Config Manager, laid out like the Config Manager itself:
// toolbar (Salvar, Adicionar, Excluir), the folders as icons grouped in Gerais,
// Comandos, Vinhetas and Músicas, and "Propriedades da pasta selecionada".
// Saving writes Folders.xml, the shortcuts and LIGACAO.DBF, closing and
// reopening the Playlist when it is open, as the Config Manager asks.
class ConfigManagerView : public View, private juce::Timer
{
public:
    explicit ConfigManagerView (AppContext& context);
    ~ConfigManagerView() override;

    juce::String title() const override { return "Config Manager"; }
    juce::String subtitle() const override { return L"Configurações de pastas do Playlist Digital (Folders.xml, Atalhos e códigos)"; }
    void refresh() override;
    bool hasUnsavedChanges() const override { return dirty_; }
    void save() override;
    void discard() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    // Renders the "Nova pasta" and "Selecione o ícone" dialogs to PNG files
    // (screenshot mode).
    static void captureDialogs (const juce::File& pgm, const juce::String& iconFile, int iconIndex, const juce::File& folder);

private:
    struct Grid;
    struct ToolButton;
    struct IconButton;
    struct Progress;

    void timerCallback() override;
    void load();
    void select (int index);
    void showSelection();
    void edited (const juce::String& statusText = {});
    void updateEnabled();
    void updateStatus();
    void addFolder();
    void createFolder (FolderKind kind);
    void removeFolder();
    void chooseDirectory();
    void chooseIcon();
    void attachJob();
    int nextId() const;

    std::optional<FoldersXml> original_;
    juce::MemoryBlock originalBytes_;
    std::vector<FolderEntry> folders_;
    int selected_ = -1;
    bool dirty_ = false;
    bool updating_ = false;

    std::unique_ptr<ToolButton> saveButton_, addButton_, removeButton_;
    juce::Viewport viewport_;
    std::unique_ptr<Grid> grid_;
    juce::Label propertiesTitle_, titleLabel_, typeLabel_, dirLabel_, codeLabel_, commandLabel_, status_;
    juce::TextEditor title_, directory_, code_, command_;
    juce::ComboBox type_;
    juce::TextButton browse_ { ".." };
    std::unique_ptr<IconButton> icon_;
    std::unique_ptr<Progress> progress_;
    Banner banner_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> toolbarArea_, listArea_, propertiesArea_, statusArea_;
};

} // namespace pc::ui
