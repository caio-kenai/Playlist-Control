#pragma once

#include <juce_core/juce_core.h>

#include <functional>

namespace pc
{

// Running Playlist Digital of an installation.
struct PlaylistProgram
{
    juce::uint32 pid = 0;
    juce::String name;
    juce::File exe;
};

struct ProgramCheck
{
    std::vector<PlaylistProgram> playlist; // Playlist Digital processes of this pgm
    juce::StringArray blockers;            // reasons that prevent the operation
    juce::StringArray warnings;            // points the operator should know
    juce::File playlistExe;                // executable to start at the end
};

// Programs that must be closed before files of the pgm are replaced: the
// Playlist Digital of this pgm (closed by the operation), and the Config
// Manager, Ligacao, SeparaComprove or any other program of the pgm folder
// (blockers). A Playlist whose folder cannot be read is never closed from
// here: it is a blocker too.
ProgramCheck checkPlaylistPrograms (const juce::File& pgm);

// Step of an operation shown to the operator.
struct JobStep
{
    enum class State
    {
        pending,
        running,
        done,
        failed,
        skipped
    };

    juce::String title;
    State state = State::pending;
    juce::String detail;
};

// Closes the Playlist through its windows, as the operator would (WM_CLOSE),
// never terminating the process. When the Playlist asks "Deseja fechar o
// programa?", the answer is Sim (the operator already confirmed in the
// Playlist Control). Any other question is left to the operator; if it is not
// answered in time, the question is cancelled so the Playlist stays open.
// 'progress' receives texts for the operator.
bool closePlaylist (const std::vector<PlaylistProgram>& programs, int timeoutMs,
                    const std::function<void (const juce::String&)>& progress,
                    const std::function<bool()>& shouldStop, juce::String& error);

// Starts the Playlist from the pgm and waits for its window.
bool startPlaylist (const juce::File& exe, const juce::File& pgm, juce::String& detail);

} // namespace pc
