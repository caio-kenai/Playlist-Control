#include <juce_gui_basics/juce_gui_basics.h>

#include "logging/Logger.h"
#include "platform/WinInclude.h"
#include "services/AppSettings.h"
#include "services/Workspace.h"
#include "ui/MainComponent.h"
#include "ui/views/ScheduleView.h"

#include "Version.h"

namespace
{

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (pc::Workspace& workspace, pc::AppSettings& settings)
        : juce::DocumentWindow ("Playlist Control", pc::theme::colours::background, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        auto* content = new pc::ui::MainComponent (workspace, settings);
        main_ = content;
        setContentOwned (content, false);
        setResizable (true, true);
        setResizeLimits (1100, 700, 10000, 10000);
        centreWithSize (1440, 900);
        setVisible (true);
        applyIcon();
    }

    // The icon resource embedded by PlaylistControl.rc, for the title bar
    // and the taskbar.
    void applyIcon()
    {
        auto hwnd = static_cast<HWND> (getWindowHandle());
        if (hwnd == nullptr)
            return;
        auto instance = GetModuleHandleW (nullptr);
        auto load = [instance] (int size) {
            return static_cast<HICON> (LoadImageW (instance, MAKEINTRESOURCEW (1), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
        };
        if (auto big = load (GetSystemMetrics (SM_CXICON)))
            SendMessageW (hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM> (big));
        if (auto small = load (GetSystemMetrics (SM_CXSMICON)))
            SendMessageW (hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM> (small));
    }

    void closeButtonPressed() override
    {
        main_->guardUnsaved ([] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });
    }

    pc::ui::MainComponent* main_ = nullptr;
};

// Renders every view to PNG files without showing a window:
//   PlaylistControl --screenshots=<folder> --installation=<pgm> [--only=<name>] [--height=<px>]
int renderScreenshots (const juce::ArgumentList& args)
{
    juce::File out (args.getValueForOption ("--screenshots"));
    juce::File pgm (args.getValueForOption ("--installation"));
    out.createDirectory();
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("PlaylistControlShots", "");

    pc::AppSettings settings;
    pc::Workspace workspace (temp.getChildFile ("history"));
    if (! workspace.open (pgm))
        return 2;

    pc::ui::MainComponent main (workspace, settings);
    auto height = args.getValueForOption ("--height").getIntValue();
    main.setSize (1440, height > 0 ? height : 900);
    const std::pair<pc::ui::ViewId, const char*> shots[] = {
        { pc::ui::ViewId::dashboard, "01-painel" },
        { pc::ui::ViewId::maps, "02-mapas" },
        { pc::ui::ViewId::grades, "03-grades" },
        { pc::ui::ViewId::clocks, "04-relogios" },
        { pc::ui::ViewId::playlistIni, "05-leitura-de-mapas" },
        { pc::ui::ViewId::config, "06-opcoes" },
        { pc::ui::ViewId::folders, "07-pastas" },
        { pc::ui::ViewId::operators, "08-operadores" },
        { pc::ui::ViewId::diagnostics, "09-diagnostico" },
        { pc::ui::ViewId::indexes, "10-indices" },
        { pc::ui::ViewId::history, "11-historico" },
    };
    // --demo-edit performs one edit through the same path as the interface
    // (block edit, validation, safe write, history). Use it only on a copy.
    if (args.containsOption ("--demo-edit"))
    {
        workspace.setReadOnly (false);
        auto map = pgm.getChildFile ("Mapas").getChildFile (args.getValueForOption ("--demo-edit"));
        main.showView (pc::ui::ViewId::maps, map, 0);
        if (auto* view = dynamic_cast<pc::ui::ScheduleView*> (main.view()); view != nullptr && view->document() != nullptr)
        {
            auto& lines = view->document()->lines();
            for (int i = (int) lines.size() - 1; i >= 0; --i)
            {
                if (lines[(size_t) i].kind == pc::ScheduleLine::Kind::block && lines[(size_t) i].block.items.empty())
                {
                    lines[(size_t) i].block.items.push_back (pc::ScheduleItem::makeCode ("55"));
                    view->documentEdited (i, "55 adicionado ao bloco " + lines[(size_t) i].block.time.toString());
                    view->save();
                    break;
                }
            }
        }
        workspace.setReadOnly (true);
    }

    auto only = args.getValueForOption ("--only");
    for (auto& [id, name] : shots)
    {
        if (only.isNotEmpty() && ! juce::String (name).contains (only))
            continue;
        main.showView (id);
        main.dismissToast();
        auto image = main.createComponentSnapshot (main.getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto file = out.getChildFile (juce::String (name) + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        png.writeImageToStream (image, stream);
    }
    workspace.close();
    temp.deleteRecursively();
    return 0;
}

} // namespace

class PlaylistControlApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Playlist Control"; }
    const juce::String getApplicationVersion() override { return PLAYLISTCONTROL_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel_);
        juce::ArgumentList args ("PlaylistControl", commandLine);

        if (args.containsOption ("--screenshots"))
        {
            setApplicationReturnValue (renderScreenshots (args));
            quit();
            return;
        }

        pc::Logger::instance().open (pc::AppSettings::logFolder());
        pc::Logger::instance().info ("app.start", juce::String ("Playlist Control ") + PLAYLISTCONTROL_VERSION_STRING,
                                     { { "user", juce::SystemStats::getLogonName() }, { "machine", juce::SystemStats::getComputerName() } });

        workspace_ = std::make_unique<pc::Workspace> (pc::AppSettings::historyFolder());
        workspace_->history().prune (settings_.historyKeepDays());

        // The saved installation is reopened only if the operator confirmed it;
        // otherwise the known locations are tried, always in read-only mode.
        auto saved = settings_.pgmFolder();
        bool opened = settings_.installationConfirmed() && saved.isDirectory() && workspace_->open (saved);
        if (! opened)
        {
            for (auto& candidate : pc::findInstallationCandidates())
            {
                if (pc::inspectInstallation (candidate.pgm).valid && workspace_->open (candidate.pgm))
                {
                    pc::Logger::instance().info ("installation.detected", candidate.source, { { "pgm", candidate.pgm.getFullPathName() } });
                    break;
                }
            }
        }
        workspace_->setReadOnly (! settings_.installationConfirmed() || settings_.readOnly());

        window_ = std::make_unique<MainWindow> (*workspace_, settings_);
    }

    void shutdown() override
    {
        window_.reset();
        if (workspace_ != nullptr)
            workspace_->close();
        workspace_.reset();
        pc::Logger::instance().info ("app.stop", "Playlist Control encerrado");
        pc::Logger::instance().close();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    pc::theme::LookAndFeel lookAndFeel_;
    pc::AppSettings settings_;
    std::unique_ptr<pc::Workspace> workspace_;
    std::unique_ptr<MainWindow> window_;
};

START_JUCE_APPLICATION (PlaylistControlApplication)
