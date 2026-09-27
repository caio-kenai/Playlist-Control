#pragma once

#include "ui/Theme.h"

namespace pc::ui
{

// Read-only table of text cells with optional per-row status and filtering.
class Table : public juce::Component, private juce::TableListBoxModel
{
public:
    struct Column
    {
        juce::String title;
        int width = 120;
    };

    struct Row
    {
        juce::StringArray cells;
        std::optional<Severity> status; // icon in the first column
        bool statusOk = false;
        juce::String tooltip;
        int tag = 0;
    };

    Table();
    ~Table() override;

    void setColumns (const std::vector<Column>& columns);
    void setRows (std::vector<Row> rows);
    void setFilter (const juce::String& text);
    const Row* selectedRow() const;
    void selectRow (int row) { table_.selectRow (row); }
    int numVisibleRows() const { return (int) visible_.size(); }

    std::function<void (const Row&)> onSelect;
    std::function<void (const Row&)> onDoubleClick;

    void resized() override;

private:
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int column, int width, int height, bool selected) override;
    void selectedRowsChanged (int row) override;
    void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
    juce::String getCellTooltip (int row, int column) override;
    void sortOrderChanged (int columnId, bool forwards) override;

    void applyFilter();

    juce::TableListBox table_;
    std::vector<Column> columns_;
    std::vector<Row> rows_;
    std::vector<size_t> visible_;
    juce::String filter_;
    bool hasStatus_ = false;
};

} // namespace pc::ui
