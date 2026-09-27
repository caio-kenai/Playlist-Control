#include "ui/Table.h"

namespace pc::ui
{

using namespace theme;

Table::Table()
{
    addAndMakeVisible (table_);
    table_.setModel (this);
    table_.setRowHeight (26);
    table_.setHeaderHeight (28);
    table_.setOutlineThickness (1);
    table_.getHeader().setStretchToFitActive (true);
}

Table::~Table()
{
    table_.setModel (nullptr);
}

void Table::setColumns (const std::vector<Column>& columns)
{
    columns_ = columns;
    auto& header = table_.getHeader();
    header.removeAllColumns();
    for (int i = 0; i < (int) columns.size(); ++i)
        header.addColumn (columns[(size_t) i].title, i + 1, columns[(size_t) i].width, 40, -1,
                          juce::TableHeaderComponent::defaultFlags);
}

void Table::setRows (std::vector<Row> rows)
{
    rows_ = std::move (rows);
    hasStatus_ = false;
    for (auto& r : rows_)
        hasStatus_ = hasStatus_ || r.status.has_value() || r.statusOk;
    applyFilter();
}

void Table::setFilter (const juce::String& text)
{
    filter_ = text.trim();
    applyFilter();
}

void Table::applyFilter()
{
    visible_.clear();
    for (size_t i = 0; i < rows_.size(); ++i)
    {
        bool match = filter_.isEmpty();
        for (auto& c : rows_[i].cells)
            match = match || c.containsIgnoreCase (filter_);
        if (match)
            visible_.push_back (i);
    }
    table_.updateContent();
    table_.repaint();
}

const Table::Row* Table::selectedRow() const
{
    auto r = table_.getSelectedRow();
    return r >= 0 && r < (int) visible_.size() ? &rows_[visible_[(size_t) r]] : nullptr;
}

void Table::resized()
{
    table_.setBounds (getLocalBounds());
}

int Table::getNumRows()
{
    return (int) visible_.size();
}

void Table::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
    g.fillAll (selected ? colours::infoBack : (row % 2 ? colours::panelAlt : colours::panel));
}

void Table::paintCell (juce::Graphics& g, int row, int column, int width, int height, bool)
{
    if (row >= (int) visible_.size())
        return;
    auto& r = rows_[visible_[(size_t) row]];
    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);
    if (column == 1 && hasStatus_)
    {
        auto icon = area.removeFromLeft (18).toFloat().withSizeKeepingCentre (13, 13);
        if (r.status.has_value() || r.statusOk)
            drawStatusIcon (g, icon, r.status.value_or (Severity::info), r.statusOk);
        area.removeFromLeft (4);
    }
    g.setColour (colours::text);
    g.setFont (font (13.0f));
    auto text = column - 1 < r.cells.size() ? r.cells[column - 1] : juce::String();
    g.drawText (text, area, juce::Justification::centredLeft, true);
}

void Table::selectedRowsChanged (int row)
{
    if (row >= 0 && row < (int) visible_.size() && onSelect)
        onSelect (rows_[visible_[(size_t) row]]);
}

void Table::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) visible_.size() && onDoubleClick)
        onDoubleClick (rows_[visible_[(size_t) row]]);
}

juce::String Table::getCellTooltip (int row, int column)
{
    if (row < 0 || row >= (int) visible_.size())
        return {};
    auto& r = rows_[visible_[(size_t) row]];
    if (r.tooltip.isNotEmpty())
        return r.tooltip;
    return column - 1 < r.cells.size() ? r.cells[column - 1] : juce::String();
}

void Table::sortOrderChanged (int columnId, bool forwards)
{
    auto col = columnId - 1;
    std::stable_sort (rows_.begin(), rows_.end(), [&] (const Row& a, const Row& b) {
        auto cmp = a.cells[col].compareNatural (b.cells[col]);
        return forwards ? cmp < 0 : cmp > 0;
    });
    applyFilter();
}

} // namespace pc::ui
