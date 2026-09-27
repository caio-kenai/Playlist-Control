#pragma once

#include "core/TimeOfDay.h"

#include <optional>
#include <string>

namespace pc
{

struct DbfField
{
    juce::String name;
    char type = 'C';
    int length = 0;
    int decimals = 0;
    int offset = 0; // within the record, after the deletion flag
};

// Read-only dBase III table (LIGACAO.DBF, COMPROVE.DBF). Text is Windows-1252.
class DbfTable
{
public:
    static std::optional<DbfTable> parse (const juce::MemoryBlock& bytes, juce::String& error);

    int version() const noexcept { return version_; }
    bool hasMemo() const noexcept { return (version_ & 0x80) != 0; }
    Date lastUpdate() const noexcept { return lastUpdate_; }
    int declaredRecords() const noexcept { return declaredRecords_; }
    int recordCount() const noexcept { return recordCount_; } // records actually present
    int headerLength() const noexcept { return headerLength_; }
    int recordLength() const noexcept { return recordLength_; }
    bool truncated() const noexcept { return recordCount_ < declaredRecords_; }
    bool hasEofMarker() const noexcept { return eof_; }
    const std::vector<DbfField>& fields() const noexcept { return fields_; }

    int fieldIndex (const juce::String& name) const;
    bool isDeleted (int record) const;
    juce::String getString (int record, int field) const;
    std::string rawField (int record, int field) const; // bytes as stored
    juce::String getString (int record, const juce::String& field) const;
    std::optional<Date> getDate (int record, int field) const;
    std::optional<Date> getDate (int record, const juce::String& field) const;

private:
    const juce::uint8* recordPtr (int record) const;

    juce::MemoryBlock data_;
    int version_ = 0;
    Date lastUpdate_;
    int declaredRecords_ = 0, recordCount_ = 0, headerLength_ = 0, recordLength_ = 0;
    bool eof_ = false;
    std::vector<DbfField> fields_;
};

} // namespace pc
