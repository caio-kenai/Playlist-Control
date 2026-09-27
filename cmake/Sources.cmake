# Source lists, kept explicit so the Visual Studio solution mirrors the tree.

set(PLAYLISTCONTROL_CORE_SOURCES
    src/platform/WinInclude.h

    src/core/Diagnostic.h
    src/core/Diagnostic.cpp
    src/core/TextCodec.h
    src/core/TextCodec.cpp
    src/core/TimeOfDay.h
    src/core/TimeOfDay.cpp

    src/logging/Logger.h
    src/logging/Logger.cpp

    src/storage/FileIO.h
    src/storage/FileIO.cpp
    src/storage/HistoryStore.h
    src/storage/HistoryStore.cpp
    src/storage/SafeWriter.h
    src/storage/SafeWriter.cpp
)

set(PLAYLISTCONTROL_APP_SOURCES
    src/app/Main.cpp
)

set(PLAYLISTCONTROL_TEST_SOURCES
    tests/TestMain.cpp
    tests/TestUtils.h
    tests/TextCodecTests.cpp
    tests/StorageTests.cpp
)
