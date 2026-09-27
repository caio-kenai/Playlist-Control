#include "ui/views/DashboardView.h"
#include "storage/FileIO.h"

namespace pc::ui
{

using namespace theme;

namespace
{
juce::String plural (int n, const juce::String& one, const juce::String& many)
{
    return juce::String (n) + " " + (n == 1 ? one : many);
}

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

void drawKeyValue (juce::Graphics& g, juce::Rectangle<int>& area, const juce::String& key, const juce::String& value,
                   juce::Colour valueColour = colours::text)
{
    auto row = area.removeFromTop (22);
    g.setFont (font (13.0f));
    g.setColour (colours::textMuted);
    g.drawText (key, row.removeFromLeft (130), juce::Justification::centredLeft, true);
    g.setColour (valueColour);
    g.setFont (font (13.5f, valueColour != colours::text));
    g.drawText (value, row, juce::Justification::centredLeft, true);
}
} // namespace

DashboardView::DashboardView (AppContext& context) : View (context)
{
    addAndMakeVisible (diagnosticsButton_);
    diagnosticsButton_.onClick = [this] { ctx.navigate (ViewId::diagnostics, {}, 0); };
    setMouseCursor (juce::MouseCursor::NormalCursor);
    refresh();
}

juce::String DashboardView::subtitle() const
{
    auto today = Date::today();
    static const char* const days[] = { "domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado" };
    return juce::String::fromUTF8 (days[today.dayOfWeek()]) + ", " + today.toString();
}

void DashboardView::refresh()
{
    auto& ws = ctx.workspace;
    today_.clear();
    week_.clear();
    diagnostics_ = {};
    if (! ws.isOpen())
    {
        repaint();
        return;
    }

    auto today = Date::today();
    for (auto kind : { ScheduleKind::commercial, ScheduleKind::musical, ScheduleKind::commercialClock, ScheduleKind::musicalClock })
    {
        auto src = ws.source (kind);
        bool clock = kind == ScheduleKind::commercialClock || kind == ScheduleKind::musicalClock;
        if (clock && ! src.sectionPresent)
            continue;
        TodayRow row;
        row.kind = kind;
        row.label = toDisplayString (kind);
        if (auto c = ws.activeFile (kind, today))
        {
            row.file = c->file;
            row.rule = c->rule;
            juce::MemoryBlock m;
            juce::String err;
            if (readFileShared (c->file, m, err))
            {
                auto doc = ScheduleDocument::fromBytes (m);
                row.blocks = (int) doc.blockIndexes().size();
                auto o = ws.originOf (c->file, doc, kind);
                row.origin = o.origin;
                row.rewritten = o.rewrittenAutomatically;
                ScheduleFileInfo info { c->file, kind, today, clock };
                auto d = validateSchedule (doc, c->file, kind, ws.validationContext (info));
                row.errors = d.count (Severity::error);
                row.warnings = d.count (Severity::warning);
            }
        }
        today_.push_back (row);
    }

    for (int i = 0; i < 7; ++i)
    {
        DayColumn col;
        col.date = today.addDays (i);
        col.map = ws.activeFile (ScheduleKind::commercial, col.date).has_value();
        col.grade = ws.activeFile (ScheduleKind::musical, col.date).has_value();
        week_.push_back (col);
    }

    diagnostics_ = ws.runFullDiagnostics();
    for (auto& d : diagnostics_.items())
    {
        if (d.severity != Severity::error)
            continue;
        if (auto date = dateFromFileName (d.file.getFileName()))
            for (auto& col : week_)
                if (col.date == *date)
                    ++col.errors;
    }
    repaint();
}

void DashboardView::paintCard (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent)
{
    auto f = r.toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (f, 6.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (f, 6.0f, 1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) r.getX() + 14, (float) r.getY() + 14, 4.0f, 16.0f), 2.0f);
    g.setColour (colours::text);
    g.setFont (font (15.0f, true));
    g.drawText (title, r.getX() + 24, r.getY() + 10, r.getWidth() - 36, 24, juce::Justification::centredLeft, true);
}

void DashboardView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    if (! ctx.workspace.isOpen())
    {
        g.setColour (colours::textMuted);
        g.setFont (font (16.0f));
        g.drawFittedText (L"Nenhuma instalação carregada. Use \"Instalação\" no topo para localizar a pasta pgm do Playlist Digital.",
                          getLocalBounds().reduced (40), juce::Justification::centred, 3);
        return;
    }
    paintInstallation (g, installationArea_);
    paintPrograms (g, programsArea_);
    paintSummary (g, summaryArea_);
    paintToday (g, todayArea_);
    paintWeek (g, weekArea_);
    paintAlerts (g, alertsArea_);
    paintActivity (g, activityArea_);
}

void DashboardView::paintInstallation (juce::Graphics& g, juce::Rectangle<int> r)
{
    auto& ws = ctx.workspace;
    paintCard (g, r, L"Instalação", colours::brandLight);
    auto a = r.reduced (16).withTrimmedTop (30);
    drawKeyValue (g, a, "Pasta", ws.pgm().getFullPathName());
    drawKeyValue (g, a, "Playlist Digital", ws.installation().playlistVersion.isNotEmpty() ? L"versão " + ws.installation().playlistVersion : "-");
    auto station = ws.config().has_value() ? ws.config()->get ("sNomeDaEmissora").value_or ("") : juce::String();
    drawKeyValue (g, a, "Emissora", station.isNotEmpty() ? station : "-");
    drawKeyValue (g, a, "Modo", ws.readOnly() ? L"Somente leitura" : L"Alterações permitidas",
                  ws.readOnly() ? colours::warning : colours::ok);
    int folders = ws.folders().has_value() ? (int) ws.folders()->folders().size() : 0;
    int regs = 0;
    for (auto& reg : ws.catalog().registrations())
        regs += reg.deleted ? 0 : 1;
    drawKeyValue (g, a, L"Pastas / códigos", juce::String (folders) + " pastas, " + juce::String (regs) + " registros");
}

void DashboardView::paintPrograms (juce::Graphics& g, juce::Rectangle<int> r)
{
    auto& ws = ctx.workspace;
    auto& eco = ws.ecosystem();
    auto& rt = ws.runtime();
    paintCard (g, r, "Programas", colours::brandLight);
    auto a = r.reduced (16).withTrimmedTop (30);

    auto state = [&] (bool running) { return running ? L"Em execução" : juce::String ("Fechado"); };
    drawKeyValue (g, a, "Playlist Digital", state (rt.playlistRunning), rt.playlistRunning ? colours::ok : colours::textMuted);
    drawKeyValue (g, a, "Config Manager", state (rt.configManagerRunning), rt.configManagerRunning ? colours::warning : colours::textMuted);
    if (eco.sync.found)
    {
        auto s = toDisplayString (eco.sync.service);
        if (eco.sync.lastSync.isNotEmpty())
            s << L" · última sincronização " << eco.sync.lastSync;
        drawKeyValue (g, a, "Sync Service (Planner)", s, eco.sync.service == ServiceState::running ? colours::ok : colours::warning);
    }
    else
        drawKeyValue (g, a, "Sync Service (Planner)", L"Não instalado", colours::textMuted);
    if (eco.playlistServer.found)
        drawKeyValue (g, a, "Playlist Server (Maker)", toDisplayString (eco.playlistServer.service),
                      eco.playlistServer.service == ServiceState::running ? colours::ok : colours::warning);
    else
        drawKeyValue (g, a, "Playlist Server (Maker)", L"Não instalado", colours::textMuted);
    drawKeyValue (g, a, "Commercial", eco.commercial.found ? (L"versão " + eco.commercial.version
                                                             + (samePath (eco.commercial.mapsFolder, ws.pgm().getChildFile ("Mapas")) ? juce::String (L" · exporta para esta instalação") : juce::String()))
                                                           : juce::String (L"Não instalado"),
                  eco.commercial.found ? colours::text : colours::textMuted);
}

void DashboardView::paintSummary (juce::Graphics& g, juce::Rectangle<int> r)
{
    paintCard (g, r, L"Situação", colours::brandLight);
    auto a = r.reduced (16).withTrimmedTop (32);
    auto errors = diagnostics_.count (Severity::error);
    auto warnings = diagnostics_.count (Severity::warning);
    auto box = a.removeFromTop (70);
    auto half = box.getWidth() / 2;
    auto drawCount = [&] (juce::Rectangle<int> b, int n, const juce::String& label, Severity s) {
        g.setColour (n > 0 ? severityBackground (s) : colours::okBack);
        g.fillRoundedRectangle (b.reduced (4).toFloat(), 6.0f);
        g.setColour (n > 0 ? severityColour (s) : colours::ok);
        g.setFont (font (28.0f, true));
        g.drawText (juce::String (n), b.withTrimmedBottom (20), juce::Justification::centred, false);
        g.setFont (font (12.5f));
        g.drawText (label, b.withTrimmedTop (44), juce::Justification::centred, false);
    };
    drawCount (box.removeFromLeft (half), errors, errors == 1 ? "erro" : "erros", Severity::error);
    drawCount (box, warnings, warnings == 1 ? "aviso" : "avisos", Severity::warning);
    a.removeFromTop (8);
    g.setColour (colours::textMuted);
    g.setFont (font (12.5f));
    g.drawFittedText (L"Configuração, pastas, merges e mapas/grades de hoje em diante.", a.removeFromTop (34),
                      juce::Justification::topLeft, 2);
}

void DashboardView::paintToday (juce::Graphics& g, juce::Rectangle<int> r)
{
    paintCard (g, r, L"Programação de hoje", colours::brandLight);
    auto a = r.reduced (16).withTrimmedTop (32);
    if (today_.empty())
    {
        g.setColour (colours::textMuted);
        g.setFont (font (13.5f));
        g.drawText (L"PLAYLIST.ini não encontrado.", a, juce::Justification::topLeft, true);
        return;
    }
    for (int i = 0; i < (int) today_.size(); ++i)
    {
        auto& row = today_[(size_t) i];
        auto line = a.removeFromTop (46);
        row.area = line;
        bool commercial = row.kind == ScheduleKind::commercial || row.kind == ScheduleKind::commercialClock;
        auto band = commercial ? colours::commercial : colours::musical;
        g.setColour (i == hoverRow_ ? colours::panelAlt : colours::panel);
        g.fillRect (line);
        g.setColour (band);
        g.fillRect (line.removeFromLeft (5).reduced (0, 4));
        line.removeFromLeft (10);

        auto text = line.removeFromLeft (line.getWidth() - 190);
        g.setColour (colours::text);
        g.setFont (font (14.0f, true));
        g.drawText (row.label, text.removeFromTop (22), juce::Justification::bottomLeft, true);
        g.setFont (font (12.5f));
        g.setColour (row.file.existsAsFile() ? colours::textMuted : colours::error);
        g.drawText (row.file.existsAsFile() ? row.file.getFileName() + "  (" + juce::String (row.blocks) + " blocos)"
                                            : L"Nenhum arquivo para hoje — o Playlist não terá programação automática",
                    text, juce::Justification::topLeft, true);

        auto badges = line.toFloat().withSizeKeepingCentre ((float) line.getWidth(), 20.0f);
        if (row.file.existsAsFile())
        {
            auto origin = toDisplayString (row.origin);
            auto w = badgeWidth (origin);
            drawBadge (g, badges.removeFromLeft (w), origin, originColour (row.origin), juce::Colours::white);
            badges.removeFromLeft (6);
            auto problems = row.errors > 0 ? plural (row.errors, "erro", "erros")
                          : row.warnings > 0 ? plural (row.warnings, "aviso", "avisos") : juce::String ("OK");
            auto s = row.errors > 0 ? Severity::error : Severity::warning;
            drawBadge (g, badges.removeFromLeft (badgeWidth (problems)), problems,
                       row.errors + row.warnings > 0 ? severityBackground (s) : colours::okBack,
                       row.errors + row.warnings > 0 ? severityColour (s) : colours::ok);
        }
        g.setColour (colours::border);
        g.drawHorizontalLine (row.area.getBottom() - 1, (float) row.area.getX(), (float) row.area.getRight());
    }
}

void DashboardView::paintWeek (juce::Graphics& g, juce::Rectangle<int> r)
{
    paintCard (g, r, L"Próximos 7 dias", colours::brandLight);
    auto a = r.reduced (16).withTrimmedTop (34);
    if (week_.empty())
        return;
    auto labels = a.removeFromLeft (64);
    g.setFont (font (12.5f));
    g.setColour (colours::textMuted);
    g.drawText ("Mapa", labels.withTrimmedTop (30).removeFromTop (28), juce::Justification::centredLeft, false);
    g.drawText ("Grade", labels.withTrimmedTop (62).removeFromTop (28), juce::Justification::centredLeft, false);
    auto w = a.getWidth() / (int) week_.size();
    static const char* const days[] = { "Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb" };
    for (auto& col : week_)
    {
        auto c = a.removeFromLeft (w);
        g.setColour (colours::text);
        g.setFont (font (12.5f, true));
        g.drawText (juce::String::fromUTF8 (days[col.date.dayOfWeek()]) + " " + juce::String (col.date.day).paddedLeft ('0', 2),
                    c.removeFromTop (26), juce::Justification::centred, false);
        auto cell = [&] (bool present, juce::Colour band) {
            auto b = c.removeFromTop (32).reduced (4, 3).toFloat();
            g.setColour (present ? band.withAlpha (0.12f) : colours::errorBack);
            g.fillRoundedRectangle (b, 4.0f);
            drawStatusIcon (g, b.withSizeKeepingCentre (14, 14), Severity::error, present);
        };
        cell (col.map, colours::commercial);
        cell (col.grade, colours::musical);
        if (col.errors > 0)
            drawBadge (g, c.removeFromTop (22).toFloat().withSizeKeepingCentre (badgeWidth (juce::String (col.errors)), 18),
                       juce::String (col.errors), colours::errorBack, colours::error);
    }
}

void DashboardView::paintAlerts (juce::Graphics& g, juce::Rectangle<int> r)
{
    paintCard (g, r, "Alertas", colours::error);
    alertAreas_.clear();
    auto a = r.reduced (16).withTrimmedTop (32);
    std::vector<Diagnostic> top;
    for (auto s : { Severity::error, Severity::warning })
        for (auto& d : diagnostics_.items())
            if (d.severity == s && top.size() < 50)
                top.push_back (d);
    if (top.empty())
    {
        g.setColour (colours::ok);
        g.setFont (font (13.5f));
        g.drawText (L"Nenhum problema encontrado.", a, juce::Justification::topLeft, true);
        return;
    }
    for (auto& d : top)
    {
        if (a.getHeight() < 40)
            break;
        auto line = a.removeFromTop (40);
        alertAreas_.push_back ({ line, d });
        drawStatusIcon (g, line.removeFromLeft (22).toFloat().withSizeKeepingCentre (14, 14), d.severity, false);
        g.setColour (colours::text);
        g.setFont (font (13.0f));
        g.drawText (d.message, line.removeFromTop (20), juce::Justification::bottomLeft, true);
        g.setColour (colours::textMuted);
        g.setFont (font (12.0f));
        g.drawText (d.locationText(), line, juce::Justification::topLeft, true);
    }
}

void DashboardView::paintActivity (juce::Graphics& g, juce::Rectangle<int> r)
{
    paintCard (g, r, "Atividade recente", colours::brandLight);
    auto a = r.reduced (16).withTrimmedTop (32);
    auto& items = ctx.workspace.activity();
    auto history = ctx.workspace.history().list (8);
    if (items.empty() && history.empty())
    {
        g.setColour (colours::textMuted);
        g.setFont (font (13.5f));
        g.drawText (L"Sem atividade nesta sessão.", a, juce::Justification::topLeft, true);
        return;
    }
    for (auto& e : items)
    {
        if (a.getHeight() < 22)
            break;
        auto line = a.removeFromTop (22);
        g.setColour (colours::textMuted);
        g.setFont (font (12.5f));
        g.drawText (e.time.formatted ("%H:%M:%S"), line.removeFromLeft (64), juce::Justification::centredLeft, false);
        g.setColour (e.external ? colours::warning : colours::text);
        g.setFont (font (13.0f));
        g.drawText (e.text, line, juce::Justification::centredLeft, true);
    }
    for (auto& h : history)
    {
        if (a.getHeight() < 22)
            break;
        auto line = a.removeFromTop (22);
        g.setColour (colours::textMuted);
        g.setFont (font (12.5f));
        g.drawText (h.time.formatted ("%d/%m %H:%M"), line.removeFromLeft (64), juce::Justification::centredLeft, false);
        g.setColour (colours::text);
        g.setFont (font (13.0f));
        g.drawText (h.operation + ": " + h.target.getFileName(), line, juce::Justification::centredLeft, true);
    }
}

void DashboardView::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto top = r.removeFromTop (190);
    auto third = (top.getWidth() - 24) / 3;
    installationArea_ = top.removeFromLeft (third + 40);
    top.removeFromLeft (12);
    summaryArea_ = top.removeFromRight (third - 80);
    top.removeFromRight (12);
    programsArea_ = top;
    r.removeFromTop (12);

    auto middle = r.removeFromTop (juce::jmax (220, 46 * (int) juce::jmax<size_t> (2, today_.size()) + 60));
    todayArea_ = middle.removeFromLeft (middle.getWidth() / 2 - 6);
    middle.removeFromLeft (12);
    weekArea_ = middle;
    r.removeFromTop (12);

    alertsArea_ = r.removeFromLeft (r.getWidth() / 2 - 6);
    r.removeFromLeft (12);
    activityArea_ = r;

    diagnosticsButton_.setBounds (summaryArea_.getX() + 16, summaryArea_.getBottom() - 40, summaryArea_.getWidth() - 32, 28);
}

void DashboardView::mouseMove (const juce::MouseEvent& e)
{
    int hover = -1;
    for (int i = 0; i < (int) today_.size(); ++i)
        if (today_[(size_t) i].area.contains (e.getPosition()) && today_[(size_t) i].file.existsAsFile())
            hover = i;
    bool overAlert = false;
    for (auto& [area, d] : alertAreas_)
        overAlert = overAlert || area.contains (e.getPosition());
    setMouseCursor (hover >= 0 || overAlert ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (hover != hoverRow_)
    {
        hoverRow_ = hover;
        repaint (todayArea_);
    }
}

void DashboardView::mouseUp (const juce::MouseEvent& e)
{
    for (auto& row : today_)
    {
        if (! row.area.contains (e.getPosition()) || ! row.file.existsAsFile())
            continue;
        auto view = row.kind == ScheduleKind::commercial ? ViewId::maps
                  : row.kind == ScheduleKind::musical    ? ViewId::grades : ViewId::clocks;
        ctx.navigate (view, row.file, 0);
        return;
    }
    for (auto& [area, d] : alertAreas_)
    {
        if (! area.contains (e.getPosition()))
            continue;
        ctx.navigate (ViewId::diagnostics, d.file, d.line);
        return;
    }
}

} // namespace pc::ui
