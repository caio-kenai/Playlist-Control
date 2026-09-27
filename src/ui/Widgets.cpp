#include "ui/Widgets.h"

namespace pc::ui
{

using namespace theme;

void Card::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    if (title_.isNotEmpty())
    {
        g.setColour (accent_);
        g.fillRoundedRectangle (juce::Rectangle<float> (14.0f, 14.0f, 4.0f, 16.0f), 2.0f);
        g.setColour (colours::text);
        g.setFont (font (15.0f, true));
        g.drawText (title_, juce::Rectangle<int> (24, 10, getWidth() - 38, 24), juce::Justification::centredLeft, true);
    }
}

Banner::Banner()
{
    addChildComponent (action_);
    action_.onClick = [this] { if (onAction_) onAction_(); };
    setVisible (false);
}

void Banner::show (Severity severity, const juce::String& text, const juce::String& actionText, std::function<void()> action)
{
    severity_ = severity;
    text_ = text;
    onAction_ = std::move (action);
    action_.setButtonText (actionText);
    action_.setVisible (actionText.isNotEmpty());
    setVisible (true);
    resized();
    repaint();
}

void Banner::hideBanner()
{
    setVisible (false);
}

void Banner::paint (juce::Graphics& g)
{
    g.setColour (severityBackground (severity_));
    g.fillRect (getLocalBounds());
    g.setColour (severityColour (severity_));
    g.fillRect (0, 0, 4, getHeight());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    drawStatusIcon (g, juce::Rectangle<float> (12.0f, (float) getHeight() / 2 - 8, 16.0f, 16.0f), severity_, false);
    g.setColour (colours::text);
    g.setFont (font (13.5f));
    auto textArea = getLocalBounds().withTrimmedLeft (36).withTrimmedRight (action_.isVisible() ? action_.getWidth() + 20 : 10);
    g.drawFittedText (text_, textArea, juce::Justification::centredLeft, 2);
}

void Banner::resized()
{
    auto w = juce::jmax (90, (int) (juce::GlyphArrangement::getStringWidth (font (14.0f), action_.getButtonText()) + 28));
    action_.setBounds (getWidth() - w - 8, 5, w, getHeight() - 10);
}

ActionButton::ActionButton (const juce::String& text, std::optional<Icon> icon, Style style)
    : juce::Button (text), icon_ (icon), style_ (style)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int ActionButton::preferredWidth (int height) const
{
    auto textWidth = getButtonText().isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font (14.0f, true), getButtonText());
    auto iconWidth = icon_.has_value() ? (float) height * 0.5f + (getButtonText().isEmpty() ? 0.0f : 8.0f) : 0.0f;
    return (int) std::ceil (textWidth + iconWidth + (getButtonText().isEmpty() ? (float) height * 0.5f : 30.0f));
}

void ActionButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float radius = 8.0f;
    juce::Colour fill, border, ink;
    switch (style_)
    {
        case Style::primary:
            fill = colours::brandLight;
            border = colours::brandLight.darker (0.25f);
            ink = juce::Colours::white;
            if (over) fill = fill.brighter (0.12f);
            break;
        case Style::secondary:
            fill = over ? colours::panelAlt : colours::panel;
            border = over ? colours::brandLight.withAlpha (0.55f) : colours::border;
            ink = colours::text;
            break;
        case Style::danger:
            fill = over ? colours::errorBack.darker (0.03f) : colours::panel;
            border = colours::error.withAlpha (over ? 0.8f : 0.45f);
            ink = colours::error;
            break;
        case Style::header:
            fill = juce::Colours::white.withAlpha (over ? 0.16f : 0.08f);
            border = juce::Colours::white.withAlpha (over ? 0.35f : 0.18f);
            ink = juce::Colours::white;
            break;
        case Style::headerAccent:
            fill = juce::Colour (0xffffb020).withAlpha (over ? 0.32f : 0.22f);
            border = juce::Colour (0xffffc452).withAlpha (0.8f);
            ink = juce::Colour (0xffffe2a8);
            break;
    }
    if (down)
        fill = fill.darker (0.08f);
    if (! isEnabled())
    {
        fill = fill.withMultipliedAlpha (0.55f);
        border = border.withMultipliedAlpha (0.5f);
        ink = ink.withMultipliedAlpha (0.45f);
    }
    if (style_ == Style::primary && isEnabled())
    {
        g.setGradientFill (juce::ColourGradient (fill.brighter (0.08f), 0, r.getY(), fill.darker (0.08f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
    }
    else
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, radius);
    }
    g.setColour (border);
    g.drawRoundedRectangle (r, radius, 1.0f);
    if (hasKeyboardFocus (false) && style_ != Style::header && style_ != Style::headerAccent)
    {
        g.setColour (colours::brandLight.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (1.5f), radius + 1.5f, 2.0f);
    }

    auto content = getLocalBounds().toFloat().reduced (12.0f, 0.0f);
    auto label = getButtonText();
    auto iconSize = juce::jmin (18.0f, (float) getHeight() * 0.5f);
    auto textWidth = label.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font (14.0f, true), label);
    auto total = textWidth + (icon_.has_value() ? iconSize + (label.isEmpty() ? 0.0f : 8.0f) : 0.0f);
    auto x = content.getCentreX() - total / 2.0f;
    if (icon_.has_value())
    {
        drawIcon (g, *icon_, { x, content.getCentreY() - iconSize / 2.0f, iconSize, iconSize }, ink, 1.8f);
        x += iconSize + 8.0f;
    }
    if (label.isNotEmpty())
    {
        g.setColour (ink);
        g.setFont (font (14.0f, true));
        g.drawText (label, juce::Rectangle<float> (x, 0.0f, textWidth + 2.0f, (float) getHeight()), juce::Justification::centredLeft, false);
    }
}

Toast::Toast()
{
    setInterceptsMouseClicks (false, false);
    setVisible (false);
}

int Toast::preferredWidth() const
{
    return juce::jmin (560, (int) juce::GlyphArrangement::getStringWidth (font (13.5f), text_) + 64);
}

void Toast::show (const juce::String& text, Severity severity)
{
    text_ = text;
    severity_ = severity;
    shownAt_ = juce::Time::getMillisecondCounter();
    setAlpha (1.0f);
    setVisible (true);
    if (auto* parent = getParentComponent())
        parent->resized();
    toFront (false);
    repaint();
    startTimerHz (30);
}

void Toast::timerCallback()
{
    auto elapsed = (int) (juce::Time::getMillisecondCounter() - shownAt_);
    const int visibleFor = 3200, fade = 500;
    if (elapsed > visibleFor + fade)
    {
        stopTimer();
        setVisible (false);
        return;
    }
    if (elapsed > visibleFor)
        setAlpha (1.0f - (float) (elapsed - visibleFor) / (float) fade);
}

void Toast::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 10, { 0, 3 }).drawForRectangle (g, r.toNearestInt());
    g.setColour (juce::Colour (0xff1f2a37));
    g.fillRoundedRectangle (r, 9.0f);
    auto accent = severity_ == Severity::info ? colours::ok : severityColour (severity_);
    g.setColour (accent);
    g.fillEllipse (r.getX() + 14.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (juce::Colours::white);
    g.setFont (font (13.5f));
    g.drawFittedText (text_, r.toNearestInt().withTrimmedLeft (32).withTrimmedRight (12), juce::Justification::centredLeft, 2);
}

void makePrimary (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, colours::brandLight);
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
}

void styleLabel (juce::Label& l, float height, bool bold, juce::Colour colour)
{
    l.setFont (font (height, bold));
    l.setColour (juce::Label::textColourId, colour);
    l.setMinimumHorizontalScale (1.0f);
}

void styleReadOnlyText (juce::TextEditor& e, bool monospace)
{
    e.setMultiLine (true, true);
    e.setReadOnly (true);
    e.setScrollbarsShown (true);
    e.setCaretVisible (false);
    e.setFont (monospace ? monoFont (13.0f) : font (13.5f));
    e.setColour (juce::TextEditor::backgroundColourId, colours::panel);
}

void confirm (const juce::String& title, const juce::String& message, const juce::String& confirmText,
              std::function<void()> onConfirm)
{
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle (title)
                       .withMessage (message)
                       .withButton (confirmText)
                       .withButton ("Cancelar");
    juce::AlertWindow::showAsync (options, [cb = std::move (onConfirm)] (int result) {
        if (result == 1 && cb)
            cb();
    });
}

void inform (const juce::String& title, const juce::String& message, bool isError)
{
    auto options = juce::MessageBoxOptions()
                       .withIconType (isError ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon)
                       .withTitle (title)
                       .withMessage (message)
                       .withButton ("OK");
    juce::AlertWindow::showAsync (options, nullptr);
}

} // namespace pc::ui
