#pragma once

#include <juce_core/juce_core.h>

namespace pc::test
{

inline juce::MemoryBlock bytes (const char* s)
{
    return juce::MemoryBlock (s, std::strlen (s));
}

inline juce::MemoryBlock bytes (std::initializer_list<unsigned char> b)
{
    juce::MemoryBlock m;
    for (auto c : b)
        m.append (&c, 1);
    return m;
}

inline juce::File fixturesDir()
{
    return juce::File (PLAYLISTCONTROL_FIXTURES_DIR);
}

inline juce::MemoryBlock fixture (const juce::String& relative)
{
    juce::MemoryBlock m;
    fixturesDir().getChildFile (relative).loadFileAsData (m);
    return m;
}

// Temporary folder deleted when the test ends.
class TempDir
{
public:
    TempDir()
        : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("PlaylistControlTests")
                   .getNonexistentChildFile ("run", "", false))
    {
        dir.createDirectory();
    }
    ~TempDir() { dir.deleteRecursively(); }

    juce::File dir;
};

// Copy of the installation fixture that tests can modify freely.
inline juce::File copyInstallationFixture (const TempDir& temp)
{
    auto target = temp.dir.getChildFile ("Playlist").getChildFile ("pgm");
    target.createDirectory();
    fixturesDir().getChildFile ("installation").getChildFile ("pgm").copyDirectoryTo (target);
    return target;
}

} // namespace pc::test
