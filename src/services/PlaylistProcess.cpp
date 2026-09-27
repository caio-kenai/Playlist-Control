#include "services/PlaylistProcess.h"
#include "install/Installation.h"
#include "logging/Logger.h"
#include "platform/WindowsSystem.h"

namespace pc
{

namespace
{
constexpr unsigned int bmClick = 0x00F5;
constexpr unsigned int tdmClickButton = 0x0400 + 102; // TDM_CLICK_BUTTON (task dialogs)
constexpr int idOk = 1, idCancel = 2, idYes = 6, idNo = 7;

juce::String plain (const juce::String& caption)
{
    return caption.removeCharacters ("&").trim();
}

// Button of a dialog whose caption or id matches.
const ProcessWindow* findButton (const std::vector<ProcessWindow>& children, const juce::StringArray& captions, int id)
{
    for (auto& c : children)
        if (c.className.containsIgnoreCase ("button") && captions.contains (plain (c.title), true))
            return &c;
    for (auto& c : children)
        if (c.className.containsIgnoreCase ("button") && c.controlId == id)
            return &c;
    return nullptr;
}

juce::String dialogText (const std::vector<ProcessWindow>& children)
{
    juce::StringArray parts;
    for (auto& c : children)
        if (! c.className.containsIgnoreCase ("button") && c.title.isNotEmpty())
            parts.add (c.title);
    return parts.joinIntoString (" ");
}

bool isCloseQuestion (const juce::String& text)
{
    return text.containsIgnoreCase ("fechar o programa") || text.containsIgnoreCase ("deseja fechar")
           || text.containsIgnoreCase ("deseja sair");
}

// Answers a dialog. Returns true when a button was clicked.
bool answer (const ProcessWindow& dialog, bool yes)
{
    auto children = childWindows (dialog.handle);
    juce::StringArray captions;
    if (yes)
        captions.addArray (juce::StringArray { "Sim", "Yes", "OK" });
    else
        captions.addArray (juce::StringArray { juce::String (L"Não"), juce::String ("No"), juce::String ("Cancelar"), juce::String ("Cancel") });
    auto* button = findButton (children, captions, yes ? idYes : idNo);
    if (button == nullptr && ! yes)
        button = findButton (children, {}, idCancel);
    if (button == nullptr && yes)
        button = findButton (children, {}, idOk);
    if (button != nullptr)
        return postToWindow (button->handle, bmClick);
    // Task dialogs draw their buttons without windows; they take TDM_CLICK_BUTTON.
    if (dialog.className == "#32770")
        return postToWindow (dialog.handle, tdmClickButton, yes ? idYes : idNo);
    return false;
}
} // namespace

ProgramCheck checkPlaylistPrograms (const juce::File& pgm)
{
    ProgramCheck c;
    juce::StringArray others;
    for (auto& p : runningProcesses())
    {
        juce::File path (p.fullPath);
        auto lower = p.exeName.toLowerCase();
        if (isPlaylistExecutableName (p.exeName) && p.fullPath.isEmpty())
            c.blockers.add (L"Há um " + p.exeName + L" aberto (PID " + juce::String (p.pid)
                            + L") cuja pasta não pôde ser consultada (ele pode estar rodando como administrador). "
                              L"Abra o Playlist Control como administrador ou feche o Playlist manualmente.");
        else if (isPlaylistExecutableName (p.exeName) && path.isAChildOf (pgm))
            c.playlist.push_back ({ p.pid, p.exeName, path });
        else if (lower == "configmanager.exe" || lower == "ligacao.exe" || lower == "separacomprove.exe"
                 || (p.fullPath.isNotEmpty() && path.isAChildOf (pgm)))
            others.addIfNotAlreadyThere (p.exeName);
        else if (lower == "commercial.exe")
            c.warnings.addIfNotAlreadyThere (L"O Commercial está aberto: é mais seguro fechá-lo durante a operação.");
    }
    for (auto& name : others)
        c.blockers.add (L"Feche o " + name + L" antes: ele usa os arquivos da pasta do Playlist.");

    if (! c.playlist.empty())
        c.playlistExe = c.playlist.front().exe;
    else if (pgm.getChildFile ("Playlist.exe").existsAsFile())
        c.playlistExe = pgm.getChildFile ("Playlist.exe");
    else
        for (auto& f : pgm.findChildFiles (juce::File::findFiles, false, "*.exe"))
            if (isPlaylistExecutableName (f.getFileName()))
                c.playlistExe = f;

    c.warnings.add (L"Estações da rede que abrem o Playlist a partir desta pasta pgm não são detectadas daqui: "
                    L"feche-as antes de continuar.");
    return c;
}

bool closePlaylist (const std::vector<PlaylistProgram>& programs, int timeoutMs,
                    const std::function<void (const juce::String&)>& progress,
                    const std::function<bool()>& shouldStop, juce::String& error)
{
    for (auto& p : programs)
    {
        if (requestClose (p.pid) == 0 && isProcessRunning (p.pid))
        {
            error = L"Não foi possível pedir ao " + p.name + L" que feche (a janela não aceitou a mensagem). "
                    L"Se o Playlist roda como administrador, abra o Playlist Control como administrador.";
            return false;
        }
    }

    auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    juce::Array<juce::pointer_sized_int> answered;
    for (;;)
    {
        bool anyRunning = false;
        const ProcessWindow* pendingQuestion = nullptr;
        std::vector<ProcessWindow> windows;
        for (auto& p : programs)
        {
            if (! isProcessRunning (p.pid))
                continue;
            anyRunning = true;
            for (auto& w : windowsOfProcess (p.pid))
                windows.push_back (w);
        }
        if (! anyRunning)
            return true;

        for (auto& w : windows)
        {
            if (! w.isDialog() || answered.contains (w.handle))
                continue;
            auto text = dialogText (childWindows (w.handle));
            // A question whose text cannot be read (task dialog) right after
            // the close request is taken as the close confirmation.
            if (isCloseQuestion (text) || text.isEmpty())
            {
                if (answer (w, true))
                {
                    answered.add (w.handle);
                    Logger::instance().info ("playlist.close.confirm", L"Confirmação de fechamento respondida com Sim",
                                             { { "text", text } });
                    if (progress)
                        progress (L"Fechamento confirmado na janela do Playlist.");
                    continue;
                }
            }
            pendingQuestion = &w;
            if (progress)
                progress (L"O Playlist está perguntando: \"" + text + L"\". Responda na janela dele.");
        }

        if ((shouldStop && shouldStop()) || juce::Time::getMillisecondCounter() > deadline)
        {
            // Leave the Playlist open: cancel a question still on screen.
            for (auto& w : windows)
                if (w.isDialog())
                    answer (w, false);
            error = L"O Playlist não fechou em " + juce::String (timeoutMs / 1000)
                    + L" s. Nada foi alterado; ele continua aberto.";
            juce::ignoreUnused (pendingQuestion);
            return false;
        }
        juce::Thread::sleep (250);
    }
}

bool startPlaylist (const juce::File& exe, const juce::File& pgm, juce::String& detail)
{
    juce::String error;
    auto pid = launchProcess (exe, pgm, error);
    if (pid == 0)
    {
        detail = L"Não foi possível abrir " + exe.getFileName() + ": " + error + L". Abra o Playlist Digital manualmente.";
        return false;
    }
    for (int i = 0; i < 240; ++i)
    {
        if (! isProcessRunning (pid))
        {
            detail = exe.getFileName() + L" fechou logo após abrir. Abra o Playlist Digital manualmente.";
            return false;
        }
        if (! windowsOfProcess (pid).empty())
        {
            detail = exe.getFileName() + L" aberto (PID " + juce::String (pid) + ").";
            return true;
        }
        juce::Thread::sleep (250);
    }
    detail = exe.getFileName() + L" iniciado (PID " + juce::String (pid) + L"), ainda sem janela visível.";
    return true;
}

} // namespace pc
