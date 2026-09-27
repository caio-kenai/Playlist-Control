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
