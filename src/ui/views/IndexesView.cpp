#include "ui/views/IndexesView.h"

namespace pc::ui
{

using namespace theme;

namespace
{
int textHeight (const juce::String& text, const juce::Font& f, int width)
{
    juce::AttributedString s;
    s.append (text, f);
    s.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout layout;
    layout.createLayout (s, (float) width);
    return (int) std::ceil (layout.getHeight());
}

int drawParagraph (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Colour colour,
                   float size = 13.0f, bool bold = false)
{
    auto f = font (size, bold);
    auto h = textHeight (text, f, area.getWidth());
    juce::AttributedString s;
    s.append (text, f, colour);
    s.setWordWrap (juce::AttributedString::byWord);
    s.draw (g, area.withHeight (h).toFloat());
    return h;
}
} // namespace

IndexesView::IndexesView (AppContext& context) : View (context)
{
    addAndMakeVisible (statusCard_);
    addAndMakeVisible (rebuildCard_);
    statusCard_.addAndMakeVisible (table_);
    statusCard_.addAndMakeVisible (verifyButton_);
    table_.setColumns ({ { "Arquivo", 170 }, { "Tabela", 130 }, { "Chaves", 80 }, { "Registros", 90 }, { "Gravado em", 150 }, { L"Situação", 520 } });
    verifyButton_.onClick = [this] {
        inspect();
        ctx.status (L"Índices conferidos.");
    };

    rebuildCard_.addAndMakeVisible (textLayer_);
    for (auto* c : std::initializer_list<juce::Component*> { &startAtEnd_, &rebuildButton_, &openBackupButton_ })
        rebuildCard_.addAndMakeVisible (*c);
    startAtEnd_.setToggleState (true, juce::dontSendNotification);
    rebuildButton_.onClick = [this] { startRebuild(); };
    openBackupButton_.onClick = [this] {
        if (auto* r = ctx.workspace.indexRebuild(); r != nullptr && r->backupFolder().isDirectory())
            r->backupFolder().startAsProcess();
    };

    attach();
    inspect();
    updateCheck();
    startTimer (2000);
}

IndexesView::~IndexesView()
{
    if (auto* r = ctx.workspace.indexRebuild())
        r->onChange = nullptr;
}

void IndexesView::attach()
{
    if (auto* r = ctx.workspace.indexRebuild())
    {
        juce::Component::SafePointer<IndexesView> self (this);
        r->onChange = [self] {
            if (self == nullptr)
                return;
            auto* rebuild = self->ctx.workspace.indexRebuild();
            if (rebuild != nullptr && rebuild->finished())
            {
                self->ctx.workspace.addActivity (L"Recriação dos índices: " + rebuild->outcome(), self->ctx.workspace.pgm().getChildFile ("Indices"));
                self->ctx.status (rebuild->outcome());
                self->inspect();
            }
            self->updateCheck();
        };
    }
}

bool IndexesView::autoplayConfigured (bool& known) const
{
    known = false;
    auto& config = ctx.workspace.config();
    if (! config.has_value())
        return false;
    auto v = config->get ("bTocarAoIniciar");
    if (! v.has_value())
        return false;
    known = true;
    return v->trim() == "1" || v->trim() == "-1";
}

void IndexesView::inspect()
{
    std::vector<Table::Row> rows;
    problems_ = 0;
    if (ctx.workspace.isOpen())
    {
        for (auto& s : inspectIndexes (ctx.workspace.pgm()))
        {
            Table::Row row;
            auto& v = s.verification;
            row.cells = { s.file.getFileName(), s.table.isNotEmpty() ? "Dados\\" + s.table : juce::String ("-"),
                          s.exists && v.readable ? juce::String (v.indexEntries) : juce::String ("-"),
                          s.exists && v.readable ? juce::String (v.tableRecords) : juce::String ("-"),
                          s.exists ? s.modified.formatted ("%d/%m/%Y %H:%M:%S") : juce::String ("-"),
                          v.consistent ? juce::String (L"Íntegro: confere com a tabela") : v.summary };
            if (v.consistent)
            {
                row.status = Severity::info;
                row.statusOk = true;
            }
            else
            {
                row.status = s.exists && ! v.readable ? Severity::error : Severity::warning;
                if (s.table.isNotEmpty())
                    ++problems_;
            }
            row.tooltip = s.file.getFullPathName();
            rows.push_back (row);
        }
    }
    table_.setRows (std::move (rows));
    repaint();
}

void IndexesView::updateCheck()
{
    if (ctx.workspace.isOpen())
        check_ = checkIndexRebuild (ctx.workspace.pgm());
    else
        check_ = {};
    auto* r = ctx.workspace.indexRebuild();
    bool running = r != nullptr && r->running() && ! r->finished();
    rebuildButton_.setEnabled (ctx.workspace.isOpen() && ! ctx.workspace.readOnly() && check_.blockers.isEmpty() && ! running);
    rebuildButton_.setTooltip (ctx.workspace.readOnly() ? juce::String (L"Ative \"Permitir alterações\" no topo da janela.")
                                                        : juce::String());
    startAtEnd_.setEnabled (! running);
    openBackupButton_.setVisible (r != nullptr && r->finished() && r->backupFolder().isDirectory());

    textLayer_.draw = [this] (juce::Graphics& g) {
        auto* rebuild = ctx.workspace.indexRebuild();
        // Left: explanation and conditions.
        auto a = textArea_;
        a.removeFromTop (drawParagraph (g,
                                        L"O Playlist Digital cria os arquivos da pasta Indices quando abre. Para recriá-los: o Playlist é "
                                        L"fechado pela própria janela (como o operador faria), os índices são copiados e apagados, o "
                                        L"SeparaComprove é executado e o Playlist é aberto de novo, gravando índices novos.",
                                        a, colours::text)
                         + 10);
        bool known = false;
        auto autoplay = autoplayConfigured (known);
        bool running = ! check_.playlist.empty();
        auto warn = running ? L"O Playlist Digital está aberto e ficará fora do ar durante a operação (em geral alguns segundos, "
                              L"mais o tempo de abertura do programa). Faça isso fora do horário crítico."
                            : L"O Playlist Digital está fechado.";
        a.removeFromTop (drawParagraph (g, warn, a, running ? colours::warning : colours::textMuted, 13.0f, running) + 8);
        if (known)
            a.removeFromTop (drawParagraph (g,
                                            autoplay ? juce::String (L"\"Tocar programação ao iniciar\" está ativo: o Playlist volta a tocar sozinho ao abrir.")
                                                     : juce::String (L"\"Tocar programação ao iniciar\" está desativado: depois de reaberto, "
                                                                     L"o Playlist fica parado até alguém dar play."),
                                            a, autoplay ? colours::ok : colours::error, 13.0f, ! autoplay)
                             + 8);
        for (auto& b : check_.blockers)
        {
            drawStatusIcon (g, a.withWidth (16).withHeight (18).toFloat(), Severity::error, false);
            a.removeFromTop (drawParagraph (g, b, a.withTrimmedLeft (22), colours::error) + 4);
        }
        for (auto& w : check_.warnings)
        {
            drawStatusIcon (g, a.withWidth (16).withHeight (18).toFloat(), Severity::warning, false);
            a.removeFromTop (drawParagraph (g, w, a.withTrimmedLeft (22), colours::textMuted, 12.5f) + 4);
        }

        // Right: steps of the last or current run.
        auto s = stepsArea_;
        g.setColour (colours::panelAlt);
        g.fillRoundedRectangle (s.toFloat(), 8.0f);
        s = s.reduced (14, 12);
        g.setColour (colours::textMuted);
        g.setFont (font (11.5f, true).withExtraKerningFactor (0.08f));
        g.drawText (L"ETAPAS", s.removeFromTop (18), juce::Justification::centredLeft, false);
        s.removeFromTop (6);
        if (rebuild == nullptr)
        {
            drawParagraph (g, L"Nenhuma recriação feita nesta sessão.", s, colours::textMuted);
            return;
        }
        for (auto& step : rebuild->steps())
        {
            auto head = s.removeFromTop (22);
            auto icon = head.removeFromLeft (24).toFloat().withSizeKeepingCentre (16, 16);
            switch (step.state)
            {
                case IndexRebuild::StepState::done:
                    drawStatusIcon (g, icon, Severity::info, true);
                    break;
                case IndexRebuild::StepState::failed:
                    drawStatusIcon (g, icon, Severity::error, false);
                    break;
                case IndexRebuild::StepState::running:
                    g.setColour (colours::brandLight);
                    g.fillEllipse (icon.reduced (3.0f));
                    g.setColour (colours::brandLight.withAlpha (0.3f));
                    g.drawEllipse (icon.reduced (0.5f), 2.0f);
                    break;
                case IndexRebuild::StepState::pending:
                case IndexRebuild::StepState::skipped:
                    g.setColour (colours::border.darker (0.1f));
                    g.drawEllipse (icon.reduced (2.0f), 1.5f);
                    break;
            }
            bool dim = step.state == IndexRebuild::StepState::pending || step.state == IndexRebuild::StepState::skipped;
            g.setColour (dim ? colours::textMuted : colours::text);
            g.setFont (font (13.5f, step.state == IndexRebuild::StepState::running));
            g.drawText (step.title + (step.state == IndexRebuild::StepState::skipped ? juce::String (L" (não executada)") : juce::String()),
                        head, juce::Justification::centredLeft, true);
            if (step.detail.isNotEmpty())
                s.removeFromTop (drawParagraph (g, step.detail, s.withTrimmedLeft (24),
                                                step.state == IndexRebuild::StepState::failed ? colours::error : colours::textMuted, 12.0f)
                                 + 4);
            s.removeFromTop (2);
        }
        if (rebuild->finished())
        {
            s.removeFromTop (6);
            drawParagraph (g, rebuild->outcome(), s, rebuild->succeeded() ? colours::ok : colours::error, 13.5f, true);
        }
    };
    textLayer_.repaint();
    resized();
}

void IndexesView::startRebuild()
{
    if (ctx.workspace.readOnly())
    {
        inform (L"Modo somente leitura", L"Ative \"Permitir alterações\" no topo da janela.");
        return;
    }
    updateCheck();
    if (! check_.blockers.isEmpty())
    {
        inform (L"Não é possível recriar agora", check_.blockers.joinIntoString ("\n"), true);
        return;
    }
    bool known = false;
    auto autoplay = autoplayConfigured (known);
    juce::String message;
    if (! check_.playlist.empty())
        message << L"O Playlist Digital será FECHADO e a programação ficará fora do ar até ele abrir de novo.\n\n";
    message << L"Serão feitos, nesta ordem:\n"
               L"1. copiar pgm\\Indices e as tabelas COMPROVE/LIGACAO para a cópia de segurança;\n"
               L"2. apagar os arquivos .NTX da pasta Indices;\n"
               L"3. executar o SeparaComprove;\n"
               L"4. abrir o Playlist Digital, que grava os índices novos.\n\n";
    if (known && ! autoplay)
        message << L"Atenção: \"Tocar programação ao iniciar\" está desativado. Depois de reaberto, será preciso dar play no Playlist.\n\n";
    message << L"Se o Playlist ou o SeparaComprove perguntarem algo, responda na janela deles.";
    juce::Component::SafePointer<IndexesView> self (this);
    confirm (L"Recriar índices do Playlist Digital", message, L"Recriar índices", [self] {
        if (self == nullptr)
            return;
        IndexRebuild::Options options;
        options.startPlaylistAtEnd = self->startAtEnd_.getToggleState();
        if (self->ctx.workspace.startIndexRebuild (options) == nullptr)
        {
            inform (L"Não foi possível iniciar", L"Outra recriação está em andamento ou a instalação está em modo somente leitura.", true);
            return;
        }
        self->attach();
        self->updateCheck();
    });
}

void IndexesView::timerCallback()
{
    auto* r = ctx.workspace.indexRebuild();
    if (r == nullptr || r->finished())
        updateCheck();
}

void IndexesView::refresh()
{
    inspect();
    updateCheck();
}

void IndexesView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void IndexesView::resized()
{
    auto r = getLocalBounds().reduced (16, 14);
    auto statusHeight = juce::jmin (r.getHeight() / 2 - 6, 40 + 30 + 28 * juce::jmax (4, table_.numVisibleRows()) + 20);
    statusCard_.setBounds (r.removeFromTop (statusHeight));
    auto sc = statusCard_.content();
    verifyButton_.setBounds (statusCard_.getWidth() - 14 - verifyButton_.preferredWidth (32), 8, verifyButton_.preferredWidth (32), 32);
    table_.setBounds (sc.withTrimmedTop (6));
    r.removeFromTop (12);
    rebuildCard_.setBounds (r);

    auto rc = rebuildCard_.content();
    textLayer_.setBounds (rebuildCard_.getLocalBounds());
    auto buttons = rc.removeFromBottom (36);
    rebuildButton_.setBounds (buttons.removeFromLeft (rebuildButton_.preferredWidth (36)));
    buttons.removeFromLeft (12);
    if (openBackupButton_.isVisible())
    {
        openBackupButton_.setBounds (buttons.removeFromRight (openBackupButton_.preferredWidth (36)));
        buttons.removeFromRight (12);
    }
    startAtEnd_.setBounds (buttons.removeFromLeft (juce::jmin (buttons.getWidth(), 520)));
    rc.removeFromBottom (12);
    auto half = rc.getWidth() * 11 / 20;
    textArea_ = rc.removeFromLeft (half).withTrimmedRight (20).withTrimmedTop (4);
    stepsArea_ = rc;
}

} // namespace pc::ui
