#include "ui/ShellIcons.h"
#include "platform/WinInclude.h"

#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

namespace pc::ui
{

namespace
{
// Draws the icon over black and over white and derives the alpha from the
// difference, which works for every kind of icon (masked or 32-bit).
juce::Image toImage (HICON icon, int size)
{
    BITMAPINFO bmi {};
    bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    auto dc = CreateCompatibleDC (nullptr);
    auto render = [&] (juce::uint32 background, std::vector<juce::uint32>& out) {
        void* bits = nullptr;
        auto dib = CreateDIBSection (dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        auto old = SelectObject (dc, dib);
        auto* px = static_cast<juce::uint32*> (bits);
        std::fill (px, px + size * size, background);
        DrawIconEx (dc, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL);
        GdiFlush();
        out.assign (px, px + size * size);
        SelectObject (dc, old);
        DeleteObject (dib);
    };
    std::vector<juce::uint32> black, white;
    render (0x00000000, black);
    render (0x00ffffff, white);
    DeleteDC (dc);

    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            auto b = black[(size_t) (y * size + x)];
            auto w = white[(size_t) (y * size + x)];
            auto channel = [] (juce::uint32 v, int shift) { return (int) ((v >> shift) & 0xff); };
            int diff = ((channel (w, 16) - channel (b, 16)) + (channel (w, 8) - channel (b, 8)) + (channel (w, 0) - channel (b, 0))) / 3;
            auto alpha = (juce::uint8) juce::jlimit (0, 255, 255 - diff);
            if (alpha == 0)
            {
                data.setPixelColour (x, y, juce::Colours::transparentBlack);
                continue;
            }
            auto un = [alpha] (int c) { return (juce::uint8) juce::jlimit (0, 255, c * 255 / alpha); };
            data.setPixelColour (x, y, juce::Colour (un (channel (b, 16)), un (channel (b, 8)), un (channel (b, 0)), alpha));
        }
    }
    return image;
}
} // namespace

ShellIcons& ShellIcons::instance()
{
    static ShellIcons icons;
    return icons;
}

juce::Image ShellIcons::get (const juce::String& file, int index, int size)
{
    auto key = file.toLowerCase() + "|" + juce::String (index) + "|" + juce::String (size);
    if (auto it = cache_.find (key); it != cache_.end())
        return it->second;
    juce::Image image;
    HICON icon = nullptr;
    UINT id = 0;
    if (file.isNotEmpty() && juce::File (file).existsAsFile()
        && PrivateExtractIconsW (file.toWideCharPointer(), index, size, size, &icon, &id, 1, 0) == 1 && icon != nullptr)
    {
        image = toImage (icon, size);
        DestroyIcon (icon);
    }
    cache_[key] = image;
    return image;
}

int ShellIcons::count (const juce::String& file)
{
    auto key = file.toLowerCase();
    if (auto it = counts_.find (key); it != counts_.end())
        return it->second;
    auto n = juce::File (file).existsAsFile() ? (int) ExtractIconExW (file.toWideCharPointer(), -1, nullptr, nullptr, 0) : 0;
    counts_[key] = n;
    return n;
}

} // namespace pc::ui
