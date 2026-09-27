#pragma once

#include "ui/Table.h"
#include "ui/View.h"

namespace pc::ui
{

// Folders of the Config Manager and codes registered in LIGACAO.DBF.
class FoldersView : public View
{
public:
    explicit FoldersView (AppContext& context);
    juce::String title() const override { return L"Pastas e códigos"; }
    juce::String subtitle() const override { return L"Pastas do Config Manager e arquivos registrados (Registrar / Ligacao.exe)"; }
    void refresh() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextEditor filter_;
    juce::TabbedComponent tabs_ { juce::TabbedButtonBar::TabsAtTop };
    Table folders_, registrations_, problems_;
    Banner banner_;
};

// Operators and their permissions (Operadores\*\Config.xml).
class OperatorsView : public View, private juce::ListBoxModel
{
public:
    explicit OperatorsView (AppContext& context);
    ~OperatorsView() override;
    juce::String title() const override { return "Operadores"; }
    juce::String subtitle() const override { return L"Permissões de cada operador (Ferramentas > Opções > Operadores)"; }
    void refresh() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int row) override;
    void showOperator (int index);

    juce::ListBox list_;
    Table permissions_;
    Banner banner_;
};

// Every check over the installation, with the reason and the fix.
class DiagnosticsView : public View
{
public:
    explicit DiagnosticsView (AppContext& context);
    juce::String title() const override { return L"Diagnóstico"; }
    juce::String subtitle() const override;
    void refresh() override;
    void openFile (const juce::File& file, int line) override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void run();
    void show (const Diagnostic& d);
    void openInEditor (const Diagnostic& d);

    DiagnosticList all_;
    juce::ComboBox severity_;
    juce::TextEditor filter_, detail_;
    juce::TextButton runButton_ { "Verificar novamente" }, openButton_ { "Abrir no editor" };
    Table table_;
    std::optional<Diagnostic> current_;
    juce::Time ranAt_;
};

// Saved changes: what, when, before/after, restore.
class HistoryView : public View
{
public:
    explicit HistoryView (AppContext& context);
    juce::String title() const override { return L"Histórico de alterações"; }
    juce::String subtitle() const override { return L"Cada gravação feita pelo PlaylistControl, com cópia do estado anterior"; }
    void refresh() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    class DiffView;

private:
    void show (int index);
    void restore();

    std::vector<HistoryEntry> entries_;
    Table table_;
    juce::TextButton restoreButton_ { L"Restaurar versão anterior" }, folderButton_ { L"Abrir pasta do histórico" };
    juce::Label summary_;
    std::unique_ptr<DiffView> diff_;
    juce::Viewport diffViewport_;
    int current_ = -1;
};

} // namespace pc::ui
