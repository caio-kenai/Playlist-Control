#pragma once

#include "ui/View.h"

namespace pc::ui
{

class BlockList;
class BlockInspector;

// Maps (commercial), grades (musical) or operational clocks.
class ScheduleView : public View,
                     private juce::ListBoxModel,
                     private juce::Timer
{
public:
    enum class Mode
    {
        maps,
        grades,
        clocks
    };

    ScheduleView (AppContext& context, Mode mode);
    ~ScheduleView() override;

    juce::String title() const override;
    juce::String subtitle() const override;
    void refresh() override;
    bool hasUnsavedChanges() const override { return dirty_; }
    void save() override;
    void discard() override;
    void openFile (const juce::File& file, int line) override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Used by the block list and the inspector.
    ScheduleDocument* document() { return doc_.has_value() ? &*doc_ : nullptr; }
    const CodeCatalog& catalog() const { return ctx.workspace.catalog(); }
    Date documentDate() const;
    bool editable() const;
    ScheduleKind kind() const noexcept { return kind_; }
    void selectLine (int lineIndex, int itemIndex = -1);
    int selectedLine() const noexcept { return selectedLine_; }
    int selectedItem() const noexcept { return selectedItem_; }
    void documentEdited (int lineIndex, const juce::String& description);
    void status (const juce::String& text) { ctx.status (text); }
    const std::vector<Diagnostic>& diagnosticsForLine (int lineIndex) const;

private:
    // ListBoxModel (file list)
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void timerCallback() override;
    void loadFiles();
    void loadDocument (const juce::File& file);
    void revalidate();
    void updateBanners();
    void doSave();
    void createNewFile();
    ScheduleKind kindFor (const juce::File& file) const;

    struct FileRow
    {
        ScheduleFileInfo info;
        FileOrigin origin = FileOrigin::unknown;
        bool active = false; // the file the Playlist reads today
        int errors = 0;
    };

    Mode mode_;
    ScheduleKind kind_;
    std::vector<FileRow> files_;
    juce::ListBox fileList_;
    juce::TextButton newButton_ { "Novo arquivo" };

    FileSession session_;
    std::optional<ScheduleDocument> doc_;
    OriginAssessment origin_;
    std::vector<std::vector<Diagnostic>> lineDiagnostics_;
    DiagnosticList diagnostics_;
    bool dirty_ = false;
    juce::StringArray pendingChanges_;
    int selectedLine_ = -1, selectedItem_ = -1;

    Banner originBanner_, conflictBanner_;
    juce::TextButton saveButton_ { "Salvar" }, discardButton_ { "Descartar" }, addBlockButton_ { "Novo bloco" },
        removeBlockButton_ { "Remover bloco" };
    juce::Label fileTitle_, fileInfo_;
    std::unique_ptr<BlockList> blocks_;
    juce::Viewport blocksViewport_;
    std::unique_ptr<BlockInspector> inspector_;
    juce::ListBox problems_;
    std::unique_ptr<juce::ListBoxModel> problemsModel_;
    bool ignoreSelection_ = false;
};

} // namespace pc::ui
