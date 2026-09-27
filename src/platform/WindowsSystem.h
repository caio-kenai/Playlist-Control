#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace pc
{

// Entry of Windows' "Programs and Features" (Uninstall registry keys).
struct InstalledProgram
{
    juce::String displayName;
    juce::String version;
    juce::String publisher;
    juce::String installLocation;
};

std::vector<InstalledProgram> installedPrograms();

struct RunningProcess
{
    juce::uint32 pid = 0;
    juce::String exeName;
    juce::String fullPath; // empty when the process cannot be queried
};

std::vector<RunningProcess> runningProcesses();

enum class ServiceState
{
    notInstalled,
    stopped,
    starting,
    running,
    stopping,
    paused,
    unknown
};

juce::String toDisplayString (ServiceState state);

struct ServiceInfo
{
    juce::String name;
    juce::String displayName;
    ServiceState state = ServiceState::unknown;
};

// Services whose name or display name contains any of the given words.
std::vector<ServiceInfo> findServices (const juce::StringArray& words);

// Visible top-level window of a process. The Playlist programs are only ever
// driven through their own windows (messages sent to a handle), never with
// global keyboard or mouse input.
struct ProcessWindow
{
    juce::pointer_sized_int handle = 0;
    juce::String title;
    juce::String className;
    bool enabled = true;
    int controlId = 0;
    bool isDialog() const { return className == "#32770" || className == "TMessageForm"; }
};

std::vector<ProcessWindow> windowsOfProcess (juce::uint32 pid);

// Every descendant window of a window (buttons, texts of a dialog).
std::vector<ProcessWindow> childWindows (juce::pointer_sized_int parent);

// Posts a message to one window (never to the whole desktop).
bool postToWindow (juce::pointer_sized_int window, unsigned int message, juce::pointer_sized_int wParam = 0,
                   juce::pointer_sized_int lParam = 0);

bool isProcessRunning (juce::uint32 pid);

// Asks the program to close as if the operator closed its main window
// (WM_CLOSE to its windows, dialogs excluded). Never terminates the process.
int requestClose (juce::uint32 pid);

// True when the process exited within the timeout.
bool waitForProcessExit (juce::uint32 pid, int timeoutMs);

// Starts a program in the given working folder, detached from this one.
// Returns its PID, or 0 with the reason in 'error'.
juce::uint32 launchProcess (const juce::File& exe, const juce::File& workingFolder, juce::String& error);

// Clicks a button of a dialog by posting BM_CLICK to that control.
bool clickDialogButton (juce::pointer_sized_int dialog, int controlId);
juce::String dialogItemText (juce::pointer_sized_int dialog, int controlId);

// Windows shortcut (.lnk), as the Config Manager writes in pgm\Atalhos.
struct ShortcutInfo
{
    juce::String target;
    juce::String arguments;
    juce::String description;
    juce::String iconLocation;
    int iconIndex = 0;
};

bool writeShortcut (const juce::File& lnk, const ShortcutInfo& info, juce::String& error);
std::optional<ShortcutInfo> readShortcut (const juce::File& lnk);

// "5.0.9.9" from the executable's version resource.
juce::String fileVersion (const juce::File& file);

} // namespace pc
