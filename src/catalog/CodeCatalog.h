#pragma once

#include "formats/dbf/DbfTable.h"
#include "formats/folders/FoldersXml.h"
#include "formats/schedule/ScheduleDocument.h"

#include <map>

namespace pc
{

// Row of LIGACAO.DBF ("Registrar").
struct Registration
{
    int record = 0;
    juce::String code;
    juce::String normalized;
    juce::String file;
    juce::String type;       // A atalho, C comercial, ...
    std::optional<Date> validFrom, validTo;
    juce::String validFromTime, validToTime;
    bool deleted = false;
};

enum class Validity
{
    valid,
    notYet,
    expired
};

// How an item of a map or grade resolves against folders and registrations.
enum class ItemStatus
{
    ok,
    unknownCode,     // "X vermelho seguido de um código"
    outOfValidity,   // registered, but not valid on that day
    fileMissing,     // "X vermelho seguido de um nome"
    notChecked       // commands and empty items
};

struct ItemResolution
{
    ItemStatus status = ItemStatus::notChecked;
    juce::String description; // what the item plays
    const FolderEntry* folder = nullptr;
    std::vector<const Registration*> registrations;
    juce::StringArray foundIn; // folder titles where the file was found
};

// Registered codes and folder files of an installation.
class CodeCatalog
{
public:
    // Manual: up to 12 characters, leading zeros ignored, upper case.
    static juce::String normalize (const juce::String& code);

    void load (const FoldersXml* folders, const DbfTable* ligacao);

    // Indexes the files of every folder target (not recursive).
    void indexFolderFiles();
    bool filesIndexed() const noexcept { return indexed_; }

    const FoldersXml* folders() const noexcept { return folders_; }
    const std::vector<Registration>& registrations() const noexcept { return registrations_; }

    const FolderEntry* folderForCode (const juce::String& code) const;
    std::vector<const Registration*> registrationsFor (const juce::String& code) const;
    static Validity validityOn (const Registration& r, const Date& date);

    // Folder titles containing a file with this name (case-insensitive).
    juce::StringArray foldersContaining (const juce::String& fileName) const;

    ItemResolution resolve (const ScheduleItem& item, const Date& date) const;

private:
    const FoldersXml* folders_ = nullptr;
    std::vector<Registration> registrations_;
    std::map<juce::String, std::vector<size_t>> byCode_;
    std::map<juce::String, juce::StringArray> fileIndex_;
    bool indexed_ = false;
};

} // namespace pc
