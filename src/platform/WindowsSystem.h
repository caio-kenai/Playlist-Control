#pragma once

#include <juce_core/juce_core.h>

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

// "5.0.9.9" from the executable's version resource.
juce::String fileVersion (const juce::File& file);

} // namespace pc
