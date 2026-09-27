#include "ui/Icons.h"

namespace pc::ui
{

namespace
{
juce::Path build (Icon icon)
{
    juce::Path p;
    auto rr = [&] (float x, float y, float w, float h, float r) { p.addRoundedRectangle (x, y, w, h, r); };
    auto line = [&] (float x1, float y1, float x2, float y2) {
        p.startNewSubPath (x1, y1);
        p.lineTo (x2, y2);
    };
    switch (icon)
    {
        case Icon::dashboard:
            rr (3, 3, 8, 8, 2); rr (13, 3, 8, 5, 2); rr (13, 10, 8, 11, 2); rr (3, 13, 8, 8, 2);
            break;
        case Icon::maps:
            rr (3, 4, 18, 16, 2.5f);
            line (3, 9, 21, 9);
            line (7, 13, 17, 13); line (7, 16.5f, 14, 16.5f);
            break;
        case Icon::grades:
            // music note
            p.addEllipse (4, 15, 6, 5); p.addEllipse (14, 13, 6, 5);
            line (10, 17.5f, 10, 5); line (20, 15.5f, 20, 3.5f); line (10, 5, 20, 3.5f); line (10, 8.5f, 20, 7);
            break;
        case Icon::clock:
            p.addEllipse (3, 3, 18, 18);
            line (12, 7, 12, 12); line (12, 12, 15.5f, 14);
            break;
        case Icon::fileSettings:
            p.startNewSubPath (6, 3); p.lineTo (14, 3); p.lineTo (19, 8); p.lineTo (19, 21); p.lineTo (6, 21); p.closeSubPath();
            line (14, 3, 14, 8); line (14, 8, 19, 8);
            line (9, 12, 16, 12); line (9, 15.5f, 16, 15.5f); line (9, 19, 13, 19);
            break;
        case Icon::sliders:
            line (4, 7, 20, 7); line (4, 12, 20, 12); line (4, 17, 20, 17);
            p.addEllipse (7, 5, 4, 4); p.addEllipse (13, 10, 4, 4); p.addEllipse (9, 15, 4, 4);
            break;
        case Icon::folder:
            p.startNewSubPath (3, 7); p.lineTo (3, 19); p.lineTo (21, 19); p.lineTo (21, 8.5f); p.lineTo (11.5f, 8.5f);
            p.lineTo (9.5f, 5.5f); p.lineTo (3, 5.5f); p.closeSubPath();
            break;
        case Icon::users:
            p.addEllipse (6, 4, 7, 7); p.addEllipse (15, 6, 5, 5);
            p.startNewSubPath (3, 20); p.cubicTo (3, 15, 6, 13, 9.5f, 13); p.cubicTo (13, 13, 16, 15, 16, 20);
            p.startNewSubPath (16.5f, 13.5f); p.cubicTo (19, 13.5f, 21, 15.5f, 21, 19);
            break;
        case Icon::stethoscope:
            // magnifier with check
            p.addEllipse (3, 3, 13, 13);
            line (14, 14, 21, 21);
            line (6.5f, 9.5f, 8.8f, 11.8f); line (8.8f, 11.8f, 12.8f, 7);
            break;
        case Icon::history:
            p.startNewSubPath (4.5f, 8.5f);
            p.addCentredArc (12, 12, 8.5f, 8.5f, 0, -2.2f, 2.8f, true);
            line (4.5f, 8.5f, 4.5f, 4); line (4.5f, 8.5f, 9, 8.5f);
            line (12, 8, 12, 12.5f); line (12, 12.5f, 15, 14.5f);
            break;
        case Icon::database:
            p.addEllipse (4, 3, 16, 5);
            p.startNewSubPath (4, 5.5f); p.lineTo (4, 18.5f);
            p.startNewSubPath (20, 5.5f); p.lineTo (20, 18.5f);
            p.addCentredArc (12, 12, 8, 2.5f, 0, juce::MathConstants<float>::pi * 0.5f, juce::MathConstants<float>::pi * 1.5f, true);
            p.addCentredArc (12, 18.5f, 8, 2.5f, 0, juce::MathConstants<float>::pi * 0.5f, juce::MathConstants<float>::pi * 1.5f, true);
            break;
        case Icon::folderOpen:
            p.startNewSubPath (3, 19); p.lineTo (3, 5.5f); p.lineTo (9.5f, 5.5f); p.lineTo (11.5f, 8); p.lineTo (19, 8); p.lineTo (19, 10.5f);
            p.startNewSubPath (3, 19); p.lineTo (6.5f, 10.5f); p.lineTo (22, 10.5f); p.lineTo (18.5f, 19); p.closeSubPath();
            break;
        case Icon::refresh:
            p.addCentredArc (12, 12, 8, 8, 0, 0.6f, 5.6f, true);
            line (18.5f, 3.5f, 19.4f, 8.5f); line (19.4f, 8.5f, 14.5f, 8.6f);
            break;
        case Icon::lock:
            rr (5, 11, 14, 10, 2);
            p.startNewSubPath (8, 11); p.lineTo (8, 8); p.cubicTo (8, 3, 16, 3, 16, 8); p.lineTo (16, 11);
            break;
        case Icon::unlock:
            rr (5, 11, 14, 10, 2);
            p.startNewSubPath (8, 11); p.lineTo (8, 8); p.cubicTo (8, 3, 16, 3, 16, 7);
            break;
        case Icon::plus:
            line (12, 5, 12, 19); line (5, 12, 19, 12);
            break;
        case Icon::save:
            p.startNewSubPath (4, 4); p.lineTo (17, 4); p.lineTo (20, 7); p.lineTo (20, 20); p.lineTo (4, 20); p.closeSubPath();
            rr (8, 4, 7, 5, 0.5f); rr (7, 13, 10, 7, 0.5f);
            break;
        case Icon::undo:
            p.startNewSubPath (9, 5); p.lineTo (4, 10); p.lineTo (9, 15);
            p.startNewSubPath (4, 10); p.lineTo (14, 10); p.cubicTo (21, 10, 21, 20, 14, 20); p.lineTo (10, 20);
            break;
        case Icon::trash:
            line (4, 7, 20, 7); line (9, 7, 9.5f, 4); line (9.5f, 4, 14.5f, 4); line (14.5f, 4, 15, 7);
            p.startNewSubPath (6, 7); p.lineTo (7, 20); p.lineTo (17, 20); p.lineTo (18, 7);
            break;
        case Icon::play:
            p.startNewSubPath (8, 5); p.lineTo (19, 12); p.lineTo (8, 19); p.closeSubPath();
            break;
        case Icon::check:
            p.startNewSubPath (5, 12.5f); p.lineTo (10, 17.5f); p.lineTo (19.5f, 6.5f);
            break;
    }
    return p;
}
} // namespace

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour, float strokeWidth)
{
    auto path = build (icon);
    auto size = juce::jmin (area.getWidth(), area.getHeight());
    auto box = area.withSizeKeepingCentre (size, size);
    path.applyTransform (juce::AffineTransform::scale (size / 24.0f).translated (box.getX(), box.getY()));
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (strokeWidth * size / 24.0f * 1.15f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

} // namespace pc::ui
