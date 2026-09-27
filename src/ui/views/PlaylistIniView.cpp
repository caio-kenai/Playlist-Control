#include "ui/views/PlaylistIniView.h"
#include "formats/playlistini/FilePatternResolver.h"

namespace pc::ui
{

using namespace theme;

// One [BLOCO ...] / [RELOGIO ...] section with a preview of the next days.
struct PlaylistIniView::SourceEditor : public juce::Component
{
    SourceEditor (PlaylistIniView& o, ScheduleKind k) : owner (o), kind (k)
    {
        addAndMakeVisible (format);
        format.addItem (L"Não configurado", 1);
        format.addItem (L"AUTO — busca automática", 2);
        format.addItem (L"TXT1 — arquivo definido pelo padrão", 3);
        format.onChange = [this] { apply(); };
        addAndMakeVisible (pattern);
        pattern.setFont (monoFont (14.0f));
        pattern.setIndents (6, 6);
        pattern.setTextToShowWhenEmpty (L"ex.: MAPAS\\%d-%m-%Y.TXT", colours::textMuted);
        pattern.onReturnKey = [this] { apply(); };
        pattern.onFocusLost = [this] { apply(); };
        pattern.onTextChange = [this] { preview(); };
        addAndMakeVisible (help);
        styleLabel (help, 12.0f, false, colours::textMuted);
        help.setText (L"Variáveis: %d dia, %m mês, %Y ano (4 dígitos), %y ano (2), %a dia da semana (Seg…Dom), %w número do dia (Seg=1 … Dom=7)",
                      juce::dontSendNotification);
    }

    void apply()
    {
        if (updating || ! owner.ini_.has_value())
            return;
        auto current = owner.ini_->source (kind);
        auto id = format.getSelectedId();
        auto newPattern = pattern.getText().trim();
        if (id == 1)
        {
            if (! current.sectionPresent)
                return;
            owner.ini_->removeSource (kind);
            owner.edited (L"[" + sectionName (kind) + L"] removida");
            return;
        }
        auto f = id == 2 ? ScheduleFormat::automatic : ScheduleFormat::txt1;
        if (current.sectionPresent && current.format == f && current.pattern == newPattern)
            return;
        owner.ini_->setSource (kind, f, newPattern);
        owner.edited ("[" + sectionName (kind) + "] FORMATO=" + PlaylistIni::formatToIniValue (f)
                      + (newPattern.isNotEmpty() ? " ARQUIVO=" + newPattern : juce::String()));
    }

    void load()
    {
        updating = true;
        auto s = owner.ini_.has_value() ? owner.ini_->source (kind) : ScheduleSource {};
        format.setSelectedId (! s.sectionPresent ? 1 : s.format == ScheduleFormat::automatic ? 2 : s.format == ScheduleFormat::txt1 ? 3 : 1,
                              juce::dontSendNotification);
        pattern.setText (s.pattern, false);
        updating = false;
        preview();
    }

    void preview()
    {
        rows.clear();
        if (! owner.ini_.has_value())
        {
            repaint();
            return;
        }
        auto s = owner.ini_->source (kind);
        if (format.getSelectedId() == 3)
        {
            s.format = ScheduleFormat::txt1;
            s.pattern = pattern.getText().trim();
        }
        else if (format.getSelectedId() == 2)
            s.format = ScheduleFormat::automatic;
        else
            s.format = ScheduleFormat::missing;
        auto today = Date::today();
        for (int i = 0; i < 7; ++i)
        {
            auto d = today.addDays (i);
            Row r;
            r.date = d;
            auto candidates = scheduleCandidates (s, d, owner.ctx.workspace.pgm());
            for (auto& c : candidates)
            {
                if (c.exists)
                {
                    r.file = c.file.getRelativePathFrom (owner.ctx.workspace.pgm());
                    r.rule = c.rule;
                    r.found = true;
                    break;
                }
            }
            if (! r.found && ! candidates.empty())
                r.file = candidates.front().file.getRelativePathFrom (owner.ctx.workspace.pgm());
            rows.push_back (r);
        }
        unknownVars = unknownPatternVariables (pattern.getText());
        pattern.setColour (juce::TextEditor::outlineColourId, unknownVars.isEmpty() ? colours::border : colours::error);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        bool commercial = kind == ScheduleKind::commercial || kind == ScheduleKind::commercialClock;
        g.setColour (commercial ? colours::commercial : colours::musical);
        g.fillRoundedRectangle (juce::Rectangle<float> (14, 14, 4, 16), 2.0f);
        g.setColour (colours::text);
        g.setFont (font (15.0f, true));
        g.drawText (toDisplayString (kind), 24, 10, getWidth() - 40, 24, juce::Justification::centredLeft, false);
        g.setColour (colours::textMuted);
        g.setFont (monoFont (12.5f));
        g.drawText ("[" + sectionName (kind) + "]", 24, 10, getWidth() - 40, 24, juce::Justification::centredRight, false);

        auto list = previewArea;
        if (! unknownVars.isEmpty())
        {
            g.setColour (colours::error);
            g.setFont (font (12.5f));
            g.drawText (L"Variável desconhecida: " + unknownVars.joinIntoString (", "), list.removeFromTop (20), juce::Justification::centredLeft, true);
        }
        if (rows.empty() || format.getSelectedId() == 1)
        {
            g.setColour (colours::textMuted);
            g.setFont (font (12.5f));
            g.drawText (kind == ScheduleKind::commercialClock || kind == ScheduleKind::musicalClock
                            ? juce::String (L"Sem relógio operacional configurado.")
                            : juce::String (L"Sem configuração: o comportamento do Playlist não é documentado."),
                        list.removeFromTop (22), juce::Justification::centredLeft, true);
            return;
        }
        static const char* const days[] = { "Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb" };
        for (auto& row : rows)
        {
            auto line = list.removeFromTop (22);
            drawStatusIcon (g, line.removeFromLeft (20).toFloat().withSizeKeepingCentre (12, 12), Severity::error, row.found);
            g.setColour (colours::textMuted);
            g.setFont (font (12.5f));
            g.drawText (juce::String::fromUTF8 (days[row.date.dayOfWeek()]) + " " + row.date.toString().substring (0, 5),
                        line.removeFromLeft (72), juce::Justification::centredLeft, false);
            g.setColour (row.found ? colours::text : colours::error);
            g.setFont (monoFont (12.5f));
            g.drawText (row.found ? row.file : row.file + L"  (não existe)", line, juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16).withTrimmedTop (30);
        format.setBounds (r.removeFromTop (30).removeFromLeft (320));
        r.removeFromTop (6);
        pattern.setBounds (r.removeFromTop (30));
        help.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        previewArea = r;
    }

    struct Row
    {
        Date date;
        juce::String file, rule;
        bool found = false;
    };

    PlaylistIniView& owner;
    ScheduleKind kind;
    juce::ComboBox format;
    juce::TextEditor pattern;
    juce::Label help;
    std::vector<Row> rows;
    juce::StringArray unknownVars;
    juce::Rectangle<int> previewArea;
    bool updating = false;
};

// [AFILIADAS]: stations that receive the PLAY/STOP commands sent by this one
// (network head). One row per affiliate: name, host or IP, data port.
struct PlaylistIniView::AffiliatesEditor : public juce::Component
{
    static constexpr int rowHeight = 40;
    static constexpr int defaultPort = 3030;

    struct Row : public juce::Component
    {
        explicit Row (AffiliatesEditor& o) : owner (o)
        {
            for (auto* e : { &name, &host, &port })
            {
                addAndMakeVisible (*e);
                e->setFont (font (14.0f));
                e->setIndents (8, 7);
                e->onFocusLost = [this] { owner.apply(); };
                e->onReturnKey = [this] { owner.apply(); };
                e->onTextChange = [this] { owner.validate(); };
            }
            name.setTextToShowWhenEmpty ("ex.: FABRICIANO", colours::textMuted);
            host.setTextToShowWhenEmpty (L"IP ou nome da máquina", colours::textMuted);
            port.setTextToShowWhenEmpty (juce::String (defaultPort), colours::textMuted);
            port.setInputRestrictions (5, "0123456789");
            addAndMakeVisible (remove);
            remove.setTooltip ("Remover esta afiliada");
            remove.onClick = [this] { owner.removeRow (this); };
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (0, 4);
            remove.setBounds (r.removeFromRight (r.getHeight()));
            r.removeFromRight (8);
            port.setBounds (r.removeFromRight (90));
            r.removeFromRight (8);
            name.setBounds (r.removeFromLeft (r.getWidth() * 2 / 5));
            r.removeFromLeft (8);
            host.setBounds (r);
        }

        // Address as written in the INI: host:port (port defaults to 3030).
        juce::String address() const
        {
            auto p = port.getText().trim();
            return host.getText().trim() + ":" + (p.isEmpty() ? juce::String (defaultPort) : p);
        }

        bool isBlank() const { return name.getText().trim().isEmpty() && host.getText().trim().isEmpty(); }

        AffiliatesEditor& owner;
        juce::TextEditor name, host, port;
        ActionButton remove { {}, Icon::trash, ActionButton::Style::danger };
    };

    explicit AffiliatesEditor (PlaylistIniView& o) : owner (o)
    {
        addAndMakeVisible (add);
        add.onClick = [this] {
            auto* row = rows.add (new Row (*this));
            addAndMakeVisible (row);
            owner.layoutContent();
            row->name.grabKeyboardFocus();
        };
    }

    void load (const std::vector<Affiliate>& list, bool editable)
    {
        rows.clear();
        for (auto& a : list)
        {
            auto* row = rows.add (new Row (*this));
            addAndMakeVisible (row);
            row->name.setText (a.id, false);
            auto hasPort = a.address.containsChar (':');
            row->host.setText (hasPort ? a.address.upToLastOccurrenceOf (":", false, false) : a.address, false);
            row->port.setText (hasPort ? a.address.fromLastOccurrenceOf (":", false, false) : juce::String(), false);
        }
        setEditable (editable);
        validate();
    }

    void setEditable (bool editable)
    {
        editable_ = editable;
        add.setEnabled (editable);
        for (auto* row : rows)
            for (auto* c : std::initializer_list<juce::Component*> { &row->name, &row->host, &row->port, &row->remove })
                c->setEnabled (editable);
    }

    int preferredHeight() const
    {
        return 106 + juce::jmax (1, rows.size()) * rowHeight + 12 + 34 + 16;
    }

    // Problem of one row, empty when the row is valid (or still blank).
    juce::String problemOf (const Row& row) const
    {
        if (row.isBlank())
            return {};
        auto n = row.name.getText().trim();
        auto h = row.host.getText().trim();
        auto p = row.port.getText().trim();
        if (n.isEmpty())
            return "Informe o nome da afiliada.";
        if (n.containsAnyOf ("=[];"))
            return L"O nome não pode conter = [ ] ;";
        for (auto* other : rows)
            if (other != &row && other->name.getText().trim().equalsIgnoreCase (n))
                return "Nome repetido: " + n + ".";
        if (h.isEmpty())
            return L"Informe o IP ou o nome da máquina de " + n + ".";
        if (h.containsAnyOf (" :=;"))
            return L"Endereço inválido: " + h + ".";
        if (p.isNotEmpty() && (p.getIntValue() < 1 || p.getIntValue() > 65535))
            return L"Porta inválida: " + p + " (1 a 65535).";
        return {};
    }

    void validate()
    {
        problems.clear();
        for (auto* row : rows)
        {
            auto problem = problemOf (*row);
            auto colour = problem.isEmpty() ? colours::border : colours::error;
            for (auto* e : { &row->name, &row->host, &row->port })
            {
                e->setColour (juce::TextEditor::outlineColourId, colour);
                e->repaint();
            }
            if (problem.isNotEmpty())
                problems.add (problem);
        }
        repaint();
    }

    // Writes the valid rows to the document when they differ from it.
    void apply()
    {
        validate();
        if (! owner.ini_.has_value() || ! editable_ || ! problems.isEmpty())
            return;
        std::vector<Affiliate> list;
        for (auto* row : rows)
            if (! row->isBlank())
                list.push_back ({ row->name.getText().trim(), row->address() });
        auto before = owner.ini_->affiliates();
        bool same = before.size() == list.size();
        for (size_t i = 0; same && i < list.size(); ++i)
            same = before[i].id == list[i].id && before[i].address == list[i].address;
        if (same)
            return;
        owner.ini_->setAffiliates (list);
        owner.edited (list.size() < before.size() ? juce::String ("Afiliada removida")
                                                  : juce::String ("[AFILIADAS] ") + juce::String ((int) list.size()) + " afiliada(s)");
    }

    void removeRow (Row* row)
    {
        rows.removeObject (row);
        owner.layoutContent();
        apply();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (colours::brandLight);
        g.fillRoundedRectangle (juce::Rectangle<float> (14, 14, 4, 16), 2.0f);
        g.setColour (colours::text);
        g.setFont (font (15.0f, true));
        g.drawText ("Afiliadas de rede", 24, 10, getWidth() - 40, 24, juce::Justification::centredLeft, false);
        g.setColour (colours::textMuted);
        g.setFont (monoFont (12.5f));
        g.drawText ("[AFILIADAS]", 24, 10, getWidth() - 40, 24, juce::Justification::centredRight, false);
        g.setFont (font (12.5f));
        g.drawFittedText (L"Emissoras que recebem os comandos PLAY/STOP enviados por esta (cabeça de rede). "
                          L"Não cadastre a própria emissora. A porta é a porta de dados do Playlist da afiliada (padrão 3030).",
                          juce::Rectangle<int> (16, 38, getWidth() - 32, 44), juce::Justification::topLeft, 3, 1.0f);

        auto header = juce::Rectangle<int> (16, 84, getWidth() - 32, 20);
        g.setFont (font (11.5f, true));
        g.setColour (colours::textMuted);
        auto cols = header;
        cols.removeFromRight (rowHeight - 8 + 8);
        g.drawText ("PORTA", cols.removeFromRight (90).withTrimmedLeft (8), juce::Justification::centredLeft, false);
        cols.removeFromRight (8);
        auto nameCol = cols.removeFromLeft (cols.getWidth() * 2 / 5);
        g.drawText ("NOME", nameCol.withTrimmedLeft (8), juce::Justification::centredLeft, false);
        g.drawText (L"ENDEREÇO (IP OU MÁQUINA)", cols.withTrimmedLeft (16), juce::Justification::centredLeft, false);

        if (rows.isEmpty())
        {
            g.setFont (font (13.0f));
            g.drawText ("Nenhuma afiliada cadastrada.", juce::Rectangle<int> (16, 106, getWidth() - 32, rowHeight),
                        juce::Justification::centredLeft, false);
        }
        if (! problems.isEmpty())
        {
            g.setColour (colours::error);
            g.setFont (font (12.5f));
            g.drawText (problems[0], add.getBounds().withLeft (add.getRight() + 14).withRight (getWidth() - 16),
                        juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 0).withTrimmedTop (106);
        for (auto* row : rows)
            row->setBounds (r.removeFromTop (rowHeight));
        if (rows.isEmpty())
            r.removeFromTop (rowHeight);
        r.removeFromTop (12);
        add.setBounds (r.removeFromTop (34).withWidth (add.preferredWidth (34)));
    }

    PlaylistIniView& owner;
    juce::OwnedArray<Row> rows;
    ActionButton add { "Adicionar afiliada", Icon::plus, ActionButton::Style::secondary };
    juce::StringArray problems;
    bool editable_ = false;
};

PlaylistIniView::PlaylistIniView (AppContext& context) : View (context)
{
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (&content_, false);
    viewport_.setScrollBarsShown (true, false);

    for (auto k : { ScheduleKind::commercial, ScheduleKind::musical, ScheduleKind::commercialClock, ScheduleKind::musicalClock })
        content_.addAndMakeVisible (sources_.add (new SourceEditor (*this, k)));

    for (auto* l : { &beepTitle_, &othersTitle_, &problemsTitle_ })
    {
        content_.addAndMakeVisible (*l);
        styleLabel (*l, 15.0f, true);
    }
    beepTitle_.setText ("Beep [BEEP]", juce::dontSendNotification);
    othersTitle_.setText (L"Outras seções (mantidas como estão)", juce::dontSendNotification);
    problemsTitle_.setText (L"Verificação", juce::dontSendNotification);
    content_.addAndMakeVisible (beepHelp_);
    styleLabel (beepHelp_, 12.0f, false, colours::textMuted);
    beepHelp_.setText (L"Arquivo (relativo à pasta pgm ou caminho completo) e minutos da hora em que toca, ex.: 0,15,30,45.",
                       juce::dontSendNotification);

    affiliates_ = std::make_unique<AffiliatesEditor> (*this);
    content_.addAndMakeVisible (*affiliates_);

    content_.addAndMakeVisible (beepEnabled_);
    content_.addAndMakeVisible (beepFile_);
    content_.addAndMakeVisible (beepMinutes_);
    beepFile_.setTextToShowWhenEmpty ("BEEP.MP3", colours::textMuted);
    beepMinutes_.setTextToShowWhenEmpty ("0,15,30,45", colours::textMuted);
    for (auto* e : { &beepFile_, &beepMinutes_ })
    {
        e->setFont (font (14.0f));
        e->setIndents (6, 6);
    }
    auto applyBeep = [this] {
        if (! ini_.has_value())
            return;
        auto b = ini_->beep();
        if (! beepEnabled_.getToggleState())
        {
            if (b.present)
            {
                ini_->removeBeep();
                edited (L"[BEEP] removido");
            }
            return;
        }
        if (b.present && b.file == beepFile_.getText().trim() && b.minutesRaw == beepMinutes_.getText().trim())
            return;
        ini_->setBeep (beepFile_.getText(), beepMinutes_.getText());
        edited (L"[BEEP] ARQUIVO=" + beepFile_.getText().trim() + " HORARIO=" + beepMinutes_.getText().trim());
    };
    beepEnabled_.onClick = applyBeep;
    beepFile_.onFocusLost = applyBeep;
    beepMinutes_.onFocusLost = applyBeep;

    content_.addAndMakeVisible (others_);
    styleReadOnlyText (others_, true);
    content_.addAndMakeVisible (problems_);
    styleReadOnlyText (problems_);

    addAndMakeVisible (fileTitle_);
    styleLabel (fileTitle_, 16.0f, true);
    addAndMakeVisible (fileInfo_);
    styleLabel (fileInfo_, 12.5f, false, colours::textMuted);
    for (auto* b : { &saveButton_, &discardButton_ })
        addAndMakeVisible (*b);
    addChildComponent (createButton_);
    makePrimary (saveButton_);
    saveButton_.onClick = [this] { save(); };
    discardButton_.onClick = [this] { discard(); };
    createButton_.onClick = [this] {
        if (ctx.workspace.readOnly())
        {
            inform (L"Modo somente leitura", L"Ative \"Permitir alterações\" no topo da janela.");
            return;
        }
        PlaylistIni fresh;
        fresh.setSource (ScheduleKind::commercial, ScheduleFormat::automatic, {});
        fresh.setSource (ScheduleKind::musical, ScheduleFormat::automatic, {});
        juce::MemoryBlock bytes;
        fresh.document().toBytes (bytes);
        WriteRequest req;
        req.target = ctx.workspace.pgm().getChildFile ("PLAYLIST.ini");
        req.content = bytes;
        req.operation = L"Criar PLAYLIST.ini";
        req.summary = L"Mapas e grades com FORMATO=AUTO (configuração de instalação do manual)";
        req.expectedBase = FileSnapshot {};
        auto r = ctx.workspace.writer().write (req);
        if (r.succeeded())
        {
            ctx.workspace.addActivity (L"PLAYLIST.ini criado", req.target);
            ctx.workspace.reload();
            load();
        }
        else
            inform (L"Não foi possível criar o arquivo", r.message, true);
    };
    addChildComponent (banner_);
    load();
}

PlaylistIniView::~PlaylistIniView() = default;

void PlaylistIniView::load()
{
    session_ = FileSession (ctx.workspace.pgm().getChildFile ("PLAYLIST.ini"));
    juce::String err;
    ini_.reset();
    if (ctx.workspace.isOpen() && session_.load (err))
        ini_ = PlaylistIni (IniDocument::fromBytes (session_.bytes()));
    dirty_ = false;
    changes_.clear();
    pushToControls();
}

void PlaylistIniView::pushToControls()
{
    bool has = ini_.has_value();
    bool editable = has && ! ctx.workspace.readOnly();
    createButton_.setVisible (ctx.workspace.isOpen() && ! has);
    for (auto* s : sources_)
    {
        s->load();
        s->format.setEnabled (editable);
        s->pattern.setEnabled (editable);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &beepEnabled_, &beepFile_, &beepMinutes_ })
        c->setEnabled (editable);
    affiliates_->load (has ? ini_->affiliates() : std::vector<Affiliate> {}, editable);

    if (has)
    {
        auto b = ini_->beep();
        beepEnabled_.setToggleState (b.present, juce::dontSendNotification);
        beepFile_.setText (b.file, false);
        beepMinutes_.setText (b.minutesRaw, false);

        juce::String other;
        auto& doc = ini_->document();
        for (auto& name : doc.sectionNames())
        {
            if (PlaylistIni::isKnownSection (name))
                continue;
            other << "[" << name << "]\n";
            for (auto& [k, v] : doc.entries (name))
                other << k << "=" << v << "\n";
            other << "\n";
        }
        for (auto& name : doc.commentedSectionNames())
            other << ";[" << name << L"]   (seção desativada com ';')\n";
        others_.setText (other.isEmpty() ? juce::String (L"Nenhuma.") : other.trimEnd());
    }
    updatePreview();
}

void PlaylistIniView::updatePreview()
{
    diagnostics_ = {};
    if (ini_.has_value())
        diagnostics_ = validatePlaylistIni (*ini_, session_.file(), ctx.workspace.pgm(), Date::today());
    problems_.setText (diagnostics_.empty() ? juce::String (L"Nenhum problema encontrado.") : diagnostics_.toText (30));
    fileTitle_.setText (ini_.has_value() ? juce::String ("PLAYLIST.ini") + (dirty_ ? "  *" : "") : juce::String (L"PLAYLIST.ini não encontrado"),
                        juce::dontSendNotification);
    fileInfo_.setText (ini_.has_value() ? session_.file().getFullPathName() + "  ·  " + toDisplayString (ini_->document().encoding())
                                        : juce::String (L"Sem o arquivo, o Playlist usa a busca padrão."),
                       juce::dontSendNotification);
    saveButton_.setEnabled (dirty_ && ! ctx.workspace.readOnly());
    discardButton_.setEnabled (dirty_);
    if (ini_.has_value())
        banner_.show (Severity::info, L"O manual não informa se o Playlist relê o PLAYLIST.ini sem reiniciar. Após salvar, reinicie o Playlist Digital para garantir.");
    else
        banner_.hideBanner();
    resized();
}

void PlaylistIniView::edited (const juce::String& what)
{
    dirty_ = true;
    changes_.add (what);
    for (auto* s : sources_)
        s->preview();
    updatePreview();
    ctx.status (what);
}

void PlaylistIniView::save()
{
    if (! dirty_ || ! ini_.has_value())
        return;
    if (ctx.workspace.readOnly())
    {
        inform (L"Modo somente leitura", L"Ative \"Permitir alterações\" no topo da janela para gravar.");
        return;
    }
    if (diagnostics_.hasErrors())
    {
        inform (L"Não é possível salvar", L"Corrija os erros antes de salvar:\n\n" + diagnostics_.toText (10), true);
        return;
    }
    juce::MemoryBlock bytes;
    juce::juce_wchar bad = 0;
    if (! ini_->document().toBytes (bytes, &bad))
    {
        inform (L"Caractere não suportado", L"O caractere \"" + juce::String::charToString (bad) + L"\" não pode ser gravado neste arquivo.", true);
        return;
    }
    auto expected = *ini_;
    auto r = session_.save (ctx.workspace.writer(), bytes, "Editar PLAYLIST.ini", changes_.joinIntoString ("; "),
                            [expected] (const juce::MemoryBlock& b) {
                                PlaylistIni check (IniDocument::fromBytes (b));
                                for (auto k : { ScheduleKind::commercial, ScheduleKind::musical, ScheduleKind::commercialClock, ScheduleKind::musicalClock })
                                {
                                    auto a = check.source (k), e = expected.source (k);
                                    if (a.format != e.format || a.pattern != e.pattern || a.sectionPresent != e.sectionPresent)
                                        return juce::String (L"A releitura do arquivo não confere com as alterações.");
                                }
                                return juce::String();
                            });
    if (r.succeeded())
    {
        ctx.workspace.addActivity (L"PLAYLIST.ini salvo: " + changes_.joinIntoString ("; "), session_.file());
        dirty_ = false;
        changes_.clear();
        ctx.status (L"PLAYLIST.ini salvo com cópia de segurança. Reinicie o Playlist Digital para aplicar.");
        ctx.workspace.reload();
        load();
    }
    else if (r.status == WriteStatus::conflict)
        inform (L"Arquivo alterado por outro programa", r.message + L"\n\nUse \"Descartar\" para recarregar.", true);
    else
        inform (L"Não foi possível salvar", r.message, true);
}

void PlaylistIniView::discard()
{
    load();
    ctx.status (L"Alterações descartadas.");
}

void PlaylistIniView::refresh()
{
    if (! dirty_)
        load();
}

void PlaylistIniView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void PlaylistIniView::layoutContent()
{
    auto width = viewport_.getMaximumVisibleWidth();
    auto r = juce::Rectangle<int> (0, 0, width, 10000).reduced (16, 12);
    auto half = (r.getWidth() - 12) / 2;
    auto cardHeight = 300;
    for (int i = 0; i < sources_.size(); i += 2)
    {
        auto row = r.removeFromTop (cardHeight);
        sources_[i]->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (12);
        if (i + 1 < sources_.size())
            sources_[i + 1]->setBounds (row);
        r.removeFromTop (12);
    }
    auto row = r.removeFromTop (juce::jmax (190, affiliates_->preferredHeight()));
    auto left = row.removeFromLeft (half);
    affiliates_->setBounds (left);
    row.removeFromLeft (12);
    row.removeFromTop (6);
    beepTitle_.setBounds (row.removeFromTop (26));
    beepHelp_.setBounds (row.removeFromTop (20));
    beepEnabled_.setBounds (row.removeFromTop (28));
    auto fields = row.removeFromTop (30);
    beepFile_.setBounds (fields.removeFromLeft (fields.getWidth() / 2 - 6));
    fields.removeFromLeft (12);
    beepMinutes_.setBounds (fields);
    r.removeFromTop (12);

    row = r.removeFromTop (220);
    left = row.removeFromLeft (half);
    othersTitle_.setBounds (left.removeFromTop (26));
    others_.setBounds (left);
    row.removeFromLeft (12);
    problemsTitle_.setBounds (row.removeFromTop (26));
    problems_.setBounds (row);
    content_.setSize (width, r.getY() + 12);
}

void PlaylistIniView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (64).reduced (16, 6);
    auto top = header.removeFromTop (32);
    saveButton_.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    discardButton_.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    createButton_.setBounds (top.removeFromRight (160));
    fileTitle_.setBounds (top);
    fileInfo_.setBounds (header);
    banner_.setBounds (r.removeFromTop (banner_.preferredHeight()));
    viewport_.setBounds (r);
    layoutContent();
}

} // namespace pc::ui
