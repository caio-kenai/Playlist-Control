#include "ui/views/ReferenceViews.h"
#include "core/LineDiff.h"
#include "core/TextCase.h"
#include "formats/playlistini/FilePatternResolver.h"
#include "storage/FileIO.h"

namespace pc::ui
{

using namespace theme;

// ============================================================================
// Folders and codes

FoldersView::FoldersView (AppContext& context) : View (context)
{
    addAndMakeVisible (filter_);
    filter_.setTextToShowWhenEmpty (L"Filtrar por nome, código ou arquivo...", colours::textMuted);
    filter_.setFont (font (14.0f));
    filter_.setIndents (8, 7);
    filter_.onTextChange = [this] {
        folders_.setFilter (filter_.getText());
        registrations_.setFilter (filter_.getText());
    };
    folders_.setColumns ({ { L"Título", 200 }, { "Tipo", 110 }, { L"Código", 110 }, { L"Diretório / comando", 420 }, { "Arquivos", 80 } });
    registrations_.setColumns ({ { L"Código", 120 }, { "Arquivo", 360 }, { "Tipo", 90 }, { "Validade", 200 }, { L"Situação", 220 } });
    problems_.setColumns ({ { "Problema", 520 }, { "Onde", 200 }, { "Como resolver", 420 } });
    tabs_.setTabBarDepth (32);
    tabs_.addTab ("Pastas", colours::panel, &folders_, false);
    tabs_.addTab (L"Códigos registrados", colours::panel, &registrations_, false);
    tabs_.addTab (L"Verificação", colours::panel, &problems_, false);
    tabs_.setOutline (0);
    addAndMakeVisible (tabs_);
    addAndMakeVisible (banner_);
    banner_.show (Severity::info, L"Somente consulta. Pastas são mantidas pelo Config Manager; códigos, pelo comando Registrar (Ligacao.exe).");
    refresh();
}

void FoldersView::refresh()
{
    auto& ws = ctx.workspace;
    std::vector<Table::Row> rows;
    if (ws.folders().has_value())
    {
        for (auto& f : ws.folders()->folders())
        {
            Table::Row r;
            juce::String target = f.kind == FolderKind::command ? L"Comando: " + f.commandLine() : f.target;
            int files = 0;
            bool exists = f.kind == FolderKind::command || f.kind == FolderKind::pause || juce::File (f.target).isDirectory();
            if (exists && f.kind != FolderKind::command && f.kind != FolderKind::pause)
                files = juce::File (f.target).getNumberOfChildFiles (juce::File::findFiles);
            r.cells = { f.title, toDisplayString (f.kind), f.code, target, f.kind == FolderKind::command ? juce::String ("-") : juce::String (files) };
            if (! exists)
                r.status = Severity::error;
            else
                r.statusOk = true;
            r.tooltip = exists ? target : L"O diretório não existe: " + f.target;
            rows.push_back (r);
        }
    }
    folders_.setRows (rows);

    rows.clear();
    auto today = Date::today();
    for (auto& reg : ws.catalog().registrations())
    {
        if (reg.deleted)
            continue;
        Table::Row r;
        auto validity = reg.validFrom.has_value() || reg.validTo.has_value()
                            ? (reg.validFrom ? reg.validFrom->toString() : juce::String ("...")) + L" até " + (reg.validTo ? reg.validTo->toString() : juce::String ("..."))
                            : juce::String ("Sem prazo");
        juce::String type = reg.type == "A" ? juce::String ("Pasta") : reg.type == "C" ? juce::String ("Comercial") : reg.type;
        juce::String situation;
        auto v = CodeCatalog::validityOn (reg, today);
        if (reg.type == "A")
        {
            situation = ws.folders().has_value() && ws.folders()->findByCode (reg.code) != nullptr ? L"Pasta cadastrada" : L"Sem pasta no Config Manager";
            r.statusOk = situation == L"Pasta cadastrada";
            if (! r.statusOk)
                r.status = Severity::warning;
        }
        else if (v != Validity::valid)
        {
            situation = v == Validity::expired ? L"Vencido" : L"Ainda não vigente";
            r.status = Severity::warning;
        }
        else
        {
            auto where = ws.catalog().foldersContaining (reg.file);
            situation = where.isEmpty() ? L"Arquivo não encontrado" : L"Em " + where.joinIntoString (", ");
            if (where.isEmpty())
                r.status = Severity::error;
            else
                r.statusOk = true;
        }
        r.cells = { reg.code, reg.file, type, validity, situation };
        rows.push_back (r);
    }
    registrations_.setRows (rows);

    rows.clear();
    if (ws.folders().has_value())
    {
        // Kept in a variable: iterating items() of a temporary would dangle.
        auto found = validateFolders (*ws.folders(), ws.catalog(), ws.pgm().getChildFile ("Folders.xml"), ws.pgm());
        for (auto& d : found.items())
        {
            Table::Row r;
            r.status = d.severity;
            r.cells = { d.message, d.locationText(), d.fix };
            r.tooltip = d.message + "\n\n" + d.reason + "\n\n" + d.fix;
            rows.push_back (r);
        }
    }
    problems_.setRows (rows);
    tabs_.setTabName (2, L"Verificação (" + juce::String ((int) rows.size()) + ")");
}

void FoldersView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void FoldersView::resized()
{
    auto r = getLocalBounds();
    banner_.setBounds (r.removeFromTop (banner_.preferredHeight()));
    r.reduce (16, 12);
    filter_.setBounds (r.removeFromTop (32).removeFromLeft (420));
    r.removeFromTop (10);
    tabs_.setBounds (r);
}

// ============================================================================
// Operators

OperatorsView::OperatorsView (AppContext& context) : View (context)
{
    addAndMakeVisible (list_);
    list_.setModel (this);
    list_.setRowHeight (52);
    permissions_.setColumns ({ { L"Seção", 150 }, { L"Permissão", 340 }, { "Valor do operador", 170 }, { L"Valor efetivo", 130 } });
    addAndMakeVisible (permissions_);
    addAndMakeVisible (banner_);
    banner_.show (Severity::info, L"Somente consulta. Operadores e permissões são alterados no Playlist Digital (Ferramentas > Opções > Operadores). Senhas não são exibidas.");
    refresh();
}

OperatorsView::~OperatorsView()
{
    list_.setModel (nullptr);
}

void OperatorsView::refresh()
{
    list_.updateContent();
    if (! ctx.workspace.operators().empty())
    {
        list_.selectRow (0);
        showOperator (0);
    }
}

int OperatorsView::getNumRows()
{
    return (int) ctx.workspace.operators().size();
}

void OperatorsView::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    auto& ops = ctx.workspace.operators();
    if (row >= (int) ops.size())
        return;
    auto& o = ops[(size_t) row];
    g.fillAll (selected ? colours::infoBack : colours::panel);
    g.setColour (colours::text);
    g.setFont (font (14.0f, true));
    g.drawText (o.name, 14, 6, width - 20, 22, juce::Justification::centredLeft, true);
    juce::StringArray tags;
    if (o.isTemplate) tags.add (L"Padrão da Guia Geral");
    if (o.admin) tags.add ("Administrador");
    if (o.hasPassword) tags.add ("Com senha");
    if (o.removed) tags.add ("Removido");
    g.setColour (colours::textMuted);
    g.setFont (font (12.0f));
    g.drawText (tags.joinIntoString ("  ·  "), 14, 26, width - 20, 20, juce::Justification::centredLeft, true);
    g.setColour (colours::border);
    g.drawHorizontalLine (height - 1, 0.0f, (float) width);
}

void OperatorsView::selectedRowsChanged (int row)
{
    showOperator (row);
}

void OperatorsView::showOperator (int index)
{
    auto& ops = ctx.workspace.operators();
    if (index < 0 || index >= (int) ops.size())
        return;
    auto& op = ops[(size_t) index];
    const OperatorProfile* defaults = nullptr;
    for (auto& o : ops)
        if (o.isTemplate)
            defaults = &o;

    static const std::map<juce::String, const char*> groups = {
        { "Geral", "Geral" }, { "Edit", "Edição" }, { "Blocos", "Mudanças de bloco" }, { "BlocoComercial", "Bloco comercial" },
        { "BlocoMusical", "Bloco musical" }, { "InsCom", "Comerciais" }, { "InsMus", "Músicas" }, { "InsVH", "Vinhetas" },
        { "InsGen", "Inserções genéricas" }, { "InsPause", "Pausas" }, { "Paineis", "Painéis" }
    };
    std::vector<Table::Row> rows;
    for (auto& p : op.permissions)
    {
        Table::Row r;
        auto it = groups.find (p.group);
        auto group = it != groups.end() ? juce::String::fromUTF8 (it->second) : p.group;
        juce::String value, effect;
        auto resolved = p.permission;
        switch (p.permission)
        {
            case Permission::inherit: value = L"Padrão"; break;
            case Permission::yes:     value = "Sim"; break;
            case Permission::no:      value = L"Não"; break;
            case Permission::other:   value = p.raw; break;
        }
        if (p.permission == Permission::inherit && defaults != nullptr)
            if (auto* d = defaults->find (p.group, p.key))
                resolved = d->permission;
        effect = resolved == Permission::yes ? juce::String ("Sim") : resolved == Permission::no ? juce::String (L"Não")
               : resolved == Permission::inherit ? juce::String ("Indefinido") : p.raw;
        r.cells = { group, permissionLabel (p.group, p.key), value, effect };
        if (resolved == Permission::yes)
            r.statusOk = true;
        rows.push_back (r);
    }
    permissions_.setRows (rows);
}

void OperatorsView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void OperatorsView::resized()
{
    auto r = getLocalBounds();
    banner_.setBounds (r.removeFromTop (banner_.preferredHeight()));
    list_.setBounds (r.removeFromLeft (260));
    permissions_.setBounds (r.reduced (16, 12));
}

// ============================================================================
// Diagnostics

DiagnosticsView::DiagnosticsView (AppContext& context) : View (context)
{
    addAndMakeVisible (severity_);
    severity_.addItem ("Erros e avisos", 1);
    severity_.addItem ("Somente erros", 2);
    severity_.addItem ("Tudo", 3);
    severity_.setSelectedId (1, juce::dontSendNotification);
    severity_.onChange = [this] { refresh(); };
    addAndMakeVisible (filter_);
    filter_.setTextToShowWhenEmpty (L"Filtrar...", colours::textMuted);
    filter_.setFont (font (14.0f));
    filter_.setIndents (8, 7);
    filter_.onTextChange = [this] { table_.setFilter (filter_.getText()); };
    addAndMakeVisible (runButton_);
    runButton_.onClick = [this] { run(); };
    addAndMakeVisible (openButton_);
    openButton_.onClick = [this] { if (current_.has_value()) openInEditor (*current_); };
    openButton_.setEnabled (false);
    table_.setColumns ({ { "Problema", 560 }, { "Arquivo", 190 }, { "Linha", 60 }, { "Tipo", 150 } });
    table_.onSelect = [this] (const Table::Row& r) {
        if (r.tag >= 0 && r.tag < (int) all_.size())
            show (all_.items()[(size_t) r.tag]);
    };
    table_.onDoubleClick = [this] (const Table::Row& r) {
        if (r.tag >= 0 && r.tag < (int) all_.size())
            openInEditor (all_.items()[(size_t) r.tag]);
    };
    addAndMakeVisible (table_);
    addAndMakeVisible (detail_);
    styleReadOnlyText (detail_);
    run();
}

juce::String DiagnosticsView::subtitle() const
{
    return juce::String (all_.count (Severity::error)) + " erro(s), " + juce::String (all_.count (Severity::warning))
         + L" aviso(s) · verificado às " + ranAt_.formatted ("%H:%M:%S");
}

void DiagnosticsView::run()
{
    all_ = ctx.workspace.runFullDiagnostics();
    ranAt_ = juce::Time::getCurrentTime();
    refresh();
    ctx.status (L"Diagnóstico concluído: " + subtitle());
    if (auto* parent = getParentComponent())
        parent->repaint();
}

void DiagnosticsView::refresh()
{
    std::vector<Table::Row> rows;
    auto mode = severity_.getSelectedId();
    for (int i = 0; i < (int) all_.size(); ++i)
    {
        auto& d = all_.items()[(size_t) i];
        if ((mode == 1 && d.severity == Severity::info) || (mode == 2 && d.severity != Severity::error))
            continue;
        Table::Row r;
        r.status = d.severity;
        r.cells = { d.message, d.file.getFileName(), d.line > 0 ? juce::String (d.line) : juce::String(), toDisplayString (d.severity) };
        r.tag = i;
        r.tooltip = d.reason;
        rows.push_back (r);
    }
    table_.setRows (rows);
    table_.setFilter (filter_.getText());
}

void DiagnosticsView::show (const Diagnostic& d)
{
    current_ = d;
    juce::String t;
    t << toUpperLatin (toDisplayString (d.severity)) << "\n" << d.message << "\n\n";
    t << L"Onde: " << d.file.getFullPathName() << (d.line > 0 ? L", linha " + juce::String (d.line) : juce::String()) << "\n";
    if (d.excerpt.isNotEmpty())
        t << "Trecho: " << d.excerpt << "\n";
    t << L"\nPor quê: " << d.reason << L"\n\nComo resolver: " << d.fix;
    detail_.setText (t);
    openButton_.setEnabled (true);
}

void DiagnosticsView::openInEditor (const Diagnostic& d)
{
    auto parent = d.file.getParentDirectory().getFileName();
    auto name = d.file.getFileName().toLowerCase();
    if (parent.equalsIgnoreCase ("Mapas") || parent.equalsIgnoreCase ("Grades"))
    {
        auto clock = d.file.getFileName().startsWithIgnoreCase ("Relogio");
        ctx.navigate (clock ? ViewId::clocks : parent.equalsIgnoreCase ("Mapas") ? ViewId::maps : ViewId::grades, d.file, d.line);
    }
    else if (name == "playlist.ini")
        ctx.navigate (ViewId::playlistIni, {}, 0);
    else if (name == "config.xml")
        ctx.navigate (ViewId::config, {}, 0);
    else if (name == "folders.xml" || name == "ligacao.dbf")
        ctx.navigate (ViewId::folders, {}, 0);
    else
        inform (L"Arquivo sem editor", d.file.getFullPathName() + L"\n\nEste arquivo é gerado por outro programa e é apenas verificado pelo Playlist Control.");
}

void DiagnosticsView::openFile (const juce::File& file, int line)
{
    for (auto& d : all_.items())
        if (d.file == file && (line == 0 || d.line == line))
        {
            show (d);
            break;
        }
}

void DiagnosticsView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void DiagnosticsView::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto top = r.removeFromTop (32);
    severity_.setBounds (top.removeFromLeft (200));
    top.removeFromLeft (10);
    filter_.setBounds (top.removeFromLeft (360));
    runButton_.setBounds (top.removeFromRight (170));
    top.removeFromRight (8);
    openButton_.setBounds (top.removeFromRight (150));
    r.removeFromTop (10);
    detail_.setBounds (r.removeFromBottom (170));
    r.removeFromBottom (10);
    table_.setBounds (r);
}

// ============================================================================
// History

class HistoryView::DiffView : public juce::Component
{
public:
    void setDiff (std::vector<DiffLine> lines)
    {
        lines_ = std::move (lines);
        setSize (getWidth(), juce::jmax (40, (int) lines_.size() * 20 + 10));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
        if (lines_.empty())
        {
            g.setColour (colours::textMuted);
            g.setFont (font (13.0f));
            g.drawText (L"Selecione uma alteração para ver a diferença.", getLocalBounds().reduced (10), juce::Justification::topLeft, true);
            return;
        }
        auto clip = g.getClipBounds();
        g.setFont (monoFont (13.0f));
        for (int i = 0; i < (int) lines_.size(); ++i)
        {
            juce::Rectangle<int> row (0, 5 + i * 20, getWidth(), 20);
            if (! row.intersects (clip))
                continue;
            auto& l = lines_[(size_t) i];
            auto bg = l.kind == DiffLine::Kind::added ? colours::okBack : l.kind == DiffLine::Kind::removed ? colours::errorBack : colours::panel;
            g.setColour (bg);
            g.fillRect (row);
            g.setColour (colours::textMuted);
            g.drawText (l.oldLine > 0 ? juce::String (l.oldLine) : juce::String(), row.removeFromLeft (50), juce::Justification::centredRight, false);
            g.drawText (l.newLine > 0 ? juce::String (l.newLine) : juce::String(), row.removeFromLeft (50), juce::Justification::centredRight, false);
            g.setColour (l.kind == DiffLine::Kind::added ? colours::ok : l.kind == DiffLine::Kind::removed ? colours::error : colours::textMuted);
            g.drawText (l.kind == DiffLine::Kind::added ? "+" : l.kind == DiffLine::Kind::removed ? "-" : " ", row.removeFromLeft (24),
                        juce::Justification::centred, false);
            g.setColour (colours::text);
            g.drawText (l.text, row, juce::Justification::centredLeft, false);
        }
    }

private:
    std::vector<DiffLine> lines_;
};

HistoryView::HistoryView (AppContext& context) : View (context)
{
    table_.setColumns ({ { "Data", 150 }, { "Arquivo", 200 }, { L"Operação", 150 }, { "Resumo", 520 }, { L"Usuário", 110 } });
    table_.onSelect = [this] (const Table::Row& r) { show (r.tag); };
    addAndMakeVisible (table_);
    addAndMakeVisible (restoreButton_);
    restoreButton_.onClick = [this] { restore(); };
    restoreButton_.setEnabled (false);
    addAndMakeVisible (folderButton_);
    folderButton_.onClick = [this] { ctx.workspace.history().root().revealToUser(); };
    addAndMakeVisible (summary_);
    styleLabel (summary_, 13.0f, false, colours::textMuted);
    diff_ = std::make_unique<DiffView>();
    diffViewport_.setViewedComponent (diff_.get(), false);
    addAndMakeVisible (diffViewport_);
    refresh();
}

void HistoryView::refresh()
{
    entries_ = ctx.workspace.history().list (500);
    std::vector<Table::Row> rows;
    for (int i = 0; i < (int) entries_.size(); ++i)
    {
        auto& e = entries_[(size_t) i];
        Table::Row r;
        r.cells = { e.time.formatted ("%d/%m/%Y %H:%M:%S"), e.target.getFileName(), e.operation, e.summary, e.user };
        r.tag = i;
        r.tooltip = e.target.getFullPathName() + "\n" + e.summary;
        rows.push_back (r);
    }
    table_.setRows (rows);
    current_ = -1;
    restoreButton_.setEnabled (false);
    diff_->setDiff ({});
    if (! entries_.empty())
        table_.selectRow (0);
    summary_.setText (entries_.empty() ? juce::String (L"Nenhuma alteração registrada ainda.")
                                       : juce::String ((int) entries_.size()) + L" alteração(ões) em " + ctx.workspace.history().root().getFullPathName(),
                      juce::dontSendNotification);
}

void HistoryView::show (int index)
{
    if (index < 0 || index >= (int) entries_.size())
        return;
    current_ = index;
    auto& e = entries_[(size_t) index];
    juce::MemoryBlock before, after;
    auto& h = ctx.workspace.history();
    juce::StringArray a, b;
    if (h.readBefore (e, before))
        for (auto& l : TextLines::fromBytes (before).lines)
            a.add (l.content);
    if (h.readAfter (e, after))
        for (auto& l : TextLines::fromBytes (after).lines)
            b.add (l.content);
    diff_->setSize (diffViewport_.getMaximumVisibleWidth(), diff_->getHeight());
    diff_->setDiff (compactDiff (diffLines (a, b), 3));
    restoreButton_.setEnabled (e.hadBefore && ! ctx.workspace.readOnly());
    summary_.setText (e.target.getFullPathName() + L"  ·  " + e.operation + L"  ·  " + e.time.formatted ("%d/%m/%Y %H:%M:%S")
                          + (e.restoredFrom.isNotEmpty() ? L"  ·  restauração" : juce::String()),
                      juce::dontSendNotification);
}

void HistoryView::restore()
{
    if (current_ < 0 || current_ >= (int) entries_.size())
        return;
    auto e = entries_[(size_t) current_];
    confirm (L"Restaurar versão anterior",
             L"O arquivo " + e.target.getFileName() + L" voltará ao estado de antes de \"" + e.operation + L"\" em "
                 + e.time.formatted ("%d/%m/%Y %H:%M") + L".\n\nO conteúdo atual também será guardado no Histórico.",
             "Restaurar", [this, e] {
                 juce::MemoryBlock before;
                 if (! ctx.workspace.history().readBefore (e, before))
                 {
                     inform (L"Cópia indisponível", L"A cópia de segurança desta alteração não pôde ser lida ou não confere com o registro.", true);
                     return;
                 }
                 WriteRequest req;
                 req.target = e.target;
                 req.content = before;
                 req.operation = "Restaurar";
                 req.summary = L"Restaurado o estado anterior a \"" + e.operation + L"\" de " + e.time.formatted ("%d/%m/%Y %H:%M");
                 req.restoredFrom = e.id;
                 req.expectedBase = FileSnapshot::take (e.target);
                 auto r = ctx.workspace.writer().write (req);
                 if (! r.succeeded())
                 {
                     inform (L"Não foi possível restaurar", r.message, true);
                     return;
                 }
                 ctx.workspace.addActivity (e.target.getFileName() + " restaurado", e.target);
                 ctx.status (e.target.getFileName() + L" restaurado. A versão substituída está no Histórico.");
                 ctx.workspace.reload();
                 refresh();
             });
}

void HistoryView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void HistoryView::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto top = r.removeFromTop (32);
    restoreButton_.setBounds (top.removeFromRight (220));
    top.removeFromRight (8);
    folderButton_.setBounds (top.removeFromRight (200));
    summary_.setBounds (top);
    r.removeFromTop (10);
    table_.setBounds (r.removeFromTop (r.getHeight() / 2));
    r.removeFromTop (10);
    diffViewport_.setBounds (r);
    diff_->setSize (diffViewport_.getMaximumVisibleWidth(), diff_->getHeight());
}

} // namespace pc::ui
