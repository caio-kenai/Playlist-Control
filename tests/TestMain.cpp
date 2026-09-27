#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <cstdio>

#include "logging/Logger.h"

// Runs every juce::UnitTest in the executable.
//   playlistcontrol_tests [--category <name>] [--installation <pgm folder>]
// --installation runs the read-only checks against a real installation.
namespace pc::test
{
juce::File installationUnderTest;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ArgumentList args (argc, argv);

    if (args.containsOption ("--installation"))
        pc::test::installationUnderTest = juce::File (args.getValueForOption ("--installation"));

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    if (args.containsOption ("--category"))
        runner.runTestsInCategory (args.getValueForOption ("--category"));
    else
        runner.runAllTests();

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* r = runner.getResult (i);
        failures += r->failures;
        passes += r->passes;
    }
    std::printf ("\n%d checks passed, %d failed.\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
