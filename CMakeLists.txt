cmake_minimum_required(VERSION 3.22)

# Single-config generators (Ninja, NMake) default to Release; multi-config
# generators (Visual Studio) ignore this and use --config. Must be set before
# project(), where MSVC would otherwise pick Debug.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

# Static CRT, so the executable has no dependency on the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(PlaylistControl
    VERSION 0.1.0
    DESCRIPTION "Central de configuração e operação do Playlist Digital"
    HOMEPAGE_URL "https://github.com/caio-kenai/PlaylistControl"
    LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "PlaylistControl targets Windows, where the Playlist Digital runs.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# JUCE (git submodule pinned to tag 9.0.2)
# ---------------------------------------------------------------------------
set(PLAYLISTCONTROL_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH
    "JUCE checkout to build against (defaults to the pinned submodule)")
if(NOT EXISTS "${PLAYLISTCONTROL_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE was not found in ${PLAYLISTCONTROL_JUCE_DIR}.\n"
        "Run:  git submodule update --init --recursive\n"
        "or pass -DPLAYLISTCONTROL_JUCE_DIR=<path to a JUCE 9 checkout>.")
endif()
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "" FORCE)
add_subdirectory("${PLAYLISTCONTROL_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

set(PLAYLISTCONTROL_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(src/app/Version.h.in "${PLAYLISTCONTROL_GENERATED_DIR}/Version.h" @ONLY)

# Every executable lands in <build>/bin; $<1:...> stops multi-config
# generators from appending a per-configuration folder.
set(PLAYLISTCONTROL_BIN_DIR "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------------------
# playlistcontrol_juce: JUCE modules compiled once and shared by the app and
# the tests.
# ---------------------------------------------------------------------------
add_library(playlistcontrol_juce STATIC)
target_link_libraries(playlistcontrol_juce
    PRIVATE
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_cryptography
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags)
target_compile_definitions(playlistcontrol_juce
    PUBLIC
        JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_MODAL_LOOPS_PERMITTED=0)
target_include_directories(playlistcontrol_juce
    INTERFACE $<TARGET_PROPERTY:playlistcontrol_juce,INCLUDE_DIRECTORIES>)
target_compile_definitions(playlistcontrol_juce
    INTERFACE $<TARGET_PROPERTY:playlistcontrol_juce,COMPILE_DEFINITIONS>)
set_target_properties(playlistcontrol_juce PROPERTIES FOLDER "JUCE")

add_library(playlistcontrol_options INTERFACE)
target_compile_definitions(playlistcontrol_options INTERFACE
    _WIN32_WINNT=0x0A00 WINVER=0x0A00 UNICODE _UNICODE)
if(MSVC)
    target_compile_options(playlistcontrol_options INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/W4 /utf-8 /permissive- /Zc:__cplusplus /EHsc /MP>)
endif()

# ---------------------------------------------------------------------------
# playlistcontrol_core: formats, validation, storage and services (no UI).
# ---------------------------------------------------------------------------
include(cmake/Sources.cmake)

add_library(playlistcontrol_core STATIC ${PLAYLISTCONTROL_CORE_SOURCES})
target_include_directories(playlistcontrol_core PUBLIC src "${PLAYLISTCONTROL_GENERATED_DIR}")
target_link_libraries(playlistcontrol_core
    PUBLIC playlistcontrol_juce
    PRIVATE playlistcontrol_options)
target_link_libraries(playlistcontrol_core PUBLIC version shell32 ole32 advapi32)
set_target_properties(playlistcontrol_core PROPERTIES FOLDER "PlaylistControl")

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(PlaylistControl WIN32 ${PLAYLISTCONTROL_APP_SOURCES} resources/PlaylistControl.rc)
target_link_libraries(PlaylistControl PRIVATE playlistcontrol_core playlistcontrol_options)
set_target_properties(PlaylistControl PROPERTIES
    FOLDER "PlaylistControl"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PLAYLISTCONTROL_BIN_DIR}>")

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(playlistcontrol_tests ${PLAYLISTCONTROL_TEST_SOURCES})
target_link_libraries(playlistcontrol_tests PRIVATE playlistcontrol_core playlistcontrol_options)
target_compile_definitions(playlistcontrol_tests PRIVATE
    PLAYLISTCONTROL_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
set_target_properties(playlistcontrol_tests PROPERTIES
    FOLDER "PlaylistControl"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PLAYLISTCONTROL_BIN_DIR}>")
add_test(NAME unit COMMAND playlistcontrol_tests)

if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT PlaylistControl)
endif()
