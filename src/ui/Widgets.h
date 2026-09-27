#pragma once

#include "ui/Theme.h"

namespace pc::ui
{

// White panel with an optional title, used as the building block of views.
class Card : public juce::Component
{
public:
    explicit Card (juce::String title = {}) : title_ (std::move (title)) {}
    void setTitle (const juce::String& t) { title_ = t; repaint(); }
    void setAccent (juce::Colour c) { accent_ = c; repaint(); }
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (14).withTrimmedTop (title_.isNotEmpty() ? 28 : 0); }
    void paint (juce::Graphics& g) override;

private:
    juce::String title_;
    juce::Colour accent_ = theme::colours::brandLight;
};

// Coloured strip with a message: read-only mode, conflicts, origin warnings.
class Banner : public juce::Component
{
public:
    Banner();
    void show (Severity severity, const juce::String& text, const juce::String& actionText = {},
               std::function<void()> action = {});
    void hideBanner();
    int preferredHeight() const { return isVisible() ? 34 : 0; }
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    Severity severity_ = Severity::info;
    juce::String text_;
    juce::TextButton action_;
    std::function<void()> onAction_;
};

// Primary (filled) button style.
void makePrimary (juce::TextButton& b);

// Label with the app typography.
void styleLabel (juce::Label& l, float height = 14.0f, bool bold = false, juce::Colour colour = theme::colours::text);

// Plain multi-line read-only text area.
void styleReadOnlyText (juce::TextEditor& e, bool monospace = false);

// Async confirmation with "Confirmar"/"Cancelar".
void confirm (const juce::String& title, const juce::String& message, const juce::String& confirmText,
              std::function<void()> onConfirm);
void inform (const juce::String& title, const juce::String& message, bool isError = false);

} // namespace pc::ui
