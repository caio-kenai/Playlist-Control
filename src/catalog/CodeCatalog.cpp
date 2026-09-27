#include "catalog/CodeCatalog.h"

namespace pc
{

juce::String CodeCatalog::normalize (const juce::String& code)
{
    auto c = code.trim().toUpperCase();
    int i = 0;
    while (i < c.length() - 1 && c[i] == '0')
        ++i;
    return c.substring (i);
}

void CodeCatalog::load (const FoldersXml* folders, const DbfTable* ligacao)
{
    folders_ = folders;
    registrations_.clear();
    byCode_.clear();
    fileIndex_.clear();
    indexed_ = false;
    if (ligacao == nullptr)
        return;

    for (int r = 0; r < ligacao->recordCount(); ++r)
    {
        Registration reg;
        reg.record = r;
        reg.deleted = ligacao->isDeleted (r);
        reg.code = ligacao->getString (r, "CODIGO").trim();
        reg.normalized = normalize (reg.code);
        reg.file = ligacao->getString (r, "ARQUIVO").trim();
        reg.type = ligacao->getString (r, "TIPO").trim();
        reg.validFrom = ligacao->getDate (r, "DATAINI");
        reg.validTo = ligacao->getDate (r, "DATAFIM");
        reg.validFromTime = ligacao->getString (r, "HORAINI").trim();
        reg.validToTime = ligacao->getString (r, "HORAFIM").trim();
        registrations_.push_back (reg);
    }
    for (size_t i = 0; i < registrations_.size(); ++i)
        if (! registrations_[i].deleted && registrations_[i].normalized.isNotEmpty())
            byCode_[registrations_[i].normalized].push_back (i);
}

void CodeCatalog::indexFolderFiles()
{
    fileIndex_.clear();
    if (folders_ != nullptr)
    {
        for (auto& f : folders_->folders())
        {
            if (f.kind == FolderKind::command || f.kind == FolderKind::pause)
                continue;
            juce::File dir (f.target);
            if (! dir.isDirectory())
                continue;
            for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFiles))
                fileIndex_[entry.getFile().getFileName().toLowerCase()].addIfNotAlreadyThere (f.title);
        }
    }
    indexed_ = true;
}

const FolderEntry* CodeCatalog::folderForCode (const juce::String& code) const
{
    if (folders_ == nullptr)
        return nullptr;
    auto n = normalize (code);
    for (auto& f : folders_->folders())
        if (f.code.isNotEmpty() && normalize (f.code) == n)
            return &f;
    return nullptr;
}

std::vector<const Registration*> CodeCatalog::registrationsFor (const juce::String& code) const
{
    std::vector<const Registration*> out;
    auto it = byCode_.find (normalize (code));
    if (it != byCode_.end())
        for (auto i : it->second)
            out.push_back (&registrations_[i]);
    return out;
}

Validity CodeCatalog::validityOn (const Registration& r, const Date& date)
{
    auto key = [] (const Date& d) { return d.year * 10000 + d.month * 100 + d.day; };
    if (r.validFrom.has_value() && key (date) < key (*r.validFrom))
        return Validity::notYet;
    if (r.validTo.has_value() && key (date) > key (*r.validTo))
        return Validity::expired;
    return Validity::valid;
}

juce::StringArray CodeCatalog::foldersContaining (const juce::String& fileName) const
{
    auto it = fileIndex_.find (fileName.trim().toLowerCase());
    return it != fileIndex_.end() ? it->second : juce::StringArray();
}

ItemResolution CodeCatalog::resolve (const ScheduleItem& item, const Date& date) const
{
    ItemResolution r;
    switch (item.kind)
    {
        case ItemKind::empty:
        case ItemKind::command:
            r.status = ItemStatus::notChecked;
            r.description = item.kind == ItemKind::command ? "Comando " + item.raw : "Item vazio";
            return r;

        case ItemKind::quotedFile:
        case ItemKind::bareText:
        case ItemKind::codeAndFile:
        {
            r.foundIn = foldersContaining (item.file);
            r.description = item.file;
            r.status = ! indexed_ || ! r.foundIn.isEmpty() ? ItemStatus::ok : ItemStatus::fileMissing;
            return r;
        }

        case ItemKind::code:
        {
            auto code = item.baseCode();
            r.folder = folderForCode (code);
            r.registrations = registrationsFor (code);
            if (r.folder != nullptr && r.registrations.empty())
            {
                r.status = ItemStatus::ok;
                r.description = "Pasta " + r.folder->title + " (" + toDisplayString (r.folder->kind) + ")";
                return r;
            }
            if (r.registrations.empty())
            {
                r.status = ItemStatus::unknownCode;
                r.description = L"Código não registrado";
                return r;
            }

            std::vector<const Registration*> valid;
            for (auto* reg : r.registrations)
                if (validityOn (*reg, date) == Validity::valid)
                    valid.push_back (reg);
            if (valid.empty())
            {
                r.status = ItemStatus::outOfValidity;
                r.description = r.registrations.front()->file;
                return r;
            }

            juce::StringArray files;
            bool anyFound = false;
            for (auto* reg : valid)
            {
                files.add (reg->file);
                if (reg->type == "A")
                {
                    anyFound = true; // shortcut: the folder is checked separately
                    continue;
                }
                auto where = foldersContaining (reg->file);
                r.foundIn.addArray (where);
                anyFound = anyFound || ! where.isEmpty();
            }
            r.foundIn.removeDuplicates (true);
            r.description = files.joinIntoString (" / ") + (valid.size() > 1 ? L"  (rodízio)" : L"");
            r.status = ! indexed_ || anyFound ? ItemStatus::ok : ItemStatus::fileMissing;
            return r;
        }
    }
    return r;
}

} // namespace pc
