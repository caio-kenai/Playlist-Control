#include "formats/folders/FoldersXml.h"

namespace pc
{

namespace
{
// Texts and defaults observed in the Config Manager 1.0.0.7 ("Nova pasta").
const std::vector<FolderTypeInfo> types = {
    { FolderKind::music, "M", "Músicas", "Essa pasta deve conter os arquivos de músicas.", 331, 3, true },
    { FolderKind::commercial, "$", "Comerciais", "Essa pasta deve conter os arquivos de áudio dos comerciais.", 303, 0, true },
    { FolderKind::sweeper, "V", "Vinhetas", "Essa pasta deve conter as vinhetas.", 356, 2, true },
    { FolderKind::voiceTrack, "L", "Locuções", "Essa pasta deve conter as locuções gravadas.", 4, 0, true },
    { FolderKind::text, "X", "Textos", "Essa pasta deve conter arquivos de textos no formato TXT ou RTF.", 6, 0, true },
    { FolderKind::track, "T", "Trilhas",
      "Essa pasta deve conter as trilhas. Quando o Playlist Digital executar um arquivo dessa pasta, ele ficará em loop.", 359, 0, true },
    { FolderKind::timeAnnouncement, "H", "Hora Certa",
      "Essa pasta deve conter as locuções de hora certa no formato hhmm.mp3 (hh: hora com dois dígitos, mm: minutos com dois "
      "dígitos).\n\nOutra opção é gravar as horas separadas dos minutos: H00.mp3, H01.mp3 … H23.mp3 para as horas e M00.mp3, "
      "M01.mp3 … M59.mp3 para os minutos.", 86, 1, true },
    { FolderKind::pause, "P", "Pausa", "Inserção especial de pausa.", 393, 1, false },
    { FolderKind::command, "C", "Comando", "Comando especial para a programação.", 6, 1, false },
    { FolderKind::random, "A", "Aleatórias", "Essa pasta deve conter os arquivos de áudios aleatórios.", 6, 1, true },
    { FolderKind::sequential, "S", "Sequenciais", "Essa pasta deve conter os arquivos de áudios sequenciais.", 6, 1, true },
    { FolderKind::temperature, "W", "Temperatura",
      "Essa pasta deve conter as locuções de temperatura, no formato GGD.wav ou GGD.mp3. Por exemplo, 23 graus e 5 décimos: "
      "235.mp3. O Playlist fará a aproximação de até 5 décimos de diferença, para mais ou para menos.", 215, 1, true },
    { FolderKind::generic, "", "Outras",
      "Essa pasta deve conter arquivos de áudio utilizados pela emissora que não se encaixam nas demais categorias.", 6, 0, true },
};

const FolderTypeInfo unknownType { FolderKind::unknown, "", "Desconhecido", "", 6, 0, true };

// Elements of a folder, in the order the Config Manager writes them.
const char* const elementOrder[] = { "ID", "Title", "Type", "Target", "IconLocation", "ShortcutArguments",
                                     "ShortcutPathName", "IconIndex", "Output", "TotalFiles", "DBFId" };

juce::String valueOf (const FolderEntry& e, const juce::String& element)
{
    if (element == "ID") return juce::String (e.id);
    if (element == "Title") return e.title;
    if (element == "Type") return e.type;
    if (element == "Target") return e.target;
    if (element == "IconLocation") return e.iconLocation;
    if (element == "ShortcutArguments") return e.shortcutArguments;
    if (element == "ShortcutPathName") return e.shortcutPath;
    if (element == "IconIndex") return juce::String (e.iconIndex);
    if (element == "Output") return juce::String (e.output);
    if (element == "TotalFiles") return juce::String (e.totalFiles);
    if (element == "DBFId") return e.code;
    return {};
}

// Replaces the content of <name>...</name> inside a folder block; adds the
// element before the closing tag when it is missing.
juce::String patchElement (const juce::String& block, const juce::String& name, const juce::String& value,
                           const juce::String& eol, const juce::String& childIndent, const juce::String& closeTag)
{
    auto open = "<" + name + ">";
    auto close = "</" + name + ">";
    auto a = block.indexOf (open);
    if (a >= 0)
    {
        auto contentStart = a + open.length();
        auto b = block.indexOf (contentStart, close);
        if (b >= 0)
            return block.substring (0, contentStart) + XmlPatchDocument::escape (value) + block.substring (b);
    }
    auto selfClosing = block.indexOf ("<" + name + " />");
    if (selfClosing < 0)
        selfClosing = block.indexOf ("<" + name + "/>");
    if (selfClosing >= 0)
    {
        auto end = block.indexOf (selfClosing, ">") + 1;
        return block.substring (0, selfClosing) + open + XmlPatchDocument::escape (value) + close + block.substring (end);
    }
    auto at = block.lastIndexOf (closeTag);
    auto lineStart = block.substring (0, at).lastIndexOf (eol);
    auto insertAt = lineStart >= 0 ? lineStart : at;
    return block.substring (0, insertAt) + eol + childIndent + open + XmlPatchDocument::escape (value) + close + block.substring (insertAt);
}
} // namespace

const std::vector<FolderTypeInfo>& folderTypes()
{
    return types;
}

const FolderTypeInfo& folderTypeInfo (FolderKind kind)
{
    for (auto& t : types)
        if (t.kind == kind)
            return t;
    return unknownType;
}

juce::String folderGroupName (int group)
{
    switch (group)
    {
        case 1:  return "Comandos";
        case 2:  return "Vinhetas";
        case 3:  return juce::String::fromUTF8 ("Músicas");
        default: return "Gerais";
    }
}

FolderKind folderKindFromType (const juce::String& type)
{
    auto t = type.trim().toUpperCase();
    if (t.isEmpty()) return FolderKind::generic;
    for (auto& info : types)
        if (juce::String (info.letter).isNotEmpty() && t[0] == juce::String (info.letter)[0])
            return info.kind;
    return FolderKind::unknown;
}

juce::String toDisplayString (FolderKind kind)
{
    return juce::String::fromUTF8 (folderTypeInfo (kind).name);
}

juce::String FolderEntry::commandLine() const
{
    if (kind != FolderKind::command)
        return {};
    auto a = shortcutArguments.trim();
    return a.startsWithIgnoreCase ("C ") ? a.substring (2).trim() : a.substring (1).trim();
}

std::optional<FoldersXml> FoldersXml::parse (const juce::MemoryBlock& bytes, juce::String& error)
{
    auto doc = XmlPatchDocument::parse (bytes, error);
    if (! doc.has_value())
        return std::nullopt;
    auto root = doc->root();
    if (doc->node (root).name != "Folders")
    {
        error = L"O elemento raiz não é <Folders>.";
        return std::nullopt;
    }

    FoldersXml f;
    auto text = [&] (int parent, const char* name) {
        auto c = doc->childNamed (parent, name);
        return c >= 0 ? doc->value (c) : juce::String();
    };

    f.version_ = text (root, "Version");
    auto count = text (root, "Folders");
    f.declaredCount_ = count.isNotEmpty() ? count.getIntValue() : -1;

    auto shared = doc->childNamed (root, "Shared");
    if (shared >= 0)
    {
        f.sharedServer_ = text (shared, "Server");
        for (auto c : doc->node (shared).children)
        {
            auto& n = doc->node (c);
            if (n.name.startsWith ("Folder") && n.name != "Folders" && ! n.children.empty())
                f.shared_.push_back ({ text (c, "Name"), text (c, "Path") });
        }
    }

    for (auto c : doc->node (root).children)
    {
        auto& n = doc->node (c);
        if (! n.name.startsWith ("Folder") || n.name == "Folders" || n.children.empty())
            continue;
        FolderEntry e;
        e.element = n.name;
        e.id = text (c, "ID").getIntValue();
        e.title = text (c, "Title");
        e.type = text (c, "Type");
        e.kind = folderKindFromType (e.type);
        e.target = text (c, "Target");
        e.iconLocation = text (c, "IconLocation");
        e.iconIndex = text (c, "IconIndex").getIntValue();
        e.shortcutArguments = text (c, "ShortcutArguments");
        e.shortcutPath = text (c, "ShortcutPathName");
        auto output = text (c, "Output");
        e.output = output.isNotEmpty() ? output.getIntValue() : -1;
        e.totalFiles = text (c, "TotalFiles").getIntValue();
        e.code = text (c, "DBFId");
        e.line = doc->lineOf (c);
        e.node = c;
        f.folders_.push_back (e);
    }
    f.doc_ = std::move (doc);
    return f;
}

const FolderEntry* FoldersXml::findByTitle (const juce::String& title) const
{
    for (auto& f : folders_)
        if (f.title.equalsIgnoreCase (title))
            return &f;
    return nullptr;
}

const FolderEntry* FoldersXml::findByCode (const juce::String& code) const
{
    for (auto& f : folders_)
        if (f.code.isNotEmpty() && f.code.equalsIgnoreCase (code))
            return &f;
    return nullptr;
}

int FoldersXml::nextId() const
{
    int top = 0;
    for (auto& f : folders_)
        top = juce::jmax (top, f.id);
    return top + 1;
}

juce::String FoldersXml::render (const std::vector<FolderEntry>& list) const
{
    jassert (doc_.has_value());
    auto& d = *doc_;
    auto eol = d.eol();
    auto root = d.root();
    auto& rootNode = d.node (root);

    // Folder elements directly under the root, in document order.
    std::vector<int> nodes;
    for (auto c : rootNode.children)
    {
        auto& n = d.node (c);
        if (n.name.startsWith ("Folder") && n.name != "Folders" && ! n.children.empty())
            nodes.push_back (c);
    }

    juce::String outerIndent = "  ", childIndent = "    ";
    int regionStart = 0, regionEnd = 0; // text replaced by the folder list
    juce::String separator = eol + outerIndent;
    if (! nodes.empty())
    {
        auto& first = d.node (nodes.front());
        regionStart = first.start;
        regionEnd = d.node (nodes.back()).end;
        auto lineBefore = d.textBetween (0, first.start);
        auto nl = lineBefore.lastIndexOf ("\n");
        outerIndent = lineBefore.substring (nl + 1);
        separator = eol + outerIndent;
        if (! first.children.empty())
        {
            auto childStart = d.node (first.children.front()).start;
            auto before = d.textBetween (first.contentStart, childStart);
            childIndent = before.substring (before.lastIndexOf ("\n") + 1);
        }
    }
    else
    {
        // No folders yet: they go before </Folders>, after the last child.
        regionStart = regionEnd = rootNode.children.empty() ? rootNode.contentStart : d.node (rootNode.children.back()).end;
    }

    juce::String blocks;
    int index = 0;
    for (auto& e : list)
    {
        auto tag = "Folder" + juce::String (index);
        juce::String block;
        if (e.node >= 0)
        {
            auto& n = d.node (e.node);
            block = d.textBetween (n.start, n.end);
            // Renumber the element.
            block = "<" + tag + block.substring (1 + n.name.length());
            auto closeAt = block.lastIndexOf ("</" + n.name + ">");
            block = block.substring (0, closeAt) + "</" + tag + ">" + block.substring (closeAt + n.name.length() + 3);
            const FolderEntry* original = nullptr;
            for (auto& o : folders_)
                if (o.node == e.node)
                    original = &o;
            for (auto* name : elementOrder)
                if (original == nullptr || valueOf (*original, name) != valueOf (e, name))
                    block = patchElement (block, name, valueOf (e, name), eol, childIndent, "</" + tag + ">");
        }
        else
        {
            block = "<" + tag + ">";
            for (auto* name : elementOrder)
                block << eol << childIndent << "<" << name << ">" << XmlPatchDocument::escape (valueOf (e, name)) << "</" << name << ">";
            block << eol << outerIndent << "</" << tag << ">";
        }
        // The first block reuses the indentation already before the region.
        if (index > 0 || nodes.empty())
            blocks << separator;
        blocks << block;
        ++index;
    }

    auto head = d.textBetween (0, regionStart);
    auto tail = d.textBetween (regionEnd, (int) d.text().length() + 1);
    if (list.empty() && ! nodes.empty())
    {
        // Drop the line that held the first folder.
        head = head.trimCharactersAtEnd (" \t");
        if (head.endsWith (eol))
            head = head.dropLastCharacters (eol.length());
    }
    auto result = head + blocks + tail;

    // Count of folders.
    auto countNode = d.childNamed (root, "Folders");
    if (countNode >= 0 && d.node (countNode).end <= regionStart)
    {
        auto& cn = d.node (countNode);
        result = d.textBetween (0, cn.contentStart) + juce::String ((int) list.size())
                 + result.substring (cn.contentEnd);
    }
    return result;
}

bool FoldersXml::renderBytes (const std::vector<FolderEntry>& list, juce::MemoryBlock& out) const
{
    if (! doc_.has_value())
        return false;
    return encodeText (render (list), doc_->encoding(), out, nullptr);
}

} // namespace pc
