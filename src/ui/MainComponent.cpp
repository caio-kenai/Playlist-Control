#include "ui/MainComponent.h"
#include "core/TextCase.h"
#include "logging/Logger.h"
#include "ui/views/ViewFactory.h"

#include "Version.h"
#include <BinaryData.h>

namespace pc::ui
{

using namespace theme;

namespace
{
constexpr int headerHeight = 64;
constexpr int sidebarWidth = 236;
constexpr int headerButtonHeight = 36;
constexpr int chipWidth = 148;
} // namespace

MainComponent::MainComponent (Workspace& workspace, AppSettings& settings)
    : workspace_ (workspace),
      settings_ (settings),
      context_ { workspace, settings, [this] (const juce::String& s) { setStatus (s); },
                 [this] (ViewId id, const juce::File& f, int line) { showView (id, f, line); } }
{
    nav_ = {
        { ViewId::dashboard, "Painel", "", Icon::dashboard },
        { ViewId::maps, "Mapas comerciais", L"Programação", Icon::maps },
        { ViewId::grades, "Grades musicais", L"Programação", Icon::grades },
        { ViewId::clocks, L"Relógios", L"Programação", Icon::clock },
        { ViewId::playlistIni, "Leitura de mapas", L"Configuração", Icon::fileSettings },
        { ViewId::config, L"Opções do Playlist", L"Configuração", Icon::sliders },
        { ViewId::folders, L"Pastas e códigos", L"Configuração", Icon::folder },
        { ViewId::operators, "Operadores", L"Configuração", Icon::users },
        { ViewId::diagnostics, L"Diagnóstico", "Suporte", Icon::stethoscope },
        { ViewId::indexes, L"Índices", "Suporte", Icon::database },
        { ViewId::history, L"Histórico", "Suporte", Icon::history },
    };

    addAndMakeVisible (banner_);
    addChildComponent (toast_);
    for (auto* b : { &modeButton_, &installButton_, &reloadButton_ })
    {
        addAndMakeVisible (*b);
        b->setStyle (ActionButton::Style::header);
    }
    installButton_.setIcon (Icon::folderOpen);
    installButton_.setTooltip (L"Instalação: escolher a pasta pgm do Playlist Digital");
    installButton_.onClick = [this] { chooseInstallation(); };
    reloadButton_.setIcon (Icon::refresh);
    reloadButton_.setTooltip (L"Lê novamente todos os arquivos da instalação (F5)");
    reloadButton_.onClick = [this] {
        guardUnsaved ([this] {
            workspace_.reload();
            setStatus (L"Instalação recarregada.");
        });
    };
    modeButton_.onClick = [this] {
        if (workspace_.readOnly())
        {
            confirm (L"Permitir alterações",
                     L"O Playlist Control passará a gravar nos arquivos do Playlist Digital quando você salvar.\n\n"
                     L"Toda gravação é validada e guarda uma cópia de segurança no Histórico.",
                     L"Permitir alterações", [this] {
                         workspace_.setReadOnly (false);
                         settings_.setReadOnly (false);
                     });
        }
        else
        {
            workspace_.setReadOnly (true);
            settings_.setReadOnly (true);
        }
    };

    workspace_.addChangeListener (this);
    setWantsKeyboardFocus (true);
    showView (ViewId::dashboard);
    updateBanner();
    startTimer (3000);
}

MainComponent::~MainComponent()
{
    workspace_.removeChangeListener (this);
}

void MainComponent::setStatus (const juce::String& text)
{
    if (text.isNotEmpty())
        toast_.show (text);
}

void MainComponent::updateBanner()
{
    if (! workspace_.isOpen())
        banner_.show (Severity::warning, L"Nenhuma instalação do Playlist Digital carregada.", L"Localizar", [this] { chooseInstallation(); });
    else if (workspace_.readOnly())
        banner_.show (Severity::info, L"Modo somente leitura: nada será gravado nos arquivos do Playlist.",
                      L"Permitir alterações", [this] { modeButton_.triggerClick(); });
    else
        banner_.hideBanner();
    modeButton_.setButtonText (workspace_.readOnly() ? L"Somente leitura" : L"Edição liberada");
    modeButton_.setIcon (workspace_.readOnly() ? Icon::lock : Icon::unlock);
    modeButton_.setStyle (workspace_.readOnly() ? ActionButton::Style::header : ActionButton::Style::headerAccent);
    modeButton_.setTooltip (workspace_.readOnly() ? L"Clique para permitir que o Playlist Control grave nos arquivos"
                                                  : L"Clique para voltar ao modo somente leitura");
    resized();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateBanner();
    if (view_ != nullptr)
        view_->refresh();
    repaint();
}

void MainComponent::timerCallback()
{
    workspace_.refreshRuntime();
    repaint (0, 0, getWidth(), headerHeight);
}

std::unique_ptr<View> MainComponent::createView (ViewId id)
{
    return createNamedView (id, context_);
}

void MainComponent::guardUnsaved (std::function<void()> proceed)
{
    if (view_ == nullptr || ! view_->hasUnsavedChanges())
    {
        proceed();
        return;
    }
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle (L"Alterações não salvas")
                       .withMessage (L"Há alterações não salvas em \"" + view_->title() + L"\". O que deseja fazer?")
                       .withButton ("Salvar")
                       .withButton ("Descartar")
                       .withButton ("Cancelar");
    juce::Component::SafePointer<MainComponent> self (this);
    juce::AlertWindow::showAsync (options, [self, proceed] (int result) {
        if (self == nullptr || self->view_ == nullptr)
            return;
        if (result == 1)
        {
            self->view_->save();
            if (! self->view_->hasUnsavedChanges())
                proceed();
        }
        else if (result == 2)
        {
            self->view_->discard();
            proceed();
        }
    });
}

void MainComponent::showView (ViewId id, const juce::File& file, int line)
{
    auto open = [this, id, file, line] {
        if (view_ == nullptr || current_ != id)
        {
            removeChildComponent (view_.get());
            view_ = createView (id);
            current_ = id;
            addAndMakeVisible (*view_);
            resized();
        }
        if (file != juce::File())
            view_->openFile (file, line);
        view_->grabKeyboardFocus();
        repaint();
    };
    if (view_ != nullptr && current_ != id)
        guardUnsaved (open);
    else
        open();
}

void MainComponent::chooseInstallation()
{
    guardUnsaved ([this] {
        chooser_ = std::make_unique<juce::FileChooser> (L"Selecione a pasta pgm do Playlist Digital",
                                                        workspace_.isOpen() ? workspace_.pgm() : juce::File ("C:\\Playlist"));
        chooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                               [this] (const juce::FileChooser& fc) {
                                   auto folder = fc.getResult();
                                   if (folder == juce::File())
                                       return;
                                   auto info = inspectInstallation (folder);
                                   if (! info.valid)
                                   {
                                       inform (L"Pasta não reconhecida", info.summary(), true);
                                       return;
                                   }
                                   confirm (L"Usar esta instalação?",
                                            info.summary() + L"\n\nO Playlist Control passará a ler e controlar os arquivos desta pasta.",
                                            L"Usar esta instalação", [this, folder] {
                                                settings_.setPgmFolder (folder);
                                                settings_.setInstallationConfirmed (true);
                                                workspace_.open (folder);
                                                showView (ViewId::dashboard);
                                            });
                               });
    });
}

int MainComponent::problemCount (ViewId id) const
{
    if (! workspace_.isOpen())
        return 0;
    switch (id)
    {
        case ViewId::diagnostics: return workspace_.loadProblems().count (Severity::error);
        default:                  return 0;
    }
}

juce::Rectangle<float> MainComponent::statusChipArea() const
{
    auto b = modeButton_.getBounds().toFloat();
    return { b.getX() - 12.0f - (float) chipWidth, b.getY(), (float) chipWidth, b.getHeight() };
}

void MainComponent::paintHeader (juce::Graphics& g, juce::Rectangle<int> r)
{
    juce::ColourGradient grad (colours::brand.darker (0.18f), 0.0f, 0.0f, colours::brand.brighter (0.18f), (float) r.getWidth(), 0.0f, false);
    g.setGradientFill (grad);
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (r.removeFromBottom (1));

    // Project symbol and wordmark.
    auto logo = r.removeFromLeft (sidebarWidth).reduced (16, 0);
    static const auto symbol = juce::ImageCache::getFromMemory (PlaylistControlAssets::symbol_png, PlaylistControlAssets::symbol_pngSize);
    static const auto wordmark = juce::ImageCache::getFromMemory (PlaylistControlAssets::wordmark_png, PlaylistControlAssets::wordmark_pngSize);
    g.setOpacity (1.0f);
    g.drawImageWithin (symbol, logo.getX(), (headerHeight - 40) / 2, 40, 40, juce::RectanglePlacement::centred);
    auto word = logo.withTrimmedLeft (50).withSizeKeepingCentre (logo.getWidth() - 50, 26);
    g.drawImageWithin (wordmark, word.getX(), word.getY(), word.getWidth(), word.getHeight(),
                       juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);

    // Title of the current view.
    auto chip = statusChipArea();
    auto info = r.withRight ((int) chip.getX() - 16).withTrimmedLeft (22);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.fillRect (juce::Rectangle<float> ((float) r.getX(), 16.0f, 1.0f, (float) headerHeight - 32.0f));
    g.setColour (juce::Colours::white);
    g.setFont (font (17.0f, true));
    g.drawText (view_ != nullptr ? view_->title() : juce::String(), info.withHeight (headerHeight / 2 + 3),
                juce::Justification::bottomLeft, true);
    g.setColour (juce::Colour (0xffb4c7e6));
    g.setFont (font (12.5f));
    g.drawText (view_ != nullptr ? view_->subtitle() : juce::String(), info.withTrimmedTop (headerHeight / 2 + 5),
                juce::Justification::topLeft, true);

    // Playlist Digital running state: a status chip, not a button.
    bool running = workspace_.isOpen() && workspace_.runtime().playlistRunning;
    auto dotColour = running ? juce::Colour (0xffff3b30) : juce::Colour (0xff8ea6c9);
    g.setColour (running ? juce::Colour (0xffff3b30).withAlpha (0.16f) : juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (chip, chip.getHeight() / 2.0f);
    g.setColour (running ? juce::Colour (0xffff6b61).withAlpha (0.7f) : juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (chip.reduced (0.5f), chip.getHeight() / 2.0f, 1.0f);
    auto dot = juce::Rectangle<float> (chip.getX() + 16.0f, chip.getCentreY() - 4.5f, 9.0f, 9.0f);
    if (running)
    {
        g.setColour (dotColour.withAlpha (0.3f));
        g.fillEllipse (dot.expanded (3.5f));
    }
    g.setColour (dotColour);
    g.fillEllipse (dot);
    g.setColour (running ? juce::Colour (0xffffd9d6) : juce::Colour (0xffc3d3ea));
    g.setFont (font (13.0f, true));
    g.drawText (running ? "Playlist no ar" : "Playlist fechado", chip.withTrimmedLeft (34.0f).withTrimmedRight (12.0f),
                juce::Justification::centredLeft, false);
}

void MainComponent::paintSidebar (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (colours::panel);
    g.fillRect (r);
    g.setColour (colours::border.withAlpha (0.8f));
    g.drawVerticalLine (r.getRight() - 1, (float) r.getY(), (float) r.getBottom());

    auto a = r.reduced (12, 12);
    juce::String group;
    for (int i = 0; i < (int) nav_.size(); ++i)
    {
        auto& item = nav_[(size_t) i];
        if (item.group != group)
        {
            group = item.group;
            auto h = a.removeFromTop (34);
            g.setColour (colours::textMuted.withAlpha (0.85f));
            g.setFont (font (11.0f, true).withExtraKerningFactor (0.1f));
            g.drawText (toUpperLatin (group), h.withTrimmedLeft (12).withTrimmedTop (12), juce::Justification::centredLeft, false);
        }
        item.area = a.removeFromTop (38);
        auto pill = item.area.toFloat().reduced (0.0f, 2.0f);
        bool selected = item.id == current_;
        bool hover = i == hoverNav_;
        if (selected)
        {
            g.setColour (colours::infoBack);
            g.fillRoundedRectangle (pill, 8.0f);
            g.setColour (colours::brandLight);
            g.fillRoundedRectangle (juce::Rectangle<float> (pill.getX(), pill.getY() + 8.0f, 3.0f, pill.getHeight() - 16.0f), 1.5f);
        }
        else if (hover)
        {
            g.setColour (colours::panelAlt);
            g.fillRoundedRectangle (pill, 8.0f);
        }
        drawIcon (g, item.icon, juce::Rectangle<float> (pill.getX() + 14.0f, pill.getCentreY() - 9.0f, 18.0f, 18.0f),
                  selected ? colours::brandLight : colours::textMuted, selected ? 1.9f : 1.7f);
        g.setColour (selected ? colours::brandLight : colours::text);
        g.setFont (font (14.0f, selected));
        g.drawText (item.label, item.area.withTrimmedLeft (44).withTrimmedRight (40), juce::Justification::centredLeft, true);
        auto n = problemCount (item.id);
        if (n > 0)
            drawBadge (g, pill.removeFromRight (40.0f).withSizeKeepingCentre (28.0f, 18.0f), juce::String (n),
                       colours::errorBack, colours::error);
    }

    g.setColour (colours::textMuted.withAlpha (0.8f));
    g.setFont (font (11.5f));
    g.drawText (juce::String (L"Versão ") + PLAYLISTCONTROL_VERSION_STRING, r.removeFromBottom (34).withTrimmedLeft (24),
                juce::Justification::centredLeft, false);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    auto r = getLocalBounds();
    paintHeader (g, r.removeFromTop (headerHeight));
    paintSidebar (g, r.removeFromLeft (sidebarWidth));
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (14, 0).withSizeKeepingCentre (getWidth() - 28, headerButtonHeight);
    reloadButton_.setBounds (header.removeFromRight (headerButtonHeight));
    header.removeFromRight (8);
    installButton_.setBounds (header.removeFromRight (headerButtonHeight));
    header.removeFromRight (12);
    modeButton_.setBounds (header.removeFromRight (juce::jmax (170, modeButton_.preferredWidth (headerButtonHeight))));

    r.removeFromLeft (sidebarWidth);
    banner_.setBounds (r.removeFromTop (banner_.preferredHeight()));
    if (view_ != nullptr)
        view_->setBounds (r);
    if (toast_.isVisible())
    {
        auto w = toast_.preferredWidth();
        toast_.setBounds (r.getCentreX() - w / 2, r.getBottom() - 64, w, 44);
        toast_.toFront (false);
    }
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('s', juce::ModifierKeys::ctrlModifier, 0) && view_ != nullptr)
    {
        view_->save();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::F5Key)
    {
        reloadButton_.triggerClick();
        return true;
    }
    if (key.getModifiers().isCtrlDown() && key.getKeyCode() >= '1' && key.getKeyCode() <= '9')
    {
        auto index = key.getKeyCode() - '1';
        if (index < (int) nav_.size())
            showView (nav_[(size_t) index].id);
        return true;
    }
    return false;
}

void MainComponent::mouseMove (const juce::MouseEvent& e)
{
    int hover = -1;
    for (int i = 0; i < (int) nav_.size(); ++i)
        if (nav_[(size_t) i].area.contains (e.getPosition()))
            hover = i;
    if (hover != hoverNav_)
    {
        hoverNav_ = hover;
        setMouseCursor (hover >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint (0, headerHeight, sidebarWidth, getHeight());
    }
}

void MainComponent::mouseUp (const juce::MouseEvent& e)
{
    for (auto& item : nav_)
        if (item.area.contains (e.getPosition()))
            showView (item.id);
}

} // namespace pc::ui
