#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pc::ui
{

// Line icons drawn with paths in a 24 x 24 box (no image assets).
enum class Icon
{
    dashboard,
    maps,
    grades,
    clock,
    fileSettings,
    sliders,
    folder,
    users,
    stethoscope,
    history,
    database,
    folderOpen,
    refresh,
    lock,
    unlock,
    plus,
    save,
    undo,
    trash,
    play,
    check,
    tag
};

// Draws the icon stroked in 'colour' inside 'area'.
void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour, float strokeWidth = 1.7f);

} // namespace pc::ui
