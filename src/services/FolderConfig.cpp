#include "services/FolderConfig.h"
#include "core/TextCase.h"
#include "formats/ntx/NtxIndex.h"
#include "logging/Logger.h"

namespace pc
{

namespace
{
const juce::String invalidTitleChars = "\\/:*?\"<>|";

juce::String lnkName (const juce::String& title)
{
    return title + ".lnk";
}

bool isRegistrationOf (const DbfTable& t, int record, const juce::String& arquivo)
{
    return ! t.isDeleted (record) && t.getString (record, "TIPO").trim().equalsIgnoreCase ("A")
           && t.getString (record, "ARQUIVO").trim().equalsIgnoreCase (arquivo);
}

int findRecord (const DbfTable& t, const juce::String& arquivo, const juce::String& code)
{
    for (int r = 0; r < t.recordCount(); ++r)
        if (isRegistrationOf (t, r, arquivo))
            return r;
    if (code.isNotEmpty())
        for (int r = 0; r < t.recordCount(); ++r)
            if (! t.isDeleted (r) && t.getString (r, "TIPO").trim().equalsIgnoreCase ("A")
                && t.getString (r, "CODIGO").trim().equalsIgnoreCase (code))
                return r;
    return -1;
}

Diagnostic problem (Severity s, const juce::String& code, const juce::String& message, const juce::String& fix,
                    const juce::String& reason = {})
{
    Diagnostic d;
    d.severity = s;
    d.code = code;
    d.message = message;
    d.reason = reason;
    d.fix = fix;
    return d;
}

std::optional<juce::MemoryBlock> rebuildIndex (const juce::File& file, const DbfTable& table, juce::String& error)
{
    if (! file.existsAsFile())
        return std::nullopt; // the Playlist creates it when it opens
    juce::MemoryBlock bytes;
    if (! readFileShared (file, bytes, error))
        return std::nullopt;
    auto current = readNtx (bytes);
    std::vector<NtxEntry> entries;
    if (! expectedNtxEntries (current.header, table, entries, error))
        return std::nullopt;
    auto built = buildNtx (current.header, entries);
    auto check = verifyNtx (built, table);
    if (! check.consistent)
    {
        error = file.getFileName() + ": " + check.summary;
        return std::nullopt;
    }
    return built;
}

ShortcutInfo shortcutOf (const FolderEntry& f)
{
    return { f.target, f.shortcutArguments, f.title, f.iconLocation, f.iconIndex };
}
} // namespace

void normalizeFolder (FolderEntry& f, const juce::File& pgm)
{
    auto& info = folderTypeInfo (f.kind);
    f.type = info.letter;
    if (f.kind == FolderKind::command)
    {
        auto line = f.shortcutArguments.trim();
        f.shortcutArguments = line.startsWithIgnoreCase ("C") ? line : "C " + line;
    }
    else
        f.shortcutArguments = info.letter;
    if (! info.needsDirectory)
        f.target = pgm.getFullPathName();
    f.title = f.title.trim();
    f.code = toUpperLatin (f.code.trim());
    f.shortcutPath = pgm.getChildFile ("Atalhos").getChildFile (lnkName (f.title)).getFullPathName();
}

juce::String suggestFolderCode (const juce::String& title, const std::vector<FolderEntry>& current, const DbfTable* ligacao,
                                juce::Random& random)
{
    auto letters = toUpperLatin (title.retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"));
    auto base = letters.substring (0, 3);
    if (base.isEmpty())
        base = "PST";
    auto taken = [&] (const juce::String& code) {
        for (auto& f : current)
            if (f.code.equalsIgnoreCase (code))
                return true;
        if (ligacao != nullptr)
            for (int r = 0; r < ligacao->recordCount(); ++r)
                if (! ligacao->isDeleted (r) && ligacao->getString (r, "CODIGO").trim().equalsIgnoreCase (code))
                    return true;
        return false;
    };
    if (! taken (base))
        return base;
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        auto code = base;
        for (int i = 0; i < 3; ++i)
            code += alphabet[random.nextInt (36)];
        if (! taken (code))
            return code;
    }
    return base + juce::String (current.size());
}

FolderEntry makeNewFolder (FolderKind kind, const juce::File& directory, const juce::File& pgm,
                           const std::vector<FolderEntry>& current, const DbfTable* ligacao, int nextId)
{
    auto& info = folderTypeInfo (kind);
    FolderEntry f;
    f.id = nextId;
    f.kind = kind;
    f.title = info.needsDirectory ? directory.getFileName() : juce::String::fromUTF8 (info.name);
    f.target = info.needsDirectory ? directory.getFullPathName() : pgm.getFullPathName();
    f.iconLocation = pgm.getChildFile ("Icones").getChildFile ("Icones5.dll").getFullPathName();
    f.iconIndex = info.defaultIconIndex;
    f.output = -1;
    f.totalFiles = 0;
    f.shortcutArguments = kind == FolderKind::command ? juce::String ("C") : juce::String (info.letter);
    if (info.needsDirectory)
    {
        juce::Random random;
        f.code = suggestFolderCode (f.title, current, ligacao, random);
    }
    normalizeFolder (f, pgm);
    return f;
}

FolderPlan planFolderChanges (const juce::File& pgm, const FoldersXml& original, const std::vector<FolderEntry>& edited,
                              const juce::MemoryBlock* ligacaoBytes, juce::Time now)
{
    FolderPlan plan;
    auto foldersFile = pgm.getChildFile ("Folders.xml");
    auto atalhos = pgm.getChildFile ("Atalhos");

    std::optional<DbfTable> table;
    if (ligacaoBytes != nullptr)
    {
        juce::String error;
        table = DbfTable::parse (*ligacaoBytes, error);
        if (! table.has_value() || table->truncated())
            plan.problems.add (problem (Severity::error, "folders.dbf", L"Não foi possível ler o LIGACAO.DBF: " + error,
                                        L"Confira o arquivo Dados\\LIGACAO.DBF.") );
    }

    auto originalOf = [&] (const FolderEntry& e) -> const FolderEntry* {
        if (e.node < 0)
            return nullptr;
        for (auto& o : original.folders())
            if (o.node == e.node)
                return &o;
        return nullptr;
    };

    // Validation.
    for (size_t i = 0; i < edited.size(); ++i)
    {
        auto& f = edited[i];
        auto where = L"Pasta \"" + f.title + "\": ";
        auto& info = folderTypeInfo (f.kind);
        if (f.title.isEmpty())
            plan.problems.add (problem (Severity::error, "folders.title", L"Há uma pasta sem título.", L"Preencha o título."));
        else if (f.title.containsAnyOf (invalidTitleChars))
            plan.problems.add (problem (Severity::error, "folders.title", where + L"o título não pode ter \\ / : * ? \" < > |.",
                                        L"Troque esses caracteres.", L"O título vira o nome do atalho em Atalhos."));
        for (size_t j = i + 1; j < edited.size(); ++j)
        {
            if (f.title.isNotEmpty() && f.title.equalsIgnoreCase (edited[j].title))
                plan.problems.add (problem (Severity::error, "folders.title.duplicate", L"Duas pastas com o título \"" + f.title + "\".",
                                            L"Use títulos diferentes: cada um vira um atalho."));
            if (f.code.isNotEmpty() && f.code.equalsIgnoreCase (edited[j].code))
                plan.problems.add (problem (Severity::error, "folders.code.duplicate", L"O código " + f.code + L" está em duas pastas.",
                                            L"Use códigos diferentes."));
        }
        if (f.code.length() > 12)
            plan.problems.add (problem (Severity::error, "folders.code", where + L"o código tem mais de 12 caracteres.",
                                        L"Use até 12 caracteres (tamanho do campo CODIGO)."));
        if (f.code.containsAnyOf (" ,()\"|<>"))
            plan.problems.add (problem (Severity::error, "folders.code", where + L"o código não pode ter espaços, vírgulas, "
                                                                                  L"parênteses, aspas ou | < >.",
                                        L"Use letras e números.", L"O código é escrito nos mapas e grades entre vírgulas."));
        for (auto c : f.title + f.code)
            if (! isRepresentableIn1252 (c))
            {
                plan.problems.add (problem (Severity::error, "folders.charset", where + L"caractere não suportado: "
                                                                                        + juce::String::charToString (c),
                                            L"O LIGACAO.DBF usa Windows-1252; troque o caractere."));
                break;
            }
        if (f.kind == FolderKind::unknown)
            plan.problems.add (problem (Severity::error, "folders.type", where + L"tipo desconhecido (" + f.type + ").",
                                        L"Escolha um tipo."));
        if (info.needsDirectory && f.target.isEmpty())
            plan.problems.add (problem (Severity::error, "folders.target", where + L"sem diretório.", L"Escolha o diretório."));
        else if (info.needsDirectory && ! juce::File (f.target).isDirectory())
            plan.problems.add (problem (Severity::warning, "folders.target", where + L"o diretório " + f.target + L" não existe.",
                                        L"Crie o diretório ou escolha outro."));
        if (f.kind == FolderKind::command && f.commandLine().isEmpty())
            plan.problems.add (problem (Severity::error, "folders.command", where + L"sem linha de comando.",
                                        L"Ex.: C UDP {PLAY}, C URL https://…, C COM4: P."));
        if (f.code.isEmpty() && f.kind != FolderKind::pause)
            plan.problems.add (problem (Severity::info, "folders.code.empty", where + L"sem código no Registrar.",
                                        L"Sem código a pasta não pode ser chamada nos mapas e grades."));
        if (f.iconLocation.isNotEmpty() && ! juce::File (f.iconLocation).existsAsFile())
            plan.problems.add (problem (Severity::warning, "folders.icon", where + L"o arquivo de ícone não existe.",
                                        L"Escolha outro ícone."));
        else if (f.iconLocation.endsWithIgnoreCase (".ico"))
            plan.problems.add (problem (Severity::info, "folders.icon.ico", where + L"ícone em arquivo .ico.",
                                        L"Prefira um ícone de pgm\\Icones\\*.dll.",
                                        L"O Config Manager não mostra ícones .ico (\"Erro ao carregar ícone\" no log)."));
    }

    // Codes already registered for other files.
    if (table.has_value())
    {
        for (auto& f : edited)
        {
            if (f.code.isEmpty())
                continue;
            auto* o = originalOf (f);
            for (int r = 0; r < table->recordCount(); ++r)
            {
                if (table->isDeleted (r) || ! table->getString (r, "CODIGO").trim().equalsIgnoreCase (f.code))
                    continue;
                auto arquivo = table->getString (r, "ARQUIVO").trim();
                bool mine = arquivo.equalsIgnoreCase (lnkName (f.title)) || (o != nullptr && arquivo.equalsIgnoreCase (lnkName (o->title)));
                bool otherFolder = false;
                for (auto& g : edited)
                    if (&g != &f && arquivo.equalsIgnoreCase (lnkName (g.title)))
                        otherFolder = true;
                if (! mine && (otherFolder || ! table->getString (r, "TIPO").trim().equalsIgnoreCase ("A")))
                    plan.problems.add (problem (Severity::error, "folders.code.registered",
                                                L"O código " + f.code + L" já está registrado para " + arquivo + ".",
                                                L"Escolha outro código para a pasta \"" + f.title + "\"."));
            }
        }
    }

    // What changed.
    juce::Array<int> kept;
    for (auto& f : edited)
    {
        auto* o = originalOf (f);
        if (o == nullptr)
        {
            plan.changes.add (L"Pasta " + f.title + L" adicionada (" + toDisplayString (f.kind)
                              + (f.code.isNotEmpty() ? ", " + f.code : juce::String()) + ")");
            continue;
        }
        kept.add (o->node);
        juce::StringArray what;
        if (o->title != f.title) what.add (L"título " + o->title + L" → " + f.title);
        if (o->kind != f.kind) what.add (L"tipo " + toDisplayString (f.kind));
        if (o->target != f.target && folderTypeInfo (f.kind).needsDirectory) what.add (L"diretório " + f.target);
        if (o->code != f.code) what.add (L"código " + o->code + L" → " + f.code);
        if (o->iconLocation != f.iconLocation || o->iconIndex != f.iconIndex) what.add (L"ícone");
        if (o->shortcutArguments != f.shortcutArguments && f.kind == FolderKind::command) what.add (L"linha de comando " + f.shortcutArguments);
        if (! what.isEmpty())
            plan.changes.add (L"Pasta " + o->title + ": " + what.joinIntoString (", "));
    }
    for (auto& o : original.folders())
        if (! kept.contains (o.node))
            plan.changes.add (L"Pasta " + o.title + L" removida");

    if (plan.problems.hasErrors() || ! plan.hasChanges())
        return plan;

    // Folders.xml.
    if (! original.renderBytes (edited, plan.foldersXml))
        plan.problems.add (problem (Severity::error, "folders.xml", L"Não foi possível montar o Folders.xml.", L"Verifique os títulos."));

    // Shortcuts.
    for (auto& f : edited)
    {
        auto* o = originalOf (f);
        auto file = atalhos.getChildFile (lnkName (f.title));
        bool changed = o == nullptr || o->title != f.title || o->target != f.target || o->shortcutArguments != f.shortcutArguments
                       || o->iconLocation != f.iconLocation || o->iconIndex != f.iconIndex || ! file.existsAsFile();
        if (changed)
            plan.shortcuts.push_back ({ file, shortcutOf (f) });
        if (o != nullptr && ! o->title.equalsIgnoreCase (f.title))
            plan.removed.push_back (o->shortcutPath.isNotEmpty() ? juce::File (o->shortcutPath) : atalhos.getChildFile (lnkName (o->title)));
    }
    for (auto& o : original.folders())
        if (! kept.contains (o.node))
            plan.removed.push_back (o.shortcutPath.isNotEmpty() ? juce::File (o.shortcutPath) : atalhos.getChildFile (lnkName (o.title)));

    // LIGACAO.DBF: one TIPO=A record per folder with a code.
    if (table.has_value())
    {
        bool dbfChanged = false;
        auto date = now.formatted ("%Y%m%d");
        auto time = now.formatted ("%H:%M");
        for (auto& f : edited)
        {
            if (f.code.isEmpty())
                continue;
            auto* o = originalOf (f);
            if (o != nullptr && o->code == f.code && o->title == f.title)
                continue;
            auto record = o != nullptr ? findRecord (*table, lnkName (o->title), o->code) : -1;
            if (record < 0)
                record = findRecord (*table, lnkName (f.title), f.code); // reuses a record left by a removed folder
            if (record < 0)
            {
                record = table->appendRecord();
                table->setString (record, "TIPO", "A");
            }
            bool ok = record >= 0 && table->setString (record, "CODIGO", f.code) && table->setString (record, "ARQUIVO", lnkName (f.title))
                      && table->setString (record, "DATAREG", date) && table->setString (record, "HORAREG", time);
            if (! ok)
                plan.problems.add (problem (Severity::error, "folders.dbf.write", L"Não foi possível registrar o código " + f.code + ".",
                                            L"Confira o título e o código."));
            dbfChanged = true;
        }
        if (dbfChanged)
        {
            plan.ligacao = table->bytes();
            juce::String error;
            auto indices = pgm.getChildFile ("Indices");
            for (auto [name, target] : { std::pair<const char*, std::optional<juce::MemoryBlock>*> { "LIGA_COD.NTX", &plan.ligaCod },
                                         std::pair<const char*, std::optional<juce::MemoryBlock>*> { "LIGA_ARQ.NTX", &plan.ligaArq } })
            {
                error.clear();
                *target = rebuildIndex (indices.getChildFile (name), *table, error);
                if (error.isNotEmpty())
                    plan.problems.add (problem (Severity::error, "folders.ntx", L"Não foi possível atualizar " + juce::String (name) + ": " + error,
                                                L"Use Suporte > Índices."));
            }
        }
    }
    juce::ignoreUnused (foldersFile);
    return plan;
}

bool applyFolderPlan (const FolderPlan& plan, const juce::File& pgm, SafeWriter& writer, const juce::File& backupFolder,
                      juce::String& error)
{
    auto foldersFile = pgm.getChildFile ("Folders.xml");
    auto ligacao = pgm.getChildFile ("Dados/LIGACAO.DBF");
    auto ligaCod = pgm.getChildFile ("Indices/LIGA_COD.NTX");
    auto ligaArq = pgm.getChildFile ("Indices/LIGA_ARQ.NTX");
    auto atalhos = pgm.getChildFile ("Atalhos");

    // Copy of everything that may change.
    if (! backupFolder.createDirectory() || ! backupFolder.getChildFile ("Atalhos").createDirectory())
    {
        error = L"Não foi possível criar a cópia de segurança em " + backupFolder.getFullPathName() + ".";
        return false;
    }
    bool copied = true;
    for (auto& f : { foldersFile, ligacao, ligaCod, ligaArq })
        if (f.existsAsFile())
            copied = copied && f.copyFileTo (backupFolder.getChildFile (f.getFileName()));
    for (auto& f : atalhos.findChildFiles (juce::File::findFiles, false, "*.lnk"))
        copied = copied && f.copyFileTo (backupFolder.getChildFile ("Atalhos").getChildFile (f.getFileName()));
    if (! copied)
    {
        error = L"Não foi possível copiar os arquivos para " + backupFolder.getFullPathName() + ".";
        return false;
    }

    auto restore = [&] {
        for (auto& f : { foldersFile, ligacao, ligaCod, ligaArq })
        {
            auto saved = backupFolder.getChildFile (f.getFileName());
            if (saved.existsAsFile())
                saved.copyFileTo (f);
        }
        for (auto& f : atalhos.findChildFiles (juce::File::findFiles, false, "*.lnk"))
            if (! backupFolder.getChildFile ("Atalhos").getChildFile (f.getFileName()).existsAsFile())
                f.deleteFile();
        for (auto& f : backupFolder.getChildFile ("Atalhos").findChildFiles (juce::File::findFiles, false, "*.lnk"))
            f.copyFileTo (atalhos.getChildFile (f.getFileName()));
    };

    auto summary = plan.changes.joinIntoString ("; ");
    auto write = [&] (const juce::File& target, const juce::MemoryBlock& bytes, const juce::String& operation) {
        WriteRequest r;
        r.target = target;
        r.content = bytes;
        r.operation = operation;
        r.summary = summary;
        auto result = writer.write (r);
        if (! result.succeeded())
            error = target.getFileName() + ": " + result.message;
        return result.succeeded();
    };

    bool ok = write (foldersFile, plan.foldersXml, L"Configurar pastas");
    if (ok && plan.ligacao.has_value())
        ok = write (ligacao, *plan.ligacao, L"Registrar códigos das pastas");
    if (ok && plan.ligaCod.has_value())
        ok = write (ligaCod, *plan.ligaCod, L"Atualizar índice LIGA_COD");
    if (ok && plan.ligaArq.has_value())
        ok = write (ligaArq, *plan.ligaArq, L"Atualizar índice LIGA_ARQ");
    if (ok)
    {
        atalhos.createDirectory();
        for (auto& f : plan.removed)
            if (f.existsAsFile() && ! f.deleteFile())
            {
                error = L"Não foi possível apagar " + f.getFileName() + ".";
                ok = false;
                break;
            }
    }
    if (ok)
        for (auto& s : plan.shortcuts)
            if (! writeShortcut (s.file, s.info, error))
            {
                error = s.file.getFileName() + ": " + error;
                ok = false;
                break;
            }
    if (! ok)
    {
        restore();
        Logger::instance().error ("folders.save", L"Falha ao gravar as pastas; arquivos devolvidos", { { "error", error } });
        return false;
    }
    Logger::instance().info ("folders.save", L"Pastas gravadas: " + summary, { { "backup", backupFolder.getFullPathName() } });
    return true;
}

FolderSave::FolderSave (FolderPlan plan, juce::File pgm, SafeWriter& writer, juce::File backupRoot, bool startPlaylistAtEnd)
    : juce::Thread ("Folder save"), plan_ (std::move (plan)), pgm_ (std::move (pgm)), backupRoot_ (std::move (backupRoot)),
      writer_ (writer), startAtEnd_ (startPlaylistAtEnd)
{
    steps_ = {
        { L"Conferir programas abertos" },
        { L"Fechar o Playlist Digital" },
        { L"Gravar as pastas (com cópia de segurança)" },
        { L"Abrir o Playlist Digital" },
    };
}

FolderSave::~FolderSave()
{
    signalThreadShouldExit();
    waitForThreadToExit (-1);
    masterReference.clear();
}

std::vector<JobStep> FolderSave::steps() const
{
    std::lock_guard<std::mutex> g (lock_);
    return steps_;
}

juce::String FolderSave::outcome() const
{
    std::lock_guard<std::mutex> g (lock_);
    return outcome_;
}

void FolderSave::set (int step, JobStep::State state, const juce::String& detail)
{
    {
        std::lock_guard<std::mutex> g (lock_);
        steps_[(size_t) step].state = state;
        steps_[(size_t) step].detail = detail;
    }
    juce::WeakReference<FolderSave> self (this);
    juce::MessageManager::callAsync ([self] {
        if (self != nullptr && self->onChange)
            self->onChange();
    });
}

void FolderSave::finish (bool ok, const juce::String& text)
{
    {
        std::lock_guard<std::mutex> g (lock_);
        outcome_ = text;
        for (auto& s : steps_)
            if (s.state == JobStep::State::pending)
                s.state = JobStep::State::skipped;
    }
    succeeded_ = ok;
    finished_ = true;
    juce::WeakReference<FolderSave> self (this);
    juce::MessageManager::callAsync ([self] {
        if (self != nullptr && self->onChange)
            self->onChange();
    });
}

void FolderSave::run()
{
    set (stepCheck, JobStep::State::running);
    auto check = checkPlaylistPrograms (pgm_);
    if (! check.blockers.isEmpty())
    {
        set (stepCheck, JobStep::State::failed, check.blockers.joinIntoString (" "));
        finish (false, L"Nada foi alterado.");
        return;
    }
    bool wasRunning = ! check.playlist.empty();
    set (stepCheck, JobStep::State::done, wasRunning ? L"Playlist Digital aberto: será fechado e aberto de novo."
                                                     : juce::String (L"Playlist Digital fechado."));

    if (wasRunning)
    {
        set (stepClose, JobStep::State::running, L"Pedindo ao Playlist que feche…");
        juce::String error;
        if (! closePlaylist (check.playlist, 90000, [this] (const juce::String& t) { set (stepClose, JobStep::State::running, t); },
                             [this] { return threadShouldExit(); }, error))
        {
            set (stepClose, JobStep::State::failed, error);
            finish (false, L"Nada foi alterado.");
            return;
        }
        set (stepClose, JobStep::State::done, L"Playlist fechado.");
    }
    else
        set (stepClose, JobStep::State::skipped, L"Já estava fechado.");

    set (stepWrite, JobStep::State::running);
    backupFolder_ = backupRoot_.getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S"));
    juce::String error;
    bool written = applyFolderPlan (plan_, pgm_, writer_, backupFolder_, error);
    set (stepWrite, written ? JobStep::State::done : JobStep::State::failed,
         written ? juce::String (plan_.changes.size()) + L" alteração(ões) gravada(s). Cópia em " + backupFolder_.getFullPathName()
                 : error + L" Os arquivos anteriores foram devolvidos.");

    if ((wasRunning || startAtEnd_) && check.playlistExe != juce::File())
    {
        set (stepStart, JobStep::State::running, L"Abrindo " + check.playlistExe.getFileName() + L"…");
        juce::String detail;
        auto started = startPlaylist (check.playlistExe, pgm_, detail);
        set (stepStart, started ? JobStep::State::done : JobStep::State::failed, detail);
    }
    else
        set (stepStart, JobStep::State::skipped, L"Abra o Playlist Digital para aplicar as pastas.");

    finish (written, written ? juce::String (L"Pastas gravadas.") : juce::String (L"As pastas não foram gravadas."));
}

} // namespace pc
