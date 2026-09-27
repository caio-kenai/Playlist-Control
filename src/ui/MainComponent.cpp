#include "ui/MainComponent.h"
#include "logging/Logger.h"
#include "ui/views/ViewFactory.h"

#include "Version.h"
#include <BinaryData.h>

namespace pc::ui
{

using namespace theme;

namespace
{
constexpr int headerHeight = 58;
constexpr int sidebarWidth = 212;
constexpr int statusHeight = 26;
} // namespace

MainComponent::MainComponent (Workspace& workspace, AppSettings& settings)
    : workspace_ (workspace),
      settings_ (settings),
      context_ { workspace, settings, [this] (const juce::String& s) { setStatus (s); },
                 [this] (ViewId id, const juce::File& f, int line) { showView (id, f, line); } }
{
    nav_ = {
        { ViewId::dashboard, "Painel", "" },
        { ViewId::maps, "Mapas comerciais", L"Programação" },
        { ViewId::grades, "Grades musicais", L"Programação" },
        { ViewId::clocks, L"Relógios", L"Programação" },
        { ViewId::playlistIni, "Leitura de mapas", L"Configuração" },
        { ViewId::config, L"Opções do Playlist", L"Configuração" },
        { ViewId::folders, L"Pastas e códigos", L"Configuração" },
        { ViewId::operators, "Operadores", L"Configuração" },
        { ViewId::diagnostics, L"Diagnóstico", "Suporte" },
        { ViewId::history, L"Histórico", "Suporte" },
    };

    addAndMakeVisible (banner_);
    for (auto* b : { &modeButton_, &installButton_, &reloadButton_ })
    {
        addAndMakeVisible (*b);
        b->setColour (juce::TextButton::buttonColourId, colours::brand.brighter (0.15f));
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }
    installButton_.setButtonText (L"Instalação");
    installButton_.setTooltip (L"Escolher a pasta pgm do Playlist Digital");
    installButton_.onClick = [this] { chooseInstallation(); };
    reloadButton_.setButtonText ("Recarregar");
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
                     L"O PlaylistControl passará a gravar nos arquivos do Playlist Digital quando você salvar.\n\n"
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
    status_ = text;
    statusTime_ = juce::Time::getCurrentTime();
    repaint (0, getHeight() - statusHeight, getWidth(), statusHeight);
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
    modeButton_.setButtonText (workspace_.readOnly() ? L"Somente leitura" : L"Alterações permitidas");
    modeButton_.setColour (juce::TextButton::buttonColourId,
                           workspace_.readOnly() ? colours::brand.brighter (0.15f) : colours::warning);
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
                                            info.summary() + L"\n\nO PlaylistControl passará a ler e controlar os arquivos desta pasta.",
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

void MainComponent::paintHeader (juce::Graphics& g, juce::Rectangle<int> r)
{
    juce::ColourGradient grad (colours::brand, 0.0f, 0.0f, colours::brand.brighter (0.25f), (float) r.getWidth(), 0.0f, false);
    g.setGradientFill (grad);
    g.fillRect (r);

    // Symbol and wordmark of the project logo.
    auto logo = r.removeFromLeft (sidebarWidth).reduced (14, 0);
    static const auto symbol = juce::ImageCache::getFromMemory (PlaylistControlAssets::symbol_png, PlaylistControlAssets::symbol_pngSize);
    g.setOpacity (1.0f);
    g.drawImageWithin (symbol, logo.getX(), 11, 36, 36, juce::RectanglePlacement::centred);
    auto word = logo.withTrimmedLeft (44);
    auto wordFont = juce::Font (juce::FontOptions ("Segoe UI", 21.0f, juce::Font::bold));
    auto playlistWidth = juce::GlyphArrangement::getStringWidth (wordFont, "playlist");
    g.setFont (wordFont);
    g.setColour (juce::Colours::white);
    g.drawText ("playlist", word.getX(), 0, (int) playlistWidth + 2, headerHeight, juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff29c3ff));
    g.drawText ("control", word.getX() + (int) playlistWidth, 0, 100, headerHeight, juce::Justification::centredLeft, false);

    auto pill = modeButton_.getBounds().toFloat().translated (-146.0f, 0.0f).withWidth (136.0f);
    auto info = r.withRight ((int) pill.getX() - 12);
    g.setColour (juce::Colours::white);
    g.setFont (font (16.0f, true));
    g.drawText (view_ != nullptr ? view_->title() : juce::String(), info.removeFromTop (34).withTrimmedTop (8),
                juce::Justification::bottomLeft, true);
    g.setColour (juce::Colour (0xffbcd0ee));
    g.setFont (font (12.5f));
    juce::String sub = view_ != nullptr ? view_->subtitle() : juce::String();
    g.drawText (sub, info, juce::Justification::topLeft, true);

    // Playlist Digital running state, like the "No ar" display.
    bool running = workspace_.isOpen() && workspace_.runtime().playlistRunning;
    g.setColour (running ? colours::onAirBack : colours::brand.darker (0.3f));
    g.fillRoundedRectangle (pill, 5.0f);
    g.setColour (running ? colours::onAirText : juce::Colour (0xff8ea6c9));
    g.setFont (font (12.5f, true));
    g.drawText (running ? "PLAYLIST NO AR" : "PLAYLIST FECHADO", pill, juce::Justification::centred, false);
}

void MainComponent::paintSidebar (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (colours::panel);
    g.fillRect (r);
    g.setColour (colours::border);
    g.drawVerticalLine (r.getRight() - 1, (float) r.getY(), (float) r.getBottom());

    auto a = r.reduced (0, 10);
    juce::String group;
    for (int i = 0; i < (int) nav_.size(); ++i)
    {
        auto& item = nav_[(size_t) i];
        if (item.group != group)
        {
            group = item.group;
            auto h = a.removeFromTop (30);
            g.setColour (colours::textMuted);
            g.setFont (font (11.0f, true).withExtraKerningFactor (0.08f));
            g.drawText (group.toUpperCase(), h.withTrimmedLeft (18).withTrimmedTop (10), juce::Justification::centredLeft, false);
        }
        item.area = a.removeFromTop (34);
        bool selected = item.id == current_;
        if (selected)
        {
            g.setColour (colours::infoBack);
            g.fillRect (item.area.reduced (8, 2));
            g.setColour (colours::brandLight);
            g.fillRect (item.area.getX() + 8, item.area.getY() + 4, 3, item.area.getHeight() - 8);
        }
        else if (i == hoverNav_)
        {
            g.setColour (colours::panelAlt);
            g.fillRect (item.area.reduced (8, 2));
        }
        g.setColour (selected ? colours::brand : colours::text);
        g.setFont (font (14.0f, selected));
        g.drawText (item.label, item.area.withTrimmedLeft (22), juce::Justification::centredLeft, true);
        auto n = problemCount (item.id);
        if (n > 0)
            drawBadge (g, item.area.toFloat().removeFromRight (46).withSizeKeepingCentre (30, 18), juce::String (n),
                       colours::errorBack, colours::error);
    }

    g.setColour (colours::textMuted);
    g.setFont (font (11.5f));
    g.drawText (juce::String (L"versão ") + PLAYLISTCONTROL_VERSION_STRING, r.removeFromBottom (28).withTrimmedLeft (18),
                juce::Justification::centredLeft, false);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    auto r = getLocalBounds();
    paintHeader (g, r.removeFromTop (headerHeight));
    auto status = r.removeFromBottom (statusHeight);
    paintSidebar (g, r.removeFromLeft (sidebarWidth));

    g.setColour (colours::panel);
    g.fillRect (status);
    g.setColour (colours::border);
    g.drawHorizontalLine (status.getY(), 0.0f, (float) getWidth());
    g.setColour (colours::textMuted);
    g.setFont (font (12.5f));
    auto s = status.reduced (12, 0);
    if (workspace_.isOpen())
        g.drawText (workspace_.pgm().getFullPathName(), s.removeFromRight (360), juce::Justification::centredRight, true);
    if (status_.isNotEmpty())
        g.drawText (statusTime_.formatted ("%H:%M:%S  ") + status_, s, juce::Justification::centredLeft, true);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (10, 14);
    reloadButton_.setBounds (header.removeFromRight (96));
    header.removeFromRight (6);
    installButton_.setBounds (header.removeFromRight (96));
    header.removeFromRight (6);
    modeButton_.setBounds (header.removeFromRight (160));

    r.removeFromBottom (statusHeight);
    r.removeFromLeft (sidebarWidth);
    banner_.setBounds (r.removeFromTop (banner_.preferredHeight()));
    if (view_ != nullptr)
        view_->setBounds (r);
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
