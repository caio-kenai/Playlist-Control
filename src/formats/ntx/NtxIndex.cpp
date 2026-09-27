#include "formats/ntx/NtxIndex.h"

#include <algorithm>
#include <set>

namespace pc
{

namespace
{
constexpr int pageSize = 1024;

juce::uint16 read16 (const juce::uint8* p) { return (juce::uint16) (p[0] | (p[1] << 8)); }
juce::uint32 read32 (const juce::uint8* p) { return (juce::uint32) p[0] | ((juce::uint32) p[1] << 8) | ((juce::uint32) p[2] << 16) | ((juce::uint32) p[3] << 24); }
void write16 (juce::uint8* p, int v) { p[0] = (juce::uint8) (v & 0xFF); p[1] = (juce::uint8) ((v >> 8) & 0xFF); }
void write32 (juce::uint8* p, juce::uint32 v) { for (int i = 0; i < 4; ++i) p[i] = (juce::uint8) ((v >> (8 * i)) & 0xFF); }
} // namespace

unsigned char upper1252 (unsigned char c) noexcept
{
    if (c >= 0x61 && c <= 0x7A) return (unsigned char) (c - 0x20);
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return (unsigned char) (c - 0x20);
    if (c == 0x9A) return 0x8A;
    if (c == 0x9C) return 0x8C;
    if (c == 0x9E) return 0x8E;
    if (c == 0xFF) return 0x9F;
    return c;
}

NtxReadResult readNtx (const juce::MemoryBlock& bytes)
{
    NtxReadResult r;
    r.header = NtxHeader::parse (bytes);
    if (! r.header.valid)
    {
        r.problem = r.header.problem;
        return r;
    }
    auto* data = static_cast<const juce::uint8*> (bytes.getData());
    const auto size = (juce::uint32) bytes.getSize();
    const auto& h = r.header;
    std::set<juce::uint32> visited;

    std::function<bool (juce::uint32, int)> walk = [&] (juce::uint32 offset, int depth) -> bool {
        if (depth > 64 || offset < (juce::uint32) pageSize || offset + pageSize > size || offset % pageSize != 0)
        {
            r.problem = "Página " + juce::String (offset) + L" fora do arquivo.";
            return false;
        }
        if (! visited.insert (offset).second)
        {
            r.problem = L"Página " + juce::String (offset) + L" referenciada mais de uma vez.";
            return false;
        }
        ++r.pages;
        auto* page = data + offset;
        int count = read16 (page);
        if (count > h.maxItems)
        {
            r.problem = L"Página " + juce::String (offset) + L" com mais itens que o permitido.";
            return false;
        }
        for (int i = 0; i <= count; ++i)
        {
            int itemOffset = read16 (page + 2 + 2 * i);
            if (itemOffset + h.itemSize > pageSize)
            {
                r.problem = L"Item fora da página " + juce::String (offset) + ".";
                return false;
            }
            auto* item = page + itemOffset;
            auto child = read32 (item);
            if (child != 0 && ! walk (child, depth + 1))
                return false;
            if (i < count)
                r.entries.push_back ({ std::string (reinterpret_cast<const char*> (item + 8), (size_t) h.keySize), read32 (item + 4) });
        }
        return true;
    };

    if (walk (h.rootPage, 0))
    {
        for (size_t i = 1; i < r.entries.size() && r.problem.isEmpty(); ++i)
            if (r.entries[i].key < r.entries[i - 1].key)
                r.problem = L"Chaves fora de ordem.";
    }
    return r;
}

namespace
{
bool evaluateTerm (juce::String term, const DbfTable& table, int record, std::string& out)
{
    term = term.trim();
    auto wrapped = [&] (const char* fn) {
        return term.startsWithIgnoreCase (juce::String (fn) + "(") && term.endsWithChar (')');
    };
    auto inner = [&] (const char* fn) { return term.substring ((int) strlen (fn) + 1, term.length() - 1); };

    if (wrapped ("UPPER"))
    {
        std::string v;
        if (! evaluateTerm (inner ("UPPER"), table, record, v))
            return false;
        for (auto& c : v)
            c = (char) upper1252 ((unsigned char) c);
        out += v;
        return true;
    }
    if (wrapped ("DTOS"))
        return evaluateTerm (inner ("DTOS"), table, record, out); // D fields are stored as YYYYMMDD
    if (wrapped ("DESCEND"))
    {
        std::string v;
        if (! evaluateTerm (inner ("DESCEND"), table, record, v))
            return false;
        for (auto& c : v)
            c = (char) ((256 - (unsigned char) c) & 0xFF);
        out += v;
        return true;
    }
    auto field = table.fieldIndex (term);
    if (field < 0)
        return false;
    out += table.rawField (record, field);
    return true;
}
} // namespace

bool evaluateNtxKey (const juce::String& expression, const DbfTable& table, int record, std::string& key)
{
    key.clear();
    // Split on '+' outside parentheses.
    int depth = 0, start = 0;
    for (int i = 0; i <= expression.length(); ++i)
    {
        auto c = i < expression.length() ? expression[i] : (juce::juce_wchar) '+';
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        else if (c == '+' && depth == 0)
        {
            if (! evaluateTerm (expression.substring (start, i), table, record, key))
                return false;
            start = i + 1;
        }
    }
    return depth == 0;
}

bool expectedNtxEntries (const NtxHeader& header, const DbfTable& table, std::vector<NtxEntry>& out, juce::String& error)
{
    out.clear();
    for (int r = 0; r < table.recordCount(); ++r)
    {
        std::string key;
        if (! evaluateNtxKey (header.keyExpression, table, r, key))
        {
            error = L"Expressão de chave não suportada: " + header.keyExpression;
            return false;
        }
        key.resize ((size_t) header.keySize, ' ');
        out.push_back ({ key, (juce::uint32) r + 1 });
    }
    std::stable_sort (out.begin(), out.end(), [] (const NtxEntry& a, const NtxEntry& b) {
        return a.key != b.key ? a.key < b.key : a.record < b.record;
    });
    return true;
}

NtxVerification verifyNtx (const juce::MemoryBlock& ntx, const DbfTable& table)
{
    NtxVerification v;
    auto read = readNtx (ntx);
    v.tableRecords = table.recordCount();
    if (read.problem.isNotEmpty())
    {
        v.summary = L"Índice danificado: " + read.problem;
        return v;
    }
    v.readable = true;
    v.indexEntries = (int) read.entries.size();
    std::vector<NtxEntry> expected;
    juce::String error;
    if (! expectedNtxEntries (read.header, table, expected, error))
    {
        v.summary = error;
        return v;
    }
    if (read.entries == expected)
    {
        v.consistent = true;
        v.summary = L"Índice confere com a tabela (" + juce::String (v.indexEntries) + L" chaves).";
        return v;
    }
    size_t i = 0;
    while (i < read.entries.size() && i < expected.size() && read.entries[i] == expected[i])
        ++i;
    v.summary = L"Índice não confere com a tabela: " + juce::String (v.indexEntries) + L" chaves no índice, "
              + juce::String ((int) expected.size()) + L" registros na tabela; primeira diferença na posição "
              + juce::String ((int) i + 1) + ".";
    return v;
}

juce::MemoryBlock buildNtx (const NtxHeader& h, const std::vector<NtxEntry>& entries)
{
    struct Item
    {
        juce::uint32 child = 0, record = 0;
        std::string key;
    };
    struct Page
    {
        std::vector<Item> items;
        juce::uint32 last = 0;
        juce::uint32 offset = 0;
        // Slots keep what was written before, as in the Playlist's own files.
        std::vector<Item> slots;
    };

    const int maxItems = h.maxItems;
    std::vector<Page> written;
    std::vector<Page> levels (1);
    juce::uint32 next = pageSize;

    auto store = [&] (Page& p) {
        for (size_t i = 0; i < p.items.size(); ++i)
        {
            if (p.slots.size() <= i) p.slots.resize (i + 1);
            p.slots[i] = p.items[i];
        }
    };
    auto allocate = [&] (Page& p) {
        p.offset = next;
        next += pageSize;
        store (p);
        written.push_back (p);
    };

    std::function<void (size_t, Item)> add = [&] (size_t level, Item item) {
        if (level == levels.size())
            levels.emplace_back();
        auto& p = levels[level];
        if ((int) p.items.size() < maxItems)
        {
            p.items.push_back (item);
            return;
        }
        p.last = item.child;
        allocate (p);
        auto offset = p.offset;
        levels[level] = Page {};
        add (level + 1, { offset, item.record, item.key });
    };

    for (auto& e : entries)
        add (0, { 0, e.record, e.key });

    auto findWritten = [&] (juce::uint32 offset) -> Page& {
        for (auto& w : written)
            if (w.offset == offset)
                return w;
        jassertfalse;
        return written.front();
    };

    // An empty last page borrows: the last separator of the parent moves
    // down and the last key of the previous page moves up.
    std::vector<size_t> rotated;
    for (size_t level = levels.size() - 1; level-- > 0;)
    {
        auto& p = levels[level];
        auto& parent = levels[level + 1];
        if (! p.items.empty() || parent.items.empty())
            continue;
        auto separator = parent.items.back();
        parent.items.pop_back();
        auto& prev = findWritten (separator.child);
        auto moved = prev.items.back();
        prev.items.pop_back();
        p.items.push_back ({ level == 0 ? 0u : prev.last, separator.record, separator.key });
        if (level > 0)
            prev.last = moved.child;
        parent.items.push_back ({ prev.offset, moved.record, moved.key });
        rotated.push_back (level);
    }

    for (size_t level = 1; level < levels.size(); ++level)
        if (levels[level].offset == 0)
            allocate (levels[level]);
    for (auto level : rotated)
    {
        allocate (levels[level]);
        levels[level + 1].last = levels[level].offset;
        findWritten (levels[level + 1].offset).last = levels[level].offset;
    }
    for (size_t level = 0; level + 1 < levels.size(); ++level)
    {
        if (levels[level].offset == 0 && ! levels[level].items.empty())
        {
            allocate (levels[level]);
            findWritten (levels[level + 1].offset).last = levels[level].offset;
        }
    }
    if (levels.back().offset == 0)
        allocate (levels.back());
    // Rightmost pointer of each level: the current page one level below.
    for (size_t level = 0; level + 1 < levels.size(); ++level)
        levels[level + 1].last = levels[level].offset;
    // Keep the final state of every level page (parents gained items).
    for (auto& l : levels)
        for (auto& w : written)
            if (w.offset == l.offset)
            {
                w.items = l.items;
                w.last = l.last;
                store (w);
            }

    juce::MemoryBlock out ((size_t) next, true);
    auto* data = static_cast<juce::uint8*> (out.getData());
    write16 (data, h.signature);
    write16 (data + 2, h.version);
    write32 (data + 4, levels.back().offset);
    write32 (data + 8, 0);
    write16 (data + 12, h.itemSize);
    write16 (data + 14, h.keySize);
    write16 (data + 16, h.keyDecimals);
    write16 (data + 18, h.maxItems);
    write16 (data + 20, h.halfPage);
    auto expr = h.keyExpression.toStdString();
    memcpy (data + 22, expr.data(), juce::jmin<size_t> (expr.size(), 255));
    data[278] = h.unique ? 1 : 0;

    const int tableStart = 2 + (maxItems + 1) * 2;
    for (auto& w : written)
    {
        auto* page = data + w.offset;
        write16 (page, (int) w.items.size());
        for (int i = 0; i <= maxItems; ++i)
            write16 (page + 2 + 2 * i, tableStart + i * h.itemSize);
        for (size_t i = 0; i < w.slots.size(); ++i)
        {
            auto* item = page + tableStart + (int) i * h.itemSize;
            write32 (item, w.slots[i].child);
            write32 (item + 4, w.slots[i].record);
            memcpy (item + 8, w.slots[i].key.data(), (size_t) h.keySize);
        }
        auto* last = page + tableStart + (int) w.items.size() * h.itemSize;
        write32 (last, w.last);
        if (w.items.size() >= w.slots.size())
        {
            write32 (last + 4, 0);
            memset (last + 8, 0, (size_t) h.keySize);
        }
    }
    return out;
}

} // namespace pc
