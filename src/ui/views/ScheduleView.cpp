#include "ui/views/ScheduleView.h"
#include "formats/playlistini/FilePatternResolver.h"
#include "logging/Logger.h"

#include <set>

namespace pc::ui
{

using namespace theme;

namespace
{
constexpr int bandWidth = 22;
constexpr int headerRowHeight = 28;
constexpr int itemRowHeight = 24;

juce::Colour originColour (FileOrigin o)
{
    switch (o)
    {
        case FileOrigin::planner:    return juce::Colour (0xff6a3fb5);
        case FileOrigin::commercial: return colours::commercial;
        case FileOrigin::maker:      return colours::musical;
        case FileOrigin::electoral:  return juce::Colour (0xff8a5a00);
        case FileOrigin::manual:     return colours::textMuted;
        case FileOrigin::unknown:    return colours::border;
    }
    return colours::textMuted;
}

bool isCommercialKind (ScheduleKind k)
{
    return k == ScheduleKind::commercial || k == ScheduleKind::commercialClock;
}

juce::String weekdayName (const Date& d)
{
    static const char* const names[] = { "domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado" };
    return juce::String::fromUTF8 (names[d.dayOfWeek()]);
}

// Structural problems that break how the Playlist reads the file.
bool isStructural (const juce::String& code)
{
    return code == "schedule.line" || code == "schedule.time.order" || code == "schedule.param.dur"
        || code == "schedule.param.localsat" || code == "schedule.param.empty";
}
} // namespace

// ============================================================================
// Block list: blocks painted like the Playlist programming panel.

class BlockList : public juce::Component
{
public:
    explicit BlockList (ScheduleView& owner) : owner_ (owner) {}

    struct Layout
    {
        int line = -1;
        int y = 0, height = 0;
    };

    void rebuild (int width)
    {
        layout_.clear();
        resolutions_.clear();
        auto* doc = owner_.document();
        int y = 8;
        if (doc != nullptr)
        {
            auto date = owner_.documentDate();
            for (int i = 0; i < (int) doc->lines().size(); ++i)
            {
                auto& l = doc->lines()[(size_t) i];
                if (l.kind == ScheduleLine::Kind::blank)
                    continue;
                Layout lay;
                lay.line = i;
                lay.y = y;
                if (l.kind == ScheduleLine::Kind::invalid)
                    lay.height = itemRowHeight + 4;
                else
                    lay.height = headerRowHeight + juce::jmax (1, (int) l.block.items.size()) * itemRowHeight + 2;
                std::vector<ItemResolution> res;
                for (auto& it : l.block.items)
                    res.push_back (owner_.catalog().resolve (it, date));
                resolutions_[i] = std::move (res);
                layout_.push_back (lay);
                y += lay.height + 8;
            }
        }
        setSize (width, juce::jmax (y + 8, 200));
        repaint();
    }

    const Layout* layoutFor (int line) const
    {
        for (auto& l : layout_)
            if (l.line == line)
                return &l;
        return nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::background);
        auto* doc = owner_.document();
        if (doc == nullptr)
        {
            g.setColour (colours::textMuted);
            g.setFont (font (15.0f));
            g.drawFittedText (L"Selecione um arquivo na lista à esquerda.", getLocalBounds().reduced (30), juce::Justification::centredTop, 2);
            return;
        }
        auto clip = g.getClipBounds();
        bool commercial = isCommercialKind (owner_.kind());
        auto band = commercial ? colours::commercial : colours::musical;
        auto bandText = commercial ? colours::commercialText : colours::musicalText;
        auto alt = commercial ? colours::commercialAlt : colours::musicalAlt;

        for (auto& lay : layout_)
        {
            juce::Rectangle<int> box (12, lay.y, getWidth() - 24, lay.height);
            if (! box.intersects (clip))
                continue;
            auto& line = doc->lines()[(size_t) lay.line];
            bool selectedBlock = lay.line == owner_.selectedLine();

            if (line.kind == ScheduleLine::Kind::invalid)
            {
                g.setColour (colours::errorBack);
                g.fillRect (box);
                g.setColour (colours::error);
                g.drawRect (box);
                drawStatusIcon (g, box.removeFromLeft (26).toFloat().withSizeKeepingCentre (14, 14), Severity::error, false);
                g.setFont (monoFont (13.0f));
                g.drawText (L"Linha " + juce::String (lay.line + 1) + L" sem horário válido: " + line.raw, box,
                            juce::Justification::centredLeft, true);
                continue;
            }

            auto& b = line.block;
            g.setColour (colours::panel);
            g.fillRect (box);

            // Vertical band with the block type, as in the Playlist.
            auto bandArea = box.removeFromLeft (bandWidth);
            g.setColour (band);
            g.fillRect (bandArea);
            if (bandArea.getHeight() > 60)
            {
                juce::Graphics::ScopedSaveState state (g);
                g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                                 (float) bandArea.getCentreX(), (float) bandArea.getCentreY()));
                g.setColour (bandText);
                g.setFont (font (12.0f, true));
                auto text = commercial ? juce::String ("Comercial") : juce::String ("Musical");
                g.drawText (text, juce::Rectangle<int> (bandArea.getCentreX() - bandArea.getHeight() / 2,
                                                        bandArea.getCentreY() - bandWidth / 2, bandArea.getHeight(), bandWidth),
                            juce::Justification::centred, false);
            }

            // Header: time, parameters and problems.
            auto header = box.removeFromTop (headerRowHeight);
            g.setColour (band.withAlpha (0.1f));
            g.fillRect (header);
            auto h = header.reduced (8, 0);
            g.setColour (colours::text);
            g.setFont (font (15.0f, true));
            g.drawText (b.time.toString(), h.removeFromLeft (54), juce::Justification::centredLeft, false);
            auto chips = h.toFloat().withSizeKeepingCentre ((float) h.getWidth(), 18.0f);
            auto chip = [&] (const juce::String& text, juce::Colour bg, juce::Colour fg) {
                auto w = badgeWidth (text);
                if (w > chips.getWidth())
                    return;
                drawBadge (g, chips.removeFromLeft (w), text, bg, fg);
                chips.removeFromLeft (5);
            };
            if (auto id = b.params.value ("ID"))
                chip (*id, colours::brand, juce::Colours::white);
            if (auto dur = b.params.value ("DUR"))
            {
                auto d = parseDuration (*dur);
                chip ("DUR " + (d.valid ? formatDuration (d.seconds) : *dur), d.valid ? colours::panelAlt : colours::errorBack,
                      d.valid ? colours::text : colours::error);
            }
            if (b.params.has ("FIXO"))     chip ("F", juce::Colour (0xffffd400), colours::text);
            if (b.params.has ("SAT"))      chip ("SAT", juce::Colour (0xff6a3fb5), juce::Colours::white);
            if (b.params.has ("LOCAL"))    chip ("LOCAL", colours::brandLight, juce::Colours::white);
            if (b.params.has ("LOCKED"))   chip ("BLOQUEADO", juce::Colour (0xffffd400), colours::text);
            if (b.params.has ("DESCARTE")) chip ("DESCARTE", colours::panelAlt, colours::text);
            for (auto& p : b.params.items())
                if (! BlockParams::isKnown (p.name))
                    chip (p.toString(), colours::warningBack, colours::warning);

            auto& diags = owner_.diagnosticsForLine (lay.line);
            int errors = 0, warnings = 0;
            for (auto& d : diags)
                (d.severity == Severity::error ? errors : warnings) += d.severity == Severity::info ? 0 : 1;
            auto right = header.reduced (8, 5).toFloat();
            if (errors + warnings > 0)
            {
                auto text = errors > 0 ? juce::String (errors) + (errors == 1 ? " erro" : " erros")
                                       : juce::String (warnings) + (warnings == 1 ? " aviso" : " avisos");
                auto s = errors > 0 ? Severity::error : Severity::warning;
                drawBadge (g, right.removeFromRight (badgeWidth (text)), text, severityBackground (s), severityColour (s));
            }

            // Items
            auto& res = resolutions_[lay.line];
            if (b.items.empty())
            {
                auto row = box.removeFromTop (itemRowHeight).withTrimmedLeft (34);
                g.setColour (colours::textMuted);
                g.setFont (font (13.0f).italicised());
                g.drawText (owner_.kind() == ScheduleKind::commercialClock || owner_.kind() == ScheduleKind::musicalClock
                                ? juce::String (L"Horário do relógio")
                                : juce::String (L"Bloco vazio — montado manualmente pelo operador"),
                            row, juce::Justification::centredLeft, true);
            }
            for (int i = 0; i < (int) b.items.size(); ++i)
            {
                auto row = box.removeFromTop (itemRowHeight);
                bool selectedItem = selectedBlock && i == owner_.selectedItem();
                g.setColour (selectedItem ? colours::selection : (i % 2 == 1 ? alt : colours::panel));
                g.fillRect (row);
                auto& it = b.items[(size_t) i];
                auto& r = res[(size_t) i];
                auto icon = row.removeFromLeft (28).toFloat().withSizeKeepingCentre (13, 13);
                bool ok = r.status == ItemStatus::ok;
                if (r.status == ItemStatus::notChecked)
                {
                    g.setColour (colours::textMuted);
                    g.fillEllipse (icon.withSizeKeepingCentre (6, 6));
                }
                else
                    drawStatusIcon (g, icon, Severity::error, ok);

                g.setColour (selectedItem ? juce::Colours::white : colours::text);
                g.setFont (font (13.5f, it.kind == ItemKind::code));
                auto main = row.removeFromLeft (juce::jmin (row.getWidth() / 2, 360));
                g.drawText (it.kind == ItemKind::empty ? juce::String ("(vazio)") : it.displayText(), main,
                            juce::Justification::centredLeft, true);
                g.setColour (selectedItem ? colours::selectionText : (ok || r.status == ItemStatus::notChecked ? colours::textMuted : colours::error));
                g.setFont (font (12.5f));
                juce::String detail;
                switch (r.status)
                {
                    case ItemStatus::ok:            detail = it.kind == ItemKind::code ? r.description : L"em " + r.foundIn.joinIntoString (", "); break;
                    case ItemStatus::unknownCode:   detail = L"código não registrado"; break;
                    case ItemStatus::outOfValidity: detail = L"registro fora da validade: " + r.description; break;
                    case ItemStatus::fileMissing:   detail = L"arquivo não encontrado: " + r.description; break;
                    case ItemStatus::notChecked:    detail = it.kind == ItemKind::command ? juce::String ("comando") : juce::String(); break;
                }
                g.drawText (detail, row.reduced (8, 0), juce::Justification::centredLeft, true);
            }

            g.setColour (selectedBlock ? colours::brandLight : colours::border);
            g.drawRect (juce::Rectangle<int> (12, lay.y, getWidth() - 24, lay.height), selectedBlock ? 2 : 1);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& lay : layout_)
        {
            if (e.y < lay.y || e.y >= lay.y + lay.height)
                continue;
            int item = -1;
            auto rel = e.y - lay.y - headerRowHeight;
            if (rel >= 0)
                item = rel / itemRowHeight;
            auto* doc = owner_.document();
            if (doc != nullptr && item >= (int) doc->lines()[(size_t) lay.line].block.items.size())
                item = -1;
            owner_.selectLine (lay.line, item);
            return;
        }
    }

private:
    ScheduleView& owner_;
    std::vector<Layout> layout_;
    std::map<int, std::vector<ItemResolution>> resolutions_;
};

// ============================================================================
// Inspector: edits the selected block.

class BlockInspector : public juce::Component, private juce::ListBoxModel
{
public:
    explicit BlockInspector (ScheduleView& owner) : owner_ (owner)
    {
        for (auto* l : { &title_, &timeLabel_, &idLabel_, &durLabel_, &itemsLabel_, &itemLabel_ })
        {
            addAndMakeVisible (*l);
            styleLabel (*l, 13.0f, false, colours::textMuted);
        }
        styleLabel (title_, 15.0f, true);
        title_.setText (L"Bloco", juce::dontSendNotification);
        timeLabel_.setText (L"Horário", juce::dontSendNotification);
        idLabel_.setText ("Nome do bloco (ID)", juce::dontSendNotification);
        durLabel_.setText (L"Duração (DUR)", juce::dontSendNotification);
        itemsLabel_.setText ("Itens", juce::dontSendNotification);
        itemLabel_.setText ("Item", juce::dontSendNotification);

        for (auto* e : { &time_, &id_, &dur_, &itemValue_ })
        {
            addAndMakeVisible (*e);
            e->setFont (font (14.0f));
            e->setIndents (6, 5);
        }
        time_.setInputRestrictions (5, "0123456789:");
        dur_.setTextToShowWhenEmpty (L"m:ss ou segundos", colours::textMuted);
        id_.setTextToShowWhenEmpty (L"sem nome", colours::textMuted);
        itemValue_.setTextToShowWhenEmpty (L"código, nome do arquivo ou comando", colours::textMuted);
        time_.onReturnKey = [this] { applyTime(); };
        time_.onFocusLost = [this] { applyTime(); };
        id_.onReturnKey = [this] { applyParam ("ID", id_.getText()); };
        id_.onFocusLost = [this] { applyParam ("ID", id_.getText()); };
        dur_.onReturnKey = [this] { applyParam ("DUR", dur_.getText()); };
        dur_.onFocusLost = [this] { applyParam ("DUR", dur_.getText()); };
        itemValue_.onReturnKey = [this] { replaceItem(); };

        const char* paramNames[] = { "FIXO", "LOCAL", "SAT", "LOCKED", "DESCARTE" };
        const char* labels[] = { "FIXO", "LOCAL", "SAT", "Bloqueado", "Descarte" };
        const char* tips[] = { "Bloco comercial que não atrasa: corta o musical e entra no horário exato.",
                               "Bloco local (afiliadas de rede).",
                               "Bloco satélite: começa por comando remoto (afiliadas de rede).",
                               "Bloqueia o bloco: nenhum operador pode mover, excluir ou adicionar.",
                               "Aplica o descarte de inserções que ultrapassam o horário do próximo bloco." };
        for (int i = 0; i < 5; ++i)
        {
            auto* t = flags_.add (new juce::ToggleButton (labels[i]));
            t->setTooltip (juce::String::fromUTF8 (tips[i]));
            t->getProperties().set ("param", paramNames[i]);
            addAndMakeVisible (t);
            t->onClick = [this, t] { toggleFlag (t->getProperties()["param"].toString(), t->getToggleState()); };
        }

        kind_.addItem (L"Código", 1);
        kind_.addItem ("Arquivo", 2);
        kind_.addItem (L"Código | arquivo", 3);
        kind_.addItem ("Comando", 4);
        kind_.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (kind_);

        addAndMakeVisible (items_);
        items_.setModel (this);
        items_.setRowHeight (24);
        items_.setOutlineThickness (1);

        for (auto* b : { &add_, &replace_, &remove_, &up_, &down_, &codes_ })
            addAndMakeVisible (*b);
        add_.onClick = [this] { addItem(); };
        replace_.onClick = [this] { replaceItem(); };
        remove_.onClick = [this] { removeItem(); };
        up_.onClick = [this] { moveItem (-1); };
        down_.onClick = [this] { moveItem (1); };
        codes_.onClick = [this] { showCodes(); };
        codes_.setTooltip (L"Escolher entre as pastas e os códigos registrados");

        addAndMakeVisible (resolution_);
        styleReadOnlyText (resolution_);
        addAndMakeVisible (unknownParams_);
        styleLabel (unknownParams_, 12.5f, false, colours::warning);
    }

    void setClockMode (bool clock)
    {
        clockMode_ = clock;
        for (auto* c : std::initializer_list<juce::Component*> { &itemsLabel_, &items_, &itemLabel_, &kind_, &itemValue_, &codes_,
                                                                 &add_, &replace_, &remove_, &up_, &down_, &resolution_ })
            c->setVisible (! clock);
        resized();
    }

    void update()
    {
        auto* doc = owner_.document();
        auto line = owner_.selectedLine();
        bool hasBlock = doc != nullptr && line >= 0 && line < (int) doc->lines().size()
                     && doc->lines()[(size_t) line].kind == ScheduleLine::Kind::block;
        bool editable = hasBlock && owner_.editable();
        for (auto* c : std::initializer_list<juce::Component*> { &time_, &id_, &dur_, &itemValue_, &kind_, &add_, &replace_,
                                                                 &remove_, &up_, &down_, &codes_ })
            c->setEnabled (editable);
        for (auto* f : flags_)
            f->setEnabled (editable);
        items_.updateContent();

        if (! hasBlock)
        {
            title_.setText (L"Nenhum bloco selecionado", juce::dontSendNotification);
            for (auto* e : { &time_, &id_, &dur_ })
                e->setText ({}, false);
            resolution_.setText ({});
            unknownParams_.setText ({}, juce::dontSendNotification);
            repaint();
            return;
        }
        auto& b = doc->lines()[(size_t) line].block;
        title_.setText (L"Bloco das " + b.time.toString() + L"  ·  linha " + juce::String (line + 1), juce::dontSendNotification);
        time_.setText (b.time.toString(), false);
        id_.setText (b.params.value ("ID").value_or (""), false);
        dur_.setText (b.params.value ("DUR").value_or (""), false);
        for (auto* f : flags_)
            f->setToggleState (b.params.has (f->getProperties()["param"].toString()), juce::dontSendNotification);
        juce::StringArray unknown;
        for (auto& p : b.params.items())
            if (! BlockParams::isKnown (p.name))
                unknown.add (p.toString());
        unknownParams_.setText (unknown.isEmpty() ? juce::String()
                                                  : L"Parâmetros não documentados (mantidos): " + unknown.joinIntoString (", "),
                                juce::dontSendNotification);

        auto item = owner_.selectedItem();
        if (item >= 0 && item < (int) b.items.size())
        {
            items_.selectRow (item, false, true);
            auto& it = b.items[(size_t) item];
            switch (it.kind)
            {
                case ItemKind::code:        kind_.setSelectedId (1, juce::dontSendNotification); itemValue_.setText (it.code, false); break;
                case ItemKind::quotedFile:
                case ItemKind::bareText:    kind_.setSelectedId (2, juce::dontSendNotification); itemValue_.setText (it.file, false); break;
                case ItemKind::codeAndFile: kind_.setSelectedId (3, juce::dontSendNotification); itemValue_.setText (it.code + "|" + it.file, false); break;
                case ItemKind::command:     kind_.setSelectedId (4, juce::dontSendNotification); itemValue_.setText (it.code, false); break;
                case ItemKind::empty:       itemValue_.setText ({}, false); break;
            }
            auto r = owner_.catalog().resolve (it, owner_.documentDate());
            juce::String text;
            switch (r.status)
            {
                case ItemStatus::ok:            text = L"Toca: " + r.description; break;
                case ItemStatus::unknownCode:   text = L"O código não está registrado. O Playlist mostra um X vermelho e o item não vai ao ar."; break;
                case ItemStatus::outOfValidity: text = L"O registro existe, mas está fora da validade nesta data. O item não vai ao ar."; break;
                case ItemStatus::fileMissing:   text = L"Arquivo não encontrado nas pastas cadastradas: " + r.description; break;
                case ItemStatus::notChecked:    text = it.kind == ItemKind::command ? juce::String (L"Comando entre < >; não verificado pelo Playlist Control.")
                                                                                   : juce::String (L"Item vazio."); break;
            }
            if (! r.foundIn.isEmpty())
                text << "\n" << L"Encontrado em: " << r.foundIn.joinIntoString (", ");
            for (auto* reg : r.registrations)
            {
                text << "\n" << L"Registro: " << reg->code << " -> " << reg->file;
                if (reg->validFrom.has_value() || reg->validTo.has_value())
                    text << L" (validade " << (reg->validFrom ? reg->validFrom->toString() : juce::String ("-")) << L" até "
                         << (reg->validTo ? reg->validTo->toString() : juce::String ("-")) << ")";
            }
            resolution_.setText (text);
        }
        else
        {
            items_.deselectAllRows();
            resolution_.setText ({});
        }
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        title_.setBounds (r.removeFromTop (26));
        r.removeFromTop (4);
        auto row = r.removeFromTop (20);
        timeLabel_.setBounds (row.removeFromLeft (80));
        durLabel_.setBounds (row);
        row = r.removeFromTop (30);
        time_.setBounds (row.removeFromLeft (70));
        row.removeFromLeft (10);
        dur_.setBounds (row);
        r.removeFromTop (6);
        idLabel_.setBounds (r.removeFromTop (20));
        id_.setBounds (r.removeFromTop (30));
        r.removeFromTop (6);
        auto flagArea = r.removeFromTop (56);
        auto top = flagArea.removeFromTop (28);
        auto bottom = flagArea;
        for (int i = 0; i < flags_.size(); ++i)
        {
            auto& area = i < 3 ? top : bottom;
            flags_[i]->setBounds (area.removeFromLeft (i < 3 ? 86 : 110));
        }
        unknownParams_.setBounds (r.removeFromTop (unknownParams_.getText().isEmpty() ? 0 : 34));
        r.removeFromTop (8);
        itemsLabel_.setBounds (r.removeFromTop (20));
        auto resolution = r.removeFromBottom (96);
        resolution_.setBounds (resolution);
        r.removeFromBottom (8);
        auto editor = r.removeFromBottom (96);
        items_.setBounds (r);
        itemLabel_.setBounds (editor.removeFromTop (20));
        row = editor.removeFromTop (30);
        kind_.setBounds (row.removeFromLeft (132));
        row.removeFromLeft (6);
        codes_.setBounds (row.removeFromRight (30));
        row.removeFromRight (4);
        itemValue_.setBounds (row);
        editor.removeFromTop (6);
        row = editor.removeFromTop (30);
        auto w = (row.getWidth() - 4 * 6) / 5;
        for (auto* b : { &add_, &replace_, &remove_, &up_, &down_ })
        {
            b->setBounds (row.removeFromLeft (b == &up_ || b == &down_ ? w / 2 + 10 : w + 8));
            row.removeFromLeft (6);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
        g.setColour (colours::border);
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
    }

private:
    int getNumRows() override
    {
        auto* b = block();
        return b != nullptr ? (int) b->items.size() : 0;
    }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        auto* b = block();
        if (b == nullptr || row >= (int) b->items.size())
            return;
        g.fillAll (selected ? colours::selection : (row % 2 ? colours::panelAlt : colours::panel));
        auto& it = b->items[(size_t) row];
        g.setColour (selected ? juce::Colours::white : colours::textMuted);
        g.setFont (font (11.5f));
        g.drawText (juce::String (row + 1), 4, 0, 22, height, juce::Justification::centredRight, false);
        g.setColour (selected ? juce::Colours::white : colours::text);
        g.setFont (font (13.0f));
        g.drawText (it.displayText(), 32, 0, width - 36, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        owner_.selectLine (owner_.selectedLine(), row);
    }

    ScheduleBlock* block()
    {
        auto* doc = owner_.document();
        auto line = owner_.selectedLine();
        if (doc == nullptr || line < 0 || line >= (int) doc->lines().size()
            || doc->lines()[(size_t) line].kind != ScheduleLine::Kind::block)
            return nullptr;
        return &doc->lines()[(size_t) line].block;
    }

    void edited (const juce::String& what)
    {
        owner_.documentEdited (owner_.selectedLine(), what);
    }

    void applyTime()
    {
        auto* doc = owner_.document();
        auto* b = block();
        if (b == nullptr || ! owner_.editable())
            return;
        auto parsed = TimeOfDay::parsePrefix (time_.getText().trim());
        if (! parsed.time.has_value() || parsed.length != time_.getText().trim().length())
        {
            time_.setText (b->time.toString(), false);
            owner_.status (L"Horário inválido. Use HH:MM entre 00:00 e 23:59.");
            return;
        }
        if (*parsed.time == b->time)
            return;
        if (doc->findBlock (*parsed.time) >= 0)
        {
            time_.setText (b->time.toString(), false);
            owner_.status (L"Já existe um bloco às " + parsed.time->toString() + ".");
            return;
        }
        auto moved = *b;
        auto old = b->time;
        moved.time = *parsed.time;
        doc->removeLine (owner_.selectedLine());
        auto index = doc->insertBlock (moved);
        owner_.selectLine (index, -1);
        owner_.documentEdited (index, L"Bloco " + old.toString() + L" movido para " + moved.time.toString());
    }

    void applyParam (const juce::String& name, const juce::String& value)
    {
        auto* b = block();
        if (b == nullptr || ! owner_.editable())
            return;
        if (b->params.value (name).value_or ("") == value.trim())
            return;
        b->params.setValue (name, value);
        edited (name + L" do bloco " + b->time.toString() + (value.trim().isEmpty() ? L" removido" : " = " + value.trim()));
    }

    void toggleFlag (const juce::String& name, bool on)
    {
        auto* b = block();
        if (b == nullptr)
            return;
        b->params.setFlag (name, on);
        if (on && name == "LOCAL") b->params.setFlag ("SAT", false);
        if (on && name == "SAT")   b->params.setFlag ("LOCAL", false);
        edited (name + (on ? L" ativado" : L" desativado") + L" no bloco " + b->time.toString());
    }

    std::optional<ScheduleItem> itemFromEditor()
    {
        auto v = itemValue_.getText().trim();
        if (v.isEmpty())
            return std::nullopt;
        switch (kind_.getSelectedId())
        {
            case 1: return ScheduleItem::makeCode (v);
            case 2: return ScheduleItem::makeFile (v);
            case 3:
                if (! v.containsChar ('|'))
                    return std::nullopt;
                return ScheduleItem::makeCodeAndFile (v.upToFirstOccurrenceOf ("|", false, false),
                                                      v.fromFirstOccurrenceOf ("|", false, false));
            case 4: return ScheduleItem::makeCommand (v);
            default: return std::nullopt;
        }
    }

    void addItem()
    {
        auto* b = block();
        auto item = itemFromEditor();
        if (b == nullptr || ! item.has_value())
        {
            owner_.status (L"Informe o item a adicionar (no formato código|arquivo para esse tipo).");
            return;
        }
        auto at = owner_.selectedItem() >= 0 ? owner_.selectedItem() + 1 : (int) b->items.size();
        b->items.insert (b->items.begin() + at, *item);
        auto line = owner_.selectedLine();
        owner_.documentEdited (line, item->raw + L" adicionado ao bloco " + b->time.toString());
        owner_.selectLine (line, at);
    }

    void replaceItem()
    {
        auto* b = block();
        auto index = owner_.selectedItem();
        auto item = itemFromEditor();
        if (b == nullptr || index < 0 || ! item.has_value())
        {
            if (b != nullptr && index < 0)
                addItem();
            return;
        }
        auto old = b->items[(size_t) index].raw;
        if (old == item->raw)
            return;
        b->items[(size_t) index] = *item;
        auto line = owner_.selectedLine();
        owner_.documentEdited (line, old + L" substituído por " + item->raw + L" no bloco " + b->time.toString());
        owner_.selectLine (line, index);
    }

    void removeItem()
    {
        auto* b = block();
        auto index = owner_.selectedItem();
        if (b == nullptr || index < 0 || index >= (int) b->items.size())
            return;
        auto old = b->items[(size_t) index].raw;
        b->items.erase (b->items.begin() + index);
        auto line = owner_.selectedLine();
        owner_.documentEdited (line, old + L" removido do bloco " + b->time.toString());
        owner_.selectLine (line, juce::jmin (index, (int) b->items.size() - 1));
    }

    void moveItem (int delta)
    {
        auto* b = block();
        auto index = owner_.selectedItem();
        auto target = index + delta;
        if (b == nullptr || index < 0 || target < 0 || target >= (int) b->items.size())
            return;
        std::swap (b->items[(size_t) index], b->items[(size_t) target]);
        auto line = owner_.selectedLine();
        owner_.documentEdited (line, L"Ordem dos itens alterada no bloco " + b->time.toString());
        owner_.selectLine (line, target);
    }

    void showCodes()
    {
        juce::PopupMenu menu, folders, registrations;
        std::vector<juce::String> values;
        if (auto* f = owner_.catalog().folders())
        {
            for (auto& folder : f->folders())
            {
                if (folder.code.isEmpty())
                    continue;
                values.push_back (folder.code);
                folders.addItem ((int) values.size(), folder.code + "  -  " + folder.title + " (" + toDisplayString (folder.kind) + ")");
            }
        }
        juce::StringArray seen;
        for (auto& r : owner_.catalog().registrations())
        {
            if (r.deleted || r.type == "A" || seen.contains (r.normalized))
                continue;
            seen.add (r.normalized);
            values.push_back (r.code);
            registrations.addItem ((int) values.size(), r.code + "  -  " + r.file);
        }
        menu.addSubMenu ("Pastas", folders);
        menu.addSubMenu (L"Arquivos registrados", registrations);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (codes_), [this, values] (int result) {
            if (result <= 0)
                return;
            kind_.setSelectedId (1, juce::dontSendNotification);
            itemValue_.setText (values[(size_t) result - 1], false);
            itemValue_.grabKeyboardFocus();
        });
    }

    ScheduleView& owner_;
    bool clockMode_ = false;
    juce::Label title_, timeLabel_, idLabel_, durLabel_, itemsLabel_, itemLabel_, unknownParams_;
    juce::TextEditor time_, id_, dur_, itemValue_, resolution_;
    juce::OwnedArray<juce::ToggleButton> flags_;
    juce::ComboBox kind_;
    juce::ListBox items_;
    juce::TextButton add_ { "Adicionar" }, replace_ { "Substituir" }, remove_ { "Remover" }, up_ { juce::String::fromUTF8 ("\xe2\x96\xb2") },
        down_ { juce::String::fromUTF8 ("\xe2\x96\xbc") }, codes_ { "..." };
};

// ============================================================================
// Problems list

namespace
{
class ProblemsModel : public juce::ListBoxModel
{
public:
    ProblemsModel (const DiagnosticList& list, std::function<void (int)> onSelect) : list_ (list), onSelect_ (std::move (onSelect)) {}
    int getNumRows() override { return (int) list_.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row >= (int) list_.size())
            return;
        auto& d = list_.items()[(size_t) row];
        g.fillAll (selected ? colours::infoBack : colours::panel);
        drawStatusIcon (g, juce::Rectangle<float> (8, (float) height / 2 - 7, 14, 14), d.severity, false);
        g.setColour (colours::textMuted);
        g.setFont (font (12.5f));
        g.drawText (d.line > 0 ? "linha " + juce::String (d.line) : juce::String(), 30, 0, 70, height, juce::Justification::centredLeft, false);
        g.setColour (colours::text);
        g.setFont (font (13.0f));
        g.drawText (d.message + "  " + d.fix, 100, 0, width - 104, height, juce::Justification::centredLeft, true);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < (int) list_.size() && onSelect_)
            onSelect_ (list_.items()[(size_t) row].line - 1);
    }
    juce::String getTooltipForRow (int row) override
    {
        if (row >= (int) list_.size())
            return {};
        auto& d = list_.items()[(size_t) row];
        return d.message + "\n\n" + d.reason + "\n\n" + d.fix;
    }

private:
    const DiagnosticList& list_;
    std::function<void (int)> onSelect_;
};
} // namespace

// ============================================================================
// ScheduleView

ScheduleView::ScheduleView (AppContext& context, Mode mode)
    : View (context),
      mode_ (mode),
      kind_ (mode == Mode::maps ? ScheduleKind::commercial : mode == Mode::grades ? ScheduleKind::musical : ScheduleKind::commercialClock)
{
    addAndMakeVisible (fileList_);
    fileList_.setModel (this);
    fileList_.setRowHeight (46);
    fileList_.setOutlineThickness (0);
    addAndMakeVisible (newButton_);
    newButton_.onClick = [this] { createNewFile(); };
    newButton_.setTooltip (L"Cria um arquivo novo, vazio ou copiado de outro");

    addChildComponent (originBanner_);
    addChildComponent (conflictBanner_);

    addAndMakeVisible (fileTitle_);
    styleLabel (fileTitle_, 16.0f, true);
    addAndMakeVisible (fileInfo_);
    styleLabel (fileInfo_, 12.5f, false, colours::textMuted);

    for (auto* b : { &saveButton_, &discardButton_, &addBlockButton_, &removeBlockButton_ })
        addAndMakeVisible (*b);
    makePrimary (saveButton_);
    saveButton_.onClick = [this] { save(); };
    saveButton_.setTooltip (L"Grava com validação e cópia de segurança (Ctrl+S)");
    discardButton_.onClick = [this] { discard(); };
    addBlockButton_.onClick = [this] {
        if (! doc_.has_value() || ! editable())
            return;
        auto* w = new juce::AlertWindow ("Novo bloco", L"Horário do novo bloco (HH:MM):", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("time", "", L"Horário");
        w->addButton ("Criar", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<ScheduleView> self (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([self, w] (int result) {
            if (self == nullptr || result != 1)
                return;
            auto t = TimeOfDay::parsePrefix (w->getTextEditorContents ("time").trim());
            if (! t.time.has_value())
            {
                inform (L"Horário inválido", L"Use o formato HH:MM, entre 00:00 e 23:59.", true);
                return;
            }
            if (self->doc_->findBlock (*t.time) >= 0)
            {
                inform (L"Bloco existente", L"Já existe um bloco às " + t.time->toString() + ".", true);
                return;
            }
            ScheduleBlock b;
            b.time = *t.time;
            auto index = self->doc_->insertBlock (b);
            self->documentEdited (index, L"Bloco " + b.time.toString() + L" criado");
            self->selectLine (index);
        }), true);
    };
    removeBlockButton_.onClick = [this] {
        if (! doc_.has_value() || selectedLine_ < 0 || ! editable())
            return;
        auto& l = doc_->lines()[(size_t) selectedLine_];
        auto text = l.kind == ScheduleLine::Kind::block ? L"o bloco das " + l.block.time.toString() : L"a linha " + juce::String (selectedLine_ + 1);
        confirm (L"Remover bloco", L"Remover " + text + L" e todos os seus itens?", "Remover", [this, text] {
            if (! doc_.has_value() || selectedLine_ < 0)
                return;
            doc_->removeLine (selectedLine_);
            documentEdited (-1, juce::String (L"Removido ") + text);
            selectLine (-1);
        });
    };

    blocks_ = std::make_unique<BlockList> (*this);
    blocksViewport_.setViewedComponent (blocks_.get(), false);
    blocksViewport_.setScrollBarsShown (true, false);
    blocksViewport_.setScrollBarThickness (10);
    addAndMakeVisible (blocksViewport_);

    inspector_ = std::make_unique<BlockInspector> (*this);
    addAndMakeVisible (*inspector_);
    inspector_->setClockMode (mode == Mode::clocks);

    problemsModel_ = std::make_unique<ProblemsModel> (diagnostics_, [this] (int line) { selectLine (line); });
    problems_.setModel (problemsModel_.get());
    problems_.setRowHeight (26);
    problems_.setOutlineThickness (1);
    addAndMakeVisible (problems_);

    loadFiles();
    // Opens what the Playlist reads today.
    for (int i = 0; i < (int) files_.size(); ++i)
        if (files_[(size_t) i].active)
        {
            fileList_.selectRow (i);
            break;
        }
    if (! doc_.has_value() && ! files_.empty())
        fileList_.selectRow (0);
    startTimer (2000);
}

ScheduleView::~ScheduleView()
{
    fileList_.setModel (nullptr);
    problems_.setModel (nullptr);
}

juce::String ScheduleView::title() const
{
    switch (mode_)
    {
        case Mode::maps:   return "Mapas comerciais";
        case Mode::grades: return "Grades musicais";
        case Mode::clocks: return L"Relógios operacionais";
    }
    return {};
}

juce::String ScheduleView::subtitle() const
{
    switch (mode_)
    {
        case Mode::maps:   return L"Blocos comerciais lidos pelo Playlist Digital (pasta Mapas)";
        case Mode::grades: return L"Blocos musicais lidos pelo Playlist Digital (pasta Grades)";
        case Mode::clocks: return L"Horários e parâmetros dos blocos (ID, DUR, FIXO, SAT, LOCAL, LOCKED, DESCARTE)";
    }
    return {};
}

ScheduleKind ScheduleView::kindFor (const juce::File& file) const
{
    bool maps = file.getParentDirectory().getFileName().equalsIgnoreCase ("Mapas");
    if (mode_ == Mode::clocks)
        return maps ? ScheduleKind::commercialClock : ScheduleKind::musicalClock;
    return maps ? ScheduleKind::commercial : ScheduleKind::musical;
}

void ScheduleView::loadFiles()
{
    auto& ws = ctx.workspace;
    files_.clear();
    if (! ws.isOpen())
    {
        fileList_.updateContent();
        return;
    }
    std::vector<ScheduleFileInfo> infos;
    if (mode_ == Mode::clocks)
    {
        for (auto k : { ScheduleKind::commercial, ScheduleKind::musical })
            for (auto& i : ws.scheduleFiles (true, k))
                infos.push_back (i);
    }
    else
        infos = ws.scheduleFiles (false, mode_ == Mode::maps ? ScheduleKind::commercial : ScheduleKind::musical);

    auto today = Date::today();
    std::optional<juce::File> active;
    if (mode_ != Mode::clocks)
        if (auto a = ws.activeFile (mode_ == Mode::maps ? ScheduleKind::commercial : ScheduleKind::musical, today))
            active = a->file;

    for (auto& info : infos)
    {
        FileRow row;
        row.info = info;
        row.active = active.has_value() && *active == info.file;
        juce::MemoryBlock m;
        juce::String err;
        if (readFileShared (info.file, m, err))
        {
            auto doc = ScheduleDocument::fromBytes (m);
            row.origin = ws.originOf (info.file, doc, info.kind).origin;
            // Counting problems of every file is cheap enough for folders with
            // a few hundred files; past days are skipped.
            auto key = [] (const Date& d) { return d.year * 10000 + d.month * 100 + d.day; };
            if (! info.date.has_value() || key (*info.date) >= key (today))
                row.errors = validateSchedule (doc, info.file, info.kind, ws.validationContext (info)).count (Severity::error);
        }
        files_.push_back (row);
    }
    fileList_.updateContent();
    fileList_.repaint();
}

int ScheduleView::getNumRows()
{
    return (int) files_.size();
}

void ScheduleView::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row >= (int) files_.size())
        return;
    auto& f = files_[(size_t) row];
    g.fillAll (selected ? colours::infoBack : colours::panel);
    if (selected)
    {
        g.setColour (colours::brandLight);
        g.fillRect (0, 4, 3, height - 8);
    }
    auto band = isCommercialKind (f.info.kind) ? colours::commercial : colours::musical;
    auto r = juce::Rectangle<int> (12, 0, width - 20, height);
    auto right = r.removeFromRight (70).toFloat();
    g.setColour (colours::text);
    g.setFont (font (13.5f, true));
    g.drawText (f.info.file.getFileName(), r.removeFromTop (height / 2 + 2).withTrimmedTop (4), juce::Justification::bottomLeft, true);
    g.setColour (colours::textMuted);
    g.setFont (font (12.0f));
    juce::String sub = f.info.date.has_value() ? weekdayName (*f.info.date) : juce::String ("arquivo fixo");
    if (mode_ == Mode::clocks)
        sub = isCommercialKind (f.info.kind) ? juce::String ("pasta Mapas") : juce::String ("pasta Grades");
    sub << "  ·  " << toDisplayString (f.origin);
    g.drawText (sub, r, juce::Justification::topLeft, true);

    auto top = right.removeFromTop ((float) height / 2).reduced (0, 4);
    if (f.active)
        drawBadge (g, top.removeFromRight (badgeWidth ("HOJE")), "HOJE", band, juce::Colours::white);
    auto bottom = right.reduced (0, 4);
    if (f.errors > 0)
    {
        auto t = juce::String (f.errors);
        drawBadge (g, bottom.removeFromRight (badgeWidth (t)), t, colours::errorBack, colours::error);
    }
    g.setColour (colours::border);
    g.drawHorizontalLine (height - 1, 8.0f, (float) width - 8.0f);
}

void ScheduleView::selectedRowsChanged (int lastRowSelected)
{
    if (ignoreSelection_ || lastRowSelected < 0 || lastRowSelected >= (int) files_.size())
        return;
    auto file = files_[(size_t) lastRowSelected].info.file;
    if (file == session_.file())
        return;
    if (dirty_)
    {
        // Put the selection back until the operator decides.
        ignoreSelection_ = true;
        for (int i = 0; i < (int) files_.size(); ++i)
            if (files_[(size_t) i].info.file == session_.file())
                fileList_.selectRow (i);
        ignoreSelection_ = false;
        auto options = juce::MessageBoxOptions()
                           .withIconType (juce::MessageBoxIconType::QuestionIcon)
                           .withTitle (L"Alterações não salvas")
                           .withMessage (L"Descartar as alterações em " + session_.file().getFileName() + "?")
                           .withButton ("Descartar")
                           .withButton ("Cancelar");
        juce::Component::SafePointer<ScheduleView> self (this);
        juce::AlertWindow::showAsync (options, [self, file] (int result) {
            if (self == nullptr || result != 1)
                return;
            self->dirty_ = false;
            self->openFile (file, 0);
        });
        return;
    }
    loadDocument (file);
}

Date ScheduleView::documentDate() const
{
    return dateFromFileName (session_.file().getFileName()).value_or (Date::today());
}

bool ScheduleView::editable() const
{
    return doc_.has_value() && ! ctx.workspace.readOnly();
}

void ScheduleView::loadDocument (const juce::File& file)
{
    session_ = FileSession (file);
    juce::String err;
    doc_.reset();
    dirty_ = false;
    pendingChanges_.clear();
    selectedLine_ = selectedItem_ = -1;
    if (session_.load (err))
    {
        doc_ = ScheduleDocument::fromBytes (session_.bytes());
        kind_ = kindFor (file);
        origin_ = ctx.workspace.originOf (file, *doc_, kind_);
        Logger::instance().info ("schedule.open", file.getFileName(), { { "file", file.getFullPathName() } });
    }
    else
    {
        ctx.status (err);
    }
    revalidate();
    updateBanners();
    blocksViewport_.setViewPosition (0, 0);
    resized();
}

void ScheduleView::revalidate()
{
    diagnostics_ = {};
    lineDiagnostics_.clear();
    if (doc_.has_value())
    {
        ScheduleFileInfo info { session_.file(), kind_, dateFromFileName (session_.file().getFileName()),
                                mode_ == Mode::clocks };
        diagnostics_ = validateSchedule (*doc_, session_.file(), kind_, ctx.workspace.validationContext (info));
        lineDiagnostics_.resize (doc_->lines().size());
        for (auto& d : diagnostics_.items())
            if (d.line > 0 && d.line <= (int) lineDiagnostics_.size())
                lineDiagnostics_[(size_t) d.line - 1].push_back (d);
    }
    problems_.updateContent();
    problems_.repaint();
    blocks_->rebuild (blocksViewport_.getMaximumVisibleWidth());
    inspector_->update();

    auto name = session_.file().getFileName();
    fileTitle_.setText (doc_.has_value() ? name + (dirty_ ? "  *" : "") : juce::String(), juce::dontSendNotification);
    if (doc_.has_value())
    {
        juce::String info = juce::String ((int) doc_->blockIndexes().size()) + " blocos  ·  " + toDisplayString (doc_->encoding())
                          + "  ·  origem: " + toDisplayString (origin_.origin);
        info << "  ·  " << diagnostics_.count (Severity::error) << " erro(s), " << diagnostics_.count (Severity::warning) << " aviso(s)";
        fileInfo_.setText (info, juce::dontSendNotification);
    }
    else
        fileInfo_.setText ({}, juce::dontSendNotification);
    saveButton_.setEnabled (dirty_ && editable());
    discardButton_.setEnabled (dirty_);
    addBlockButton_.setEnabled (editable());
    removeBlockButton_.setEnabled (editable() && selectedLine_ >= 0);
}

void ScheduleView::updateBanners()
{
    if (doc_.has_value() && origin_.overwriteWarning.isNotEmpty())
        originBanner_.show (origin_.rewrittenAutomatically ? Severity::warning : Severity::info,
                            toDisplayString (origin_.origin) + ": " + origin_.overwriteWarning);
    else if (doc_.has_value() && origin_.origin != FileOrigin::manual)
        originBanner_.show (Severity::info, L"Origem: " + toDisplayString (origin_.origin) + ". " + origin_.reason);
    else
        originBanner_.hideBanner();
    resized();
}

const std::vector<Diagnostic>& ScheduleView::diagnosticsForLine (int lineIndex) const
{
    static const std::vector<Diagnostic> none;
    return lineIndex >= 0 && lineIndex < (int) lineDiagnostics_.size() ? lineDiagnostics_[(size_t) lineIndex] : none;
}

void ScheduleView::selectLine (int lineIndex, int itemIndex)
{
    selectedLine_ = lineIndex;
    selectedItem_ = itemIndex;
    blocks_->repaint();
    inspector_->update();
    removeBlockButton_.setEnabled (editable() && selectedLine_ >= 0);
    if (auto* lay = blocks_->layoutFor (lineIndex))
    {
        auto view = blocksViewport_.getViewArea();
        if (lay->y < view.getY() || lay->y + juce::jmin (lay->height, view.getHeight()) > view.getBottom())
            blocksViewport_.setViewPosition (0, juce::jmax (0, lay->y - 20));
    }
}

void ScheduleView::documentEdited (int lineIndex, const juce::String& description)
{
    if (! doc_.has_value())
        return;
    if (lineIndex >= 0 && lineIndex < (int) doc_->lines().size())
        doc_->markDirty (lineIndex);
    dirty_ = true;
    pendingChanges_.add (description);
    revalidate();
    resized();
    ctx.status (description);
}

void ScheduleView::save()
{
    if (! dirty_)
        return;
    if (ctx.workspace.readOnly())
    {
        inform (L"Modo somente leitura", L"Ative \"Permitir alterações\" no topo da janela para gravar.");
        return;
    }
    doSave();
}

void ScheduleView::doSave()
{
    juce::MemoryBlock bytes;
    juce::juce_wchar bad = 0;
    if (! doc_->toBytes (bytes, &bad))
    {
        inform (L"Caractere não suportado",
                L"O caractere \"" + juce::String::charToString (bad) + L"\" não pode ser gravado na codificação deste arquivo ("
                    + toDisplayString (doc_->encoding()) + L"). Remova-o ou use outro caractere.",
                true);
        return;
    }

    // Problems introduced by this edit.
    auto original = ScheduleDocument::fromBytes (session_.bytes());
    ScheduleFileInfo info { session_.file(), kind_, dateFromFileName (session_.file().getFileName()), mode_ == Mode::clocks };
    auto before = validateSchedule (original, session_.file(), kind_, ctx.workspace.validationContext (info));
    std::multiset<juce::String> known;
    for (auto& d : before.items())
        known.insert (d.code + "|" + d.message);
    juce::StringArray structural, newErrors;
    for (auto& d : diagnostics_.items())
    {
        auto key = d.code + "|" + d.message;
        if (auto it = known.find (key); it != known.end())
        {
            known.erase (it);
            continue;
        }
        if (d.severity != Severity::error)
            continue;
        (isStructural (d.code) ? structural : newErrors).add (d.locationText() + ": " + d.message);
    }
    if (! structural.isEmpty())
    {
        inform (L"Não é possível salvar",
                L"A alteração deixaria o arquivo fora do formato que o Playlist Digital lê:\n\n" + structural.joinIntoString ("\n")
                    + L"\n\nCorrija esses pontos antes de salvar.",
                true);
        return;
    }

    auto summary = pendingChanges_.joinIntoString ("; ");
    auto write = [this, bytes, summary] {
        auto kindText = mode_ == Mode::maps ? juce::String ("Editar mapa") : mode_ == Mode::grades ? juce::String ("Editar grade")
                                                                                                   : juce::String (L"Editar relógio");
        auto r = session_.save (ctx.workspace.writer(), bytes, kindText, summary, [] (const juce::MemoryBlock& b) {
            // The written bytes must parse back to exactly the same text.
            auto reparsed = ScheduleDocument::fromBytes (b);
            juce::MemoryBlock again;
            reparsed.toBytes (again);
            return again == b ? juce::String() : juce::String (L"O conteúdo gerado não é lido de volta de forma idêntica.");
        });
        switch (r.status)
        {
            case WriteStatus::written:
            case WriteStatus::unchanged:
                dirty_ = false;
                pendingChanges_.clear();
                ctx.workspace.addActivity (session_.file().getFileName() + " salvo: " + summary, session_.file());
                ctx.status (session_.file().getFileName() + L" salvo com cópia de segurança. O Playlist Digital relê o arquivo automaticamente.");
                conflictBanner_.hideBanner();
                revalidate();
                loadFiles();
                break;
            case WriteStatus::conflict:
                conflictBanner_.show (Severity::error, r.message, "Recarregar", [this] { dirty_ = false; loadDocument (session_.file()); });
                resized();
                break;
            case WriteStatus::readOnly:
            case WriteStatus::verifyFailed:
            case WriteStatus::ioError:
                inform (L"Não foi possível salvar", r.message, true);
                break;
        }
    };

    juce::StringArray warnings;
    if (origin_.rewrittenAutomatically)
        warnings.add (origin_.overwriteWarning);
    if (! newErrors.isEmpty())
        warnings.add (L"A alteração introduz problemas:\n" + newErrors.joinIntoString ("\n"));
    if (warnings.isEmpty())
        write();
    else
        confirm (L"Salvar mesmo assim?", warnings.joinIntoString ("\n\n"), "Salvar", write);
}

void ScheduleView::discard()
{
    if (! session_.loaded())
        return;
    doc_ = ScheduleDocument::fromBytes (session_.bytes());
    dirty_ = false;
    pendingChanges_.clear();
    selectLine (-1);
    revalidate();
    ctx.status (L"Alterações descartadas.");
}

void ScheduleView::openFile (const juce::File& file, int line)
{
    for (int i = 0; i < (int) files_.size(); ++i)
    {
        if (files_[(size_t) i].info.file == file)
        {
            ignoreSelection_ = true;
            fileList_.selectRow (i);
            ignoreSelection_ = false;
            fileList_.scrollToEnsureRowIsOnscreen (i);
        }
    }
    if (file != session_.file() || ! doc_.has_value())
        loadDocument (file);
    if (line > 0)
        selectLine (line - 1);
}

void ScheduleView::refresh()
{
    auto current = session_.file();
    loadFiles();
    if (doc_.has_value())
    {
        origin_ = ctx.workspace.originOf (current, *doc_, kind_);
        updateBanners();
        revalidate();
    }
}

void ScheduleView::timerCallback()
{
    if (! doc_.has_value() || ! session_.changedOnDisk())
        return;
    if (! dirty_)
    {
        auto line = selectedLine_;
        loadDocument (session_.file());
        selectLine (line);
        ctx.status (session_.file().getFileName() + L" foi alterado por outro programa e foi recarregado.");
        loadFiles();
        return;
    }
    if (! conflictBanner_.isVisible())
    {
        conflictBanner_.show (Severity::error,
                              session_.file().getFileName() + L" foi alterado por outro programa. Suas alterações ainda não foram salvas.",
                              L"Recarregar e descartar", [this] {
                                  dirty_ = false;
                                  loadDocument (session_.file());
                                  conflictBanner_.hideBanner();
                                  resized();
                              });
        resized();
    }
}

void ScheduleView::createNewFile()
{
    if (ctx.workspace.readOnly())
    {
        inform (L"Modo somente leitura", L"Ative \"Permitir alterações\" no topo da janela para criar arquivos.");
        return;
    }
    auto folder = ctx.workspace.pgm().getChildFile (mode_ == Mode::grades ? "Grades" : "Mapas");
    auto tomorrow = Date::today().addDays (1);
    juce::String suggested = mode_ == Mode::clocks ? juce::String ("Relogio.txt") : expandPattern ("%d-%m-%Y.txt", tomorrow);
    if (mode_ == Mode::maps)
    {
        auto src = ctx.workspace.source (ScheduleKind::commercial);
        if (src.format == ScheduleFormat::txt1 && src.pattern.isNotEmpty())
            suggested = juce::File (expandPattern (src.pattern, tomorrow).replaceCharacter ('\\', '/')).getFileName();
    }

    auto* w = new juce::AlertWindow ("Novo arquivo", L"O arquivo será criado na pasta " + folder.getFullPathName() + ".",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", suggested, "Nome do arquivo");
    juce::StringArray sources;
    sources.add (L"Vazio (só os horários do arquivo selecionado)");
    sources.add ("Vazio");
    for (auto& f : files_)
        sources.add (f.info.file.getFileName());
    w->addComboBox ("source", sources, L"Conteúdo inicial: cópia de");
    if (auto* combo = w->getComboBoxComponent ("source"))
        combo->setSelectedItemIndex (session_.loaded() ? 0 : 1);
    w->addButton ("Criar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<ScheduleView> self (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([self, w, folder] (int result) {
        if (self == nullptr || result != 1)
            return;
        auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
        if (! name.endsWithIgnoreCase (".txt"))
            name << ".txt";
        auto target = folder.getChildFile (name);
        if (target.exists())
        {
            inform (L"Arquivo existente", name + L" já existe. Abra-o na lista para editar.", true);
            return;
        }
        auto index = w->getComboBoxComponent ("source")->getSelectedItemIndex();
        juce::MemoryBlock content;
        juce::String description;
        if (index == 0 && self->doc_.has_value())
        {
            // Same block times and parameters, without items.
            auto copy = ScheduleDocument::fromBytes (self->session_.bytes());
            for (auto i : copy.blockIndexes())
            {
                copy.lines()[(size_t) i].block.items.clear();
                copy.markDirty (i);
            }
            copy.toBytes (content);
            description = L"criado com os horários de " + self->session_.file().getFileName();
        }
        else if (index >= 2)
        {
            auto& src = self->files_[(size_t) index - 2].info.file;
            juce::String err;
            if (! readFileShared (src, content, err))
            {
                inform (L"Não foi possível ler o arquivo de origem", err, true);
                return;
            }
            description = L"copiado de " + src.getFileName();
        }
        else
            description = "criado vazio";

        WriteRequest req;
        req.target = target;
        req.content = content;
        req.operation = "Criar arquivo";
        req.summary = name + " " + description;
        req.expectedBase = FileSnapshot {};
        auto r = self->ctx.workspace.writer().write (req);
        if (r.status != WriteStatus::written && r.status != WriteStatus::unchanged)
        {
            inform (L"Não foi possível criar o arquivo", r.message, true);
            return;
        }
        self->ctx.workspace.addActivity (name + " " + description, target);
        self->loadFiles();
        self->openFile (target, 0);
        self->ctx.status (name + " " + description + ".");
    }), true);
}

void ScheduleView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    auto left = getLocalBounds().removeFromLeft (280);
    g.setColour (colours::panel);
    g.fillRect (left);
    g.setColour (colours::border);
    g.drawVerticalLine (left.getRight() - 1, 0.0f, (float) getHeight());
    g.setColour (colours::textMuted);
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.08f));
    g.drawText ("ARQUIVOS", left.withHeight (36).withTrimmedLeft (14), juce::Justification::centredLeft, false);
}

void ScheduleView::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (280);
    auto newArea = left.removeFromTop (36).reduced (8, 5);
    newButton_.setBounds (newArea.removeFromRight (120));
    fileList_.setBounds (left.withTrimmedRight (1));

    auto inspectorArea = r.removeFromRight (360);
    inspector_->setBounds (inspectorArea);

    auto header = r.removeFromTop (64).reduced (14, 6);
    auto top = header.removeFromTop (32);
    saveButton_.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    discardButton_.setBounds (top.removeFromRight (96));
    top.removeFromRight (16);
    removeBlockButton_.setBounds (top.removeFromRight (112));
    top.removeFromRight (6);
    addBlockButton_.setBounds (top.removeFromRight (100));
    fileTitle_.setBounds (top);
    fileInfo_.setBounds (header);

    conflictBanner_.setBounds (r.removeFromTop (conflictBanner_.preferredHeight()));
    originBanner_.setBounds (r.removeFromTop (originBanner_.preferredHeight()));
    problems_.setBounds (r.removeFromBottom (diagnostics_.empty() ? 0 : juce::jmin (160, 30 + 26 * (int) diagnostics_.size())).reduced (12, 6));
    blocksViewport_.setBounds (r);
    blocks_->rebuild (blocksViewport_.getMaximumVisibleWidth());
}

} // namespace pc::ui
