#include "ui/Theme.h"

namespace pc::theme
{

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Segoe UI", height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions ("Consolas", height, juce::Font::plain));
}

juce::Colour severityColour (Severity s)
{
    switch (s)
    {
        case Severity::error:   return colours::error;
        case Severity::warning: return colours::warning;
        case Severity::info:    return colours::info;
    }
    return colours::text;
}

juce::Colour severityBackground (Severity s)
{
    switch (s)
    {
        case Severity::error:   return colours::errorBack;
        case Severity::warning: return colours::warningBack;
        case Severity::info:    return colours::infoBack;
    }
    return colours::panel;
}

float badgeWidth (const juce::String& text, float fontHeight)
{
    return juce::GlyphArrangement::getStringWidth (font (fontHeight, true), text) + fontHeight * 1.2f;
}

void drawBadge (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour background,
                juce::Colour foreground, float fontHeight)
{
    g.setColour (background);
    g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    g.setColour (foreground);
    g.setFont (font (fontHeight, true));
    g.drawText (text, area, juce::Justification::centred, false);
}

void drawStatusIcon (juce::Graphics& g, juce::Rectangle<float> area, Severity severity, bool okState)
{
    auto r = area.reduced (1.0f);
    if (okState)
    {
        g.setColour (colours::ok);
        juce::Path tick;
        tick.startNewSubPath (r.getX() + r.getWidth() * 0.18f, r.getCentreY());
        tick.lineTo (r.getX() + r.getWidth() * 0.42f, r.getBottom() - r.getHeight() * 0.2f);
        tick.lineTo (r.getRight() - r.getWidth() * 0.12f, r.getY() + r.getHeight() * 0.2f);
        g.strokePath (tick, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
    if (severity == Severity::error)
    {
        // Red X, as the Playlist shows for items that will not play.
        g.setColour (colours::error);
        auto x = r.reduced (r.getWidth() * 0.15f);
        g.drawLine ({ x.getTopLeft(), x.getBottomRight() }, 2.4f);
        g.drawLine ({ x.getTopRight(), x.getBottomLeft() }, 2.4f);
        return;
    }
    if (severity == Severity::info)
    {
        g.setColour (colours::info);
        g.fillEllipse (r);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions ("Georgia", r.getHeight() * 0.85f, juce::Font::bold)));
        g.drawText ("i", r, juce::Justification::centred, false);
        return;
    }
    g.setColour (severityColour (severity));
    juce::Path tri;
    tri.addTriangle (r.getCentreX(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getBottom());
    g.fillPath (tri);
    g.setColour (juce::Colours::white);
    g.setFont (font (r.getHeight() * 0.75f, true));
    g.drawText ("!", r.withTrimmedTop (r.getHeight() * 0.2f), juce::Justification::centred, false);
}

LookAndFeel::LookAndFeel()
{
    using namespace colours;
    auto scheme = juce::LookAndFeel_V4::getLightColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, background);
    scheme.setUIColour (ColourScheme::widgetBackground, panel);
    scheme.setUIColour (ColourScheme::menuBackground, panel);
    scheme.setUIColour (ColourScheme::outline, border);
    scheme.setUIColour (ColourScheme::defaultText, text);
    scheme.setUIColour (ColourScheme::defaultFill, brandLight);
    scheme.setUIColour (ColourScheme::highlightedText, juce::Colours::white);
    scheme.setUIColour (ColourScheme::highlightedFill, brandLight);
    scheme.setUIColour (ColourScheme::menuText, text);
    setColourScheme (scheme);

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::buttonOnColourId, brandLight);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, brandLight);
    setColour (juce::TextEditor::highlightColourId, brandLight.withAlpha (0.25f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textMuted);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ListBox::outlineColourId, border);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, brandLight);
    setColour (juce::ToggleButton::tickDisabledColourId, border);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xffaab4c2));
    setColour (juce::TableHeaderComponent::backgroundColourId, panelAlt);
    setColour (juce::TableHeaderComponent::textColourId, textMuted);
    setColour (juce::TableHeaderComponent::outlineColourId, border);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, brandLight);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, border);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff27313d));
    setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff27313d));
    setColour (juce::CaretComponent::caretColourId, brand);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (15.0f, (float) buttonHeight * 0.52f));
}

juce::Font LookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont().getTypefaceName() == juce::Font::getDefaultSansSerifFontName() ? font (14.0f) : l.getFont();
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (14.0f); }
juce::Font LookAndFeel::getPopupMenuFont() { return font (14.5f); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return font (14.5f); }
juce::Font LookAndFeel::getAlertWindowTitleFont() { return font (17.0f, true); }

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                        bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto base = background;
    if (! b.isEnabled())
        base = base.withMultipliedSaturation (0.3f).withAlpha (0.6f);
    else if (down)
        base = base.darker (0.12f);
    else if (highlighted)
        base = base.brighter (0.06f).overlaidWith (colours::brandLight.withAlpha (0.06f));
    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (base == colours::panel ? colours::border : base.darker (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawTickBox (juce::Graphics& g, juce::Component& c, float x, float y, float w, float h, bool ticked,
                               bool enabled, bool, bool)
{
    juce::Rectangle<float> r (x, y, w, h);
    r = r.reduced (1.0f);
    g.setColour (ticked ? colours::brandLight : colours::panel);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ticked ? colours::brandLight.darker (0.2f) : colours::border.darker (0.1f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    if (ticked)
    {
        juce::Path tick;
        tick.startNewSubPath (r.getX() + r.getWidth() * 0.22f, r.getCentreY());
        tick.lineTo (r.getX() + r.getWidth() * 0.43f, r.getBottom() - r.getHeight() * 0.25f);
        tick.lineTo (r.getRight() - r.getWidth() * 0.2f, r.getY() + r.getHeight() * 0.25f);
        g.setColour (juce::Colours::white);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (! enabled)
    {
        g.setColour (colours::background.withAlpha (0.5f));
        g.fillRect (r);
    }
    juce::ignoreUnused (c);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& e)
{
    if (! e.isEnabled())
        return;
    g.setColour (e.hasKeyboardFocus (true) && ! e.isReadOnly() ? colours::brandLight : colours::border);
    g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f), 3.0f,
                            e.hasKeyboardFocus (true) ? 1.6f : 1.0f);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& e)
{
    g.setColour (e.isReadOnly() ? colours::panelAlt : e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) width, (float) height), 3.0f);
}

} // namespace pc::theme
