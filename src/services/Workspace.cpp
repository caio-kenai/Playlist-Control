#include "services/Workspace.h"
#include "logging/Logger.h"
#include "storage/FileIO.h"
#include "formats/ntx/NtxIndex.h"

namespace pc
{

Workspace::Workspace (juce::File historyFolder)
    : history_ (std::move (historyFolder)), writer_ (history_)
{
    writer_.setReadOnly (true);
}

Workspace::~Workspace()
{
    watcher_.stop();
}

bool Workspace::open (const juce::File& pgm)
{
    close();
    installation_ = inspectInstallation (pgm);
    Logger::instance().info ("installation.open", installation_.summary(), { { "pgm", pgm.getFullPathName() } });
    if (! installation_.valid)
    {
        sendChangeMessage();
        return false;
    }
    loadAll();
    watcher_.start (pgm, [this] (const auto& changes) { handleChanges (changes); });
    addActivity (L"Instalação carregada: " + pgm.getFullPathName());
    sendChangeMessage();
    return true;
}

void Workspace::close()
{
    watcher_.stop();
    installation_ = {};
    ini_.reset();
    config_.reset();
    folders_.reset();
    ligacao_.reset();
    operators_.clear();
    catalog_.load (nullptr, nullptr);
    loadProblems_ = {};
}

void Workspace::reload()
{
    if (! installation_.valid)
        return;
    installation_ = inspectInstallation (installation_.pgm);
    loadAll();
    sendChangeMessage();
}

void Workspace::loadAll()
{
    loadProblems_ = {};
    auto pgm = installation_.pgm;
    auto problem = [&] (const juce::File& f, const juce::String& what) {
        Diagnostic d;
        d.severity = Severity::error;
        d.code = "load";
        d.file = f;
        d.message = what;
        d.reason = L"O arquivo não pôde ser interpretado; as telas que dependem dele ficam indisponíveis.";
        d.fix = L"Confira o arquivo no programa que o gera. O Playlist Control não altera arquivos que não consegue ler.";
        loadProblems_.add (d);
        Logger::instance().warning ("load.failed", what, { { "file", f.getFullPathName() } });
    };
    auto read = [&] (const juce::File& f, juce::MemoryBlock& m) {
        if (! f.existsAsFile())
            return false;
        juce::String err;
        if (readFileShared (f, m, err))
            return true;
        problem (f, L"Não foi possível ler " + f.getFileName() + ": " + err);
        return false;
    };

    juce::MemoryBlock m;
    ini_.reset();
    if (read (pgm.getChildFile ("PLAYLIST.ini"), m))
        ini_ = PlaylistIni (IniDocument::fromBytes (m));

    config_.reset();
    if (read (pgm.getChildFile ("CONFIG.XML"), m))
    {
        juce::String err;
        config_ = ConfigXml::parse (m, err);
        if (! config_.has_value())
            problem (pgm.getChildFile ("CONFIG.XML"), "CONFIG.XML: " + err);
    }

    folders_.reset();
    if (read (pgm.getChildFile ("Folders.xml"), m))
    {
        juce::String err;
        folders_ = FoldersXml::parse (m, err);
        if (! folders_.has_value())
            problem (pgm.getChildFile ("Folders.xml"), "Folders.xml: " + err);
    }

    ligacao_.reset();
    auto lig = pgm.getChildFile ("Dados/LIGACAO.DBF");
    if (read (lig, m))
    {
        juce::String err;
        ligacao_ = DbfTable::parse (m, err);
        if (! ligacao_.has_value())
            problem (lig, "LIGACAO.DBF: " + err);
    }

    operators_.clear();
    for (auto& dir : pgm.getChildFile ("Operadores").findChildFiles (juce::File::findDirectories, false))
    {
        auto f = dir.getChildFile ("Config.xml");
        if (! read (f, m))
            continue;
        juce::String err;
        if (auto p = parseOperatorProfile (m, dir.getFileName(), err))
            operators_.push_back (*p);
        else
            problem (f, L"Operador " + dir.getFileName() + ": " + err);
    }

    catalog_.load (folders_.has_value() ? &*folders_ : nullptr, ligacao_.has_value() ? &*ligacao_ : nullptr);
    catalog_.indexFolderFiles();
    ecosystem_ = scanEcosystem (pgm);
    runtime_ = queryRuntime (pgm);
}

void Workspace::refreshRuntime()
{
    if (! installation_.valid)
        return;
    auto before = runtime_.playlistRunning;
    runtime_ = queryRuntime (installation_.pgm);
    for (auto& s : findServices ({ "PlaylistServer", "SyncService" }))
    {
        if (s.name.equalsIgnoreCase ("PlaylistServer")) ecosystem_.playlistServer.service = s.state;
        if (s.name.containsIgnoreCase ("SyncService")) ecosystem_.sync.service = s.state;
    }
    if (before != runtime_.playlistRunning)
    {
        addActivity (runtime_.playlistRunning ? L"Playlist Digital foi aberto." : L"Playlist Digital foi fechado.");
        sendChangeMessage();
    }
}

void Workspace::setReadOnly (bool readOnly)
{
    writer_.setReadOnly (readOnly);
    Logger::instance().info ("mode.readonly", readOnly ? "Modo somente leitura ativado" : "Modo de escrita ativado");
    sendChangeMessage();
}

void Workspace::addActivity (const juce::String& text, const juce::File& file, bool external)
{
    activity_.insert (activity_.begin(), { juce::Time::getCurrentTime(), text, file, external });
    if (activity_.size() > 200)
        activity_.resize (200);
    if (! external && file != juce::File())
        ownWrites_[file.getFullPathName().toLowerCase()] = juce::Time::getCurrentTime();
}

void Workspace::handleChanges (const std::vector<DirectoryWatcher::Change>& changes)
{
    std::vector<DirectoryWatcher::Change> external;
    for (auto& c : changes)
    {
        // Our own saves come back through the watcher; skip them.
        auto own = ownWrites_.find (c.file.getFullPathName().toLowerCase());
        if (own != ownWrites_.end() && (c.time - own->second).inSeconds() < 5)
            continue;
        external.push_back (c);
        auto rel = c.file.getRelativePathFrom (installation_.pgm);
        activity_.insert (activity_.begin(), { c.time, rel + " " + c.action + " por outro programa", c.file, true });
        Logger::instance().info ("external.change", rel + " " + c.action, { { "file", c.file.getFullPathName() } });
    }
    if (activity_.size() > 200)
        activity_.resize (200);
    if (external.empty())
        return;

    bool configChanged = false;
    for (auto& c : external)
    {
        auto name = c.file.getFileName().toLowerCase();
        configChanged = configChanged || name == "playlist.ini" || name == "config.xml" || name == "folders.xml"
                     || name == "ligacao.dbf" || c.file.getParentDirectory().getParentDirectory().getFileName().equalsIgnoreCase ("Operadores");
    }
    if (configChanged)
        loadAll();
    if (onExternalChange)
        onExternalChange (external);
    sendChangeMessage();
}

std::vector<ScheduleFileInfo> Workspace::scheduleFiles (bool clocks, ScheduleKind kind) const
{
    std::vector<ScheduleFileInfo> out;
    if (! installation_.valid)
        return out;
    auto commercialSide = kind == ScheduleKind::commercial || kind == ScheduleKind::commercialClock;
    auto dir = installation_.pgm.getChildFile (commercialSide ? "Mapas" : "Grades");
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.txt"))
    {
        bool isClock = f.getFileName().startsWithIgnoreCase ("Relogio");
        if (isClock != clocks)
            continue;
        ScheduleFileInfo i;
        i.file = f;
        i.isClock = isClock;
        i.kind = clocks ? (commercialSide ? ScheduleKind::commercialClock : ScheduleKind::musicalClock)
                        : (commercialSide ? ScheduleKind::commercial : ScheduleKind::musical);
        i.date = dateFromFileName (f.getFileName());
        out.push_back (i);
    }
    auto key = [] (const ScheduleFileInfo& i) {
        return i.date.has_value() ? juce::String::formatted ("1%04d%02d%02d", i.date->year, i.date->month, i.date->day)
                                  : "0" + i.file.getFileName().toLowerCase();
    };
    std::sort (out.begin(), out.end(), [&] (auto& a, auto& b) { return key (a) > key (b); });
    return out;
}

ScheduleSource Workspace::source (ScheduleKind kind) const
{
    if (ini_.has_value())
        return ini_->source (kind);
    ScheduleSource s;
    s.kind = kind;
    return s;
}

std::optional<ScheduleCandidate> Workspace::activeFile (ScheduleKind kind, const Date& date) const
{
    return resolveScheduleFile (source (kind), date, installation_.pgm);
}

OriginAssessment Workspace::originOf (const juce::File& file, const ScheduleDocument& doc, ScheduleKind kind) const
{
    OriginContext ctx { &ecosystem_, installation_.pgm, Date::today() };
    return assessScheduleOrigin (file, doc, kind, ctx);
}

ScheduleValidationContext Workspace::validationContext (const ScheduleFileInfo& info) const
{
    ScheduleValidationContext ctx;
    ctx.catalog = &catalog_;
    ctx.date = info.date;
    ctx.isClock = info.isClock;
    return ctx;
}

DiagnosticList Workspace::runFullDiagnostics() const
{
    DiagnosticList all;
    if (! installation_.valid)
        return all;
    auto pgm = installation_.pgm;
    all.addAll (loadProblems_);

    auto today = Date::today();
    if (ini_.has_value())
        all.addAll (validatePlaylistIni (*ini_, pgm.getChildFile ("PLAYLIST.ini"), pgm, today));
    if (config_.has_value())
        all.addAll (validateConfig (*config_, pgm.getChildFile ("CONFIG.XML")));
    if (folders_.has_value())
        all.addAll (validateFolders (*folders_, catalog_, pgm.getChildFile ("Folders.xml"), pgm));

    // Files the Playlist will read today and in the next days, then the rest.
    for (bool clocks : { false, true })
    {
        for (auto kind : { ScheduleKind::commercial, ScheduleKind::musical })
        {
            for (auto& info : scheduleFiles (clocks, kind))
            {
                if (info.date.has_value())
                {
                    // Past days no longer matter for the air.
                    auto key = [] (const Date& d) { return d.year * 10000 + d.month * 100 + d.day; };
                    if (key (*info.date) < key (today))
                        continue;
                }
                juce::MemoryBlock m;
                juce::String err;
                if (! readFileShared (info.file, m, err))
                    continue;
                all.addAll (validateSchedule (ScheduleDocument::fromBytes (m), info.file, info.kind, validationContext (info)));
            }
        }
    }

    for (auto& f : pgm.getChildFile ("Montagem").findChildFiles (juce::File::findFiles, false, "*.merge"))
    {
        juce::MemoryBlock m;
        juce::String err;
        if (readFileShared (f, m, err))
            all.addAll (validateMerge (MontagemFile::parse (m), f, folders_.has_value() ? &*folders_ : nullptr));
    }
    all.addAll (verifyIndexes());
    return all;
}

DiagnosticList Workspace::verifyIndexes() const
{
    DiagnosticList out;
    auto pgm = installation_.pgm;
    std::optional<DbfTable> comprove;
    juce::MemoryBlock m;
    juce::String err;
    if (readFileShared (pgm.getChildFile ("Dados/COMPROVE.DBF"), m, err))
        comprove = DbfTable::parse (m, err);

    for (auto& f : pgm.getChildFile ("Indices").findChildFiles (juce::File::findFiles, false, "*.NTX"))
    {
        const DbfTable* table = f.getFileName().startsWithIgnoreCase ("LIGA") ? (ligacao_ ? &*ligacao_ : nullptr)
                              : f.getFileName().startsWithIgnoreCase ("COMPROVE") ? (comprove ? &*comprove : nullptr) : nullptr;
        juce::MemoryBlock bytes;
        if (table == nullptr || ! readFileShared (f, bytes, err))
            continue;
        auto v = verifyNtx (bytes, *table);
        if (v.consistent)
            continue;
        Diagnostic d;
        d.severity = v.readable ? Severity::warning : Severity::error;
        d.code = "index.inconsistent";
        d.file = f;
        d.message = v.summary;
        d.reason = L"O Playlist localiza códigos e comprovações por estes índices. Um índice desatualizado pode fazer um código "
                   L"registrado não ser encontrado. Observado nesta instalação: o Playlist recria os índices LIGA_* ao iniciar.";
        d.fix = L"Feche e abra o Playlist Digital fora do horário crítico e verifique novamente. Veja docs/INDICES.md.";
        out.add (d);
    }
    return out;
}

} // namespace pc
