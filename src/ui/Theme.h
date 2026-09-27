#pragma once

#include "core/Diagnostic.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace pc::theme
{

// Palette taken from the Playlist Digital "Standard" appearance model
// (Operadores\Padrão\layout\Standard.xml) plus neutral tones for the frame.
namespace colours
{
inline const juce::Colour background { 0xffe8ecf1 };
inline const juce::Colour panel { 0xffffffff };
inline const juce::Colour panelAlt { 0xfff4f6f9 };
inline const juce::Colour border { 0xffc8d1dc };
inline const juce::Colour text { 0xff1b2430 };
inline const juce::Colour textMuted { 0xff5b6776 };

inline const juce::Colour brand { 0xff0e3b7d };      // header navy
inline const juce::Colour brandLight { 0xff1f5fb8 };
inline const juce::Colour selection { 0xff5cb142 };  // crSelectedBack
inline const juce::Colour selectionText { 0xffccffcc };

inline const juce::Colour commercial { 0xff1a6b23 }; // crIdCBack
inline const juce::Colour commercialText { 0xffd8f5db };
inline const juce::Colour commercialAlt { 0xfff6fff6 };
inline const juce::Colour musical { 0xff003c80 };    // crIdMBack
inline const juce::Colour musicalText { 0xffe4f1ff };
inline const juce::Colour musicalAlt { 0xffebf3fc };
inline const juce::Colour next { 0xfff4f389 };       // crNxBack
inline const juce::Colour onAirBack { 0xff000000 };
inline const juce::Colour onAirText { 0xffe40000 };

inline const juce::Colour error { 0xffc62828 };
inline const juce::Colour errorBack { 0xfffdecec };
inline const juce::Colour warning { 0xffa86b00 };
inline const juce::Colour warningBack { 0xfffff4d6 };
inline const juce::Colour ok { 0xff2e7d32 };
inline const juce::Colour okBack { 0xffe7f4e8 };
inline const juce::Colour info { 0xff1f5fb8 };
inline const juce::Colour infoBack { 0xffe7effb };
} // namespace colours

juce::Font font (float height, bool bold = false);
juce::Font monoFont (float height);

juce::Colour severityColour (Severity s);
juce::Colour severityBackground (Severity s);

// Small rounded label: "SAT", "Planner", "3 erros"...
void drawBadge (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour background,
                juce::Colour foreground, float fontHeight = 11.0f);
float badgeWidth (const juce::String& text, float fontHeight = 11.0f);

// Status glyphs drawn with paths (no image assets).
void drawStatusIcon (juce::Graphics& g, juce::Rectangle<float> area, Severity severity, bool okState);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowTitleFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h, bool ticked,
                      bool enabled, bool highlighted, bool down) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
};

} // namespace pc::theme
