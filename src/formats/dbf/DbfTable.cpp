#include "formats/dbf/DbfTable.h"
#include "core/TextCodec.h"

#include <algorithm>
#include <cstring>

namespace pc
{

std::optional<DbfTable> DbfTable::parse (const juce::MemoryBlock& bytes, juce::String& error)
{
    auto* p = static_cast<const juce::uint8*> (bytes.getData());
    auto size = (int) bytes.getSize();
    if (size < 33)
    {
        error = "Arquivo DBF muito pequeno.";
        return std::nullopt;
    }

    DbfTable t;
    t.data_ = bytes;
    t.version_ = p[0];
    auto base = (t.version_ & 0x07);
    if (base != 3)
    {
        error = L"Versão de DBF não suportada (0x" + juce::String::toHexString (t.version_) + ").";
        return std::nullopt;
    }
    t.lastUpdate_ = { 1900 + p[1], juce::jlimit (1, 12, (int) p[2]), juce::jlimit (1, 31, (int) p[3]) };
    t.declaredRecords_ = (int) juce::ByteOrder::littleEndianInt (p + 4);
    t.headerLength_ = juce::ByteOrder::littleEndianShort (p + 8);
    t.recordLength_ = juce::ByteOrder::littleEndianShort (p + 10);

    if (t.headerLength_ < 33 || t.headerLength_ > size || t.recordLength_ < 1)
    {
        error = L"Cabeçalho do DBF inválido.";
        return std::nullopt;
    }

    int offset = 1;
    for (int pos = 32; pos + 32 <= t.headerLength_ && p[pos] != 0x0D; pos += 32)
    {
        DbfField f;
        f.name = juce::String (reinterpret_cast<const char*> (p + pos), (size_t) strnlen (reinterpret_cast<const char*> (p + pos), 11));
        f.type = (char) p[pos + 11];
        f.length = p[pos + 16];
        f.decimals = p[pos + 17];
        f.offset = offset;
        offset += f.length;
        t.fields_.push_back (f);
    }
    if (t.fields_.empty() || offset != t.recordLength_)
    {
        error = L"A soma dos campos não confere com o tamanho do registro.";
        return std::nullopt;
    }

    auto available = (size - t.headerLength_) / t.recordLength_;
    t.recordCount_ = juce::jmin (available, t.declaredRecords_);
    auto end = t.headerLength_ + t.declaredRecords_ * t.recordLength_;
    t.eof_ = end < size && p[end] == 0x1A;
    return t;
}

int DbfTable::fieldIndex (const juce::String& name) const
{
    for (int i = 0; i < (int) fields_.size(); ++i)
        if (fields_[(size_t) i].name.equalsIgnoreCase (name))
            return i;
    return -1;
}

const juce::uint8* DbfTable::recordPtr (int record) const
{
    jassert (record >= 0 && record < recordCount_);
    return static_cast<const juce::uint8*> (data_.getData()) + headerLength_ + record * recordLength_;
}

bool DbfTable::isDeleted (int record) const
{
    return recordPtr (record)[0] == '*';
}

juce::String DbfTable::getString (int record, int field) const
{
    if (field < 0 || field >= (int) fields_.size())
        return {};
    auto& f = fields_[(size_t) field];
    return decodeWindows1252 (recordPtr (record) + f.offset, (size_t) f.length).trimEnd();
}

std::string DbfTable::rawField (int record, int field) const
{
    if (field < 0 || field >= (int) fields_.size())
        return {};
    auto& f = fields_[(size_t) field];
    return std::string (reinterpret_cast<const char*> (recordPtr (record) + f.offset), (size_t) f.length);
}

juce::String DbfTable::getString (int record, const juce::String& field) const
{
    return getString (record, fieldIndex (field));
}

std::optional<Date> DbfTable::getDate (int record, int field) const
{
    auto s = getString (record, field).trim();
    if (s.length() != 8 || ! s.containsOnly ("0123456789"))
        return std::nullopt;
    Date d { s.substring (0, 4).getIntValue(), s.substring (4, 6).getIntValue(), s.substring (6, 8).getIntValue() };
    if (d.month < 1 || d.month > 12 || d.day < 1 || d.day > 31)
        return std::nullopt;
    return d;
}

std::optional<Date> DbfTable::getDate (int record, const juce::String& field) const
{
    return getDate (record, fieldIndex (field));
}

bool DbfTable::setString (int record, int field, const juce::String& value)
{
    if (truncated() || record < 0 || record >= recordCount_ || field < 0 || field >= (int) fields_.size())
        return false;
    auto& f = fields_[(size_t) field];
    juce::MemoryBlock encoded;
    if (! encodeText (value, TextEncoding::windows1252, encoded, nullptr) || (int) encoded.getSize() > f.length)
        return false;
    auto* dest = static_cast<juce::uint8*> (data_.getData()) + headerLength_ + record * recordLength_ + f.offset;
    std::fill (dest, dest + f.length, (juce::uint8) ' ');
    if (f.type == 'N' || f.type == 'F')
        std::memcpy (dest + f.length - (int) encoded.getSize(), encoded.getData(), encoded.getSize()); // right-aligned
    else
        std::memcpy (dest, encoded.getData(), encoded.getSize());
    return true;
}

bool DbfTable::setString (int record, const juce::String& field, const juce::String& value)
{
    return setString (record, fieldIndex (field), value);
}

int DbfTable::appendRecord()
{
    if (truncated())
        return -1;
    auto end = (size_t) (headerLength_ + recordCount_ * recordLength_);
    juce::MemoryBlock updated;
    updated.append (data_.getData(), end);
    juce::HeapBlock<juce::uint8> blank ((size_t) recordLength_);
    std::fill (blank.get(), blank.get() + recordLength_, (juce::uint8) ' ');
    updated.append (blank.get(), (size_t) recordLength_);
    // Keep whatever followed the records (the 0x1A end marker).
    if (data_.getSize() > end)
        updated.append (static_cast<const char*> (data_.getData()) + end, data_.getSize() - end);
    else
    {
        const juce::uint8 eof = 0x1A;
        updated.append (&eof, 1);
    }
    data_ = updated;
    ++recordCount_;
    declaredRecords_ = recordCount_;
    auto* p = static_cast<juce::uint8*> (data_.getData());
    p[4] = (juce::uint8) (declaredRecords_ & 0xFF);
    p[5] = (juce::uint8) ((declaredRecords_ >> 8) & 0xFF);
    p[6] = (juce::uint8) ((declaredRecords_ >> 16) & 0xFF);
    p[7] = (juce::uint8) ((declaredRecords_ >> 24) & 0xFF);
    eof_ = data_.getSize() > (size_t) (headerLength_ + recordCount_ * recordLength_);
    return recordCount_ - 1;
}

} // namespace pc
