#include "services/FileSession.h"

namespace pc
{

bool FileSession::load (juce::String& error)
{
    loaded_ = false;
    bytes_.reset();
    if (! file_.existsAsFile())
    {
        snapshot_ = {};
        error = L"Arquivo não encontrado: " + file_.getFullPathName();
        return false;
    }
    if (! readFileShared (file_, bytes_, error))
        return false;
    snapshot_ = FileSnapshot::fromBytes (file_, bytes_);
    loaded_ = true;
    return true;
}

bool FileSession::changedOnDisk() const
{
    if (! loaded_)
        return false;
    // Cheap check first; the hash is computed only when size or time moved.
    if (file_.existsAsFile() && file_.getSize() == snapshot_.size && file_.getLastModificationTime() == snapshot_.modified)
        return false;
    return ! FileSnapshot::take (file_).sameContent (snapshot_);
}

WriteResult FileSession::save (SafeWriter& writer, const juce::MemoryBlock& content, const juce::String& operation,
                               const juce::String& summary, std::function<juce::String (const juce::MemoryBlock&)> verify)
{
    WriteRequest req;
    req.target = file_;
    req.content = content;
    req.operation = operation;
    req.summary = summary;
    req.expectedBase = snapshot_;
    req.verify = std::move (verify);
    auto r = writer.write (req);
    if (r.status == WriteStatus::written || r.status == WriteStatus::unchanged)
    {
        bytes_ = content;
        snapshot_ = r.after;
        loaded_ = true;
    }
    return r;
}

} // namespace pc
