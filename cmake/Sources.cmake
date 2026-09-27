# Source lists, kept explicit so the Visual Studio solution mirrors the tree.

set(PLAYLISTCONTROL_CORE_SOURCES
    src/platform/WinInclude.h
    src/platform/WindowsSystem.h
    src/platform/WindowsSystem.cpp

    src/core/Diagnostic.h
    src/core/Diagnostic.cpp
    src/core/TextCodec.h
    src/core/TextCodec.cpp
    src/core/TimeOfDay.h
    src/core/TimeOfDay.cpp
    src/core/LineDiff.h
    src/core/LineDiff.cpp

    src/logging/Logger.h
    src/logging/Logger.cpp

    src/storage/FileIO.h
    src/storage/FileIO.cpp
    src/storage/HistoryStore.h
    src/storage/HistoryStore.cpp
    src/storage/SafeWriter.h
    src/storage/SafeWriter.cpp

    src/formats/ini/IniDocument.h
    src/formats/ini/IniDocument.cpp
    src/formats/playlistini/PlaylistIni.h
    src/formats/playlistini/PlaylistIni.cpp
    src/formats/playlistini/FilePatternResolver.h
    src/formats/playlistini/FilePatternResolver.cpp
    src/formats/schedule/ScheduleDocument.h
    src/formats/schedule/ScheduleDocument.cpp
    src/formats/xml/XmlPatchDocument.h
    src/formats/xml/XmlPatchDocument.cpp
    src/formats/configxml/ConfigSchema.h
    src/formats/configxml/ConfigSchema.cpp
    src/formats/configxml/ConfigXml.h
    src/formats/configxml/ConfigXml.cpp
    src/formats/folders/FoldersXml.h
    src/formats/folders/FoldersXml.cpp
    src/formats/operators/OperatorProfile.h
    src/formats/operators/OperatorProfile.cpp
    src/formats/dbf/DbfTable.h
    src/formats/dbf/DbfTable.cpp
    src/formats/ntx/NtxHeader.h
    src/formats/ntx/NtxHeader.cpp
    src/formats/montagem/MontagemFile.h
    src/formats/montagem/MontagemFile.cpp

    src/install/Installation.h
    src/install/Installation.cpp
    src/ecosystem/Ecosystem.h
    src/ecosystem/Ecosystem.cpp
    src/ecosystem/Origin.h
    src/ecosystem/Origin.cpp
    src/catalog/CodeCatalog.h
    src/catalog/CodeCatalog.cpp
    src/validation/Validators.h
    src/validation/Validators.cpp

    src/services/AppSettings.h
    src/services/AppSettings.cpp
    src/services/FileSession.h
    src/services/FileSession.cpp
    src/services/DirectoryWatcher.h
    src/services/DirectoryWatcher.cpp
    src/services/Workspace.h
    src/services/Workspace.cpp
)

set(PLAYLISTCONTROL_APP_SOURCES
    src/app/Main.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/Widgets.h
    src/ui/Widgets.cpp
    src/ui/View.h
    src/ui/MainComponent.h
    src/ui/MainComponent.cpp
    src/ui/views/ViewFactory.h
    src/ui/views/ViewFactory.cpp
    src/ui/views/DashboardView.h
    src/ui/views/DashboardView.cpp
    src/ui/views/ScheduleView.h
    src/ui/views/ScheduleView.cpp
)

set(PLAYLISTCONTROL_TEST_SOURCES
    tests/TestMain.cpp
    tests/TestUtils.h
    tests/TextCodecTests.cpp
    tests/StorageTests.cpp
    tests/IniTests.cpp
    tests/ScheduleTests.cpp
    tests/XmlFormatTests.cpp
    tests/DataFormatTests.cpp
    tests/ValidationTests.cpp
    tests/RealInstallationTests.cpp
    tests/ServiceTests.cpp
)
