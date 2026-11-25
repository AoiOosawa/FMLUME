cmake_minimum_required(VERSION 3.25)

# Target platform (docs/DECISIONS.md D-049, same policy as POLYBRISK). Must be set before project().
# Development builds are arm64 only (fast). Distribution builds are Universal (arm64 + x86_64):
# scripts/package.sh passes -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64".
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architectures")
endif()

project(FMLUME VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# Windows (MSVC): ソースは UTF-8 (日本語のコメントを含む)。この指定がないと、Windows が文字を読み違える。
# NOMINMAX: Windows の min / max と std::min / std::max がぶつかるのを防ぐ。
# _USE_MATH_DEFINES: M_PI などの数学の定数を使えるようにする。
# JUCE を add_subdirectory する前に設定する (JUCE 側のターゲットにも効かせるため)。
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)
endif()

option(FMLUME_BUILD_PLUGIN "Build the AU/VST3 plug-in (requires external/JUCE)" ON)
option(FMLUME_BUILD_TESTS  "Build unit tests and tools" ON)
option(FMLUME_COPY_PLUGIN  "Copy plug-ins to ~/Library/Audio/Plug-Ins after build" ON)
# Development builds are a separate plug-in ("FMLUME Dev") with their own identifiers, so they sit
# next to an installed release in the DAW and can never be mistaken for it or overwrite it.
# Projects saved with the Dev plug-in do not open with the release one (different ids): the Dev
# build is for testing. Release builds: -DFMLUME_DEV_BUILD=OFF. See docs/DECISIONS.md D-047.
option(FMLUME_DEV_BUILD "Build the development plug-in 'FMLUME Dev' (separate ids) instead of the release 'FMLUME'" ON)
option(FMLUME_BUILD_UI_SNAPSHOT "Build the off-screen editor snapshot tool (development only)" OFF)

# ---------------------------------------------------------------- FMLUMECore
# Host-independent DSP / model / preset library. No JUCE dependency.
file(GLOB_RECURSE FMLUME_CORE_SOURCES CONFIGURE_DEPENDS
    Source/Core/*.cpp Source/Core/*.h
    Source/DSP/*.cpp Source/DSP/*.h
    Source/Exploration/*.cpp Source/Exploration/*.h
    Source/Presets/*.cpp Source/Presets/*.h)
add_library(FMLUMECore STATIC ${FMLUME_CORE_SOURCES})
target_include_directories(FMLUMECore PUBLIC Source)
target_compile_definitions(FMLUMECore PUBLIC FMLUME_VERSION_STRING="${PROJECT_VERSION}")
if(MSVC)
    # MSVC には -Wextra / -O3 などの指定がない。Windows では /W4 を使う (最適化は Release の既定で入る)。
    target_compile_options(FMLUMECore PRIVATE /W4)
else()
    target_compile_options(FMLUMECore PRIVATE -Wall -Wextra -Wno-unused-parameter)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(FMLUMECore PRIVATE $<$<NOT:$<CONFIG:Debug>>:-O3>)
    endif()
endif()

# ---------------------------------------------------------------- Plug-in
if(FMLUME_BUILD_PLUGIN)
    add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)

    file(GLOB FMLUME_FACTORY_PRESETS CONFIGURE_DEPENDS Resources/Presets/*.fmlume)
    if(FMLUME_FACTORY_PRESETS)
        juce_add_binary_data(FMLUMEFactoryData
            HEADER_NAME FactoryData.h NAMESPACE FactoryData
            SOURCES ${FMLUME_FACTORY_PRESETS})
    endif()

    if(FMLUME_DEV_BUILD)
        set(FMLUME_PRODUCT_NAME "FMLUME Dev")
        set(FMLUME_PLUGIN_CODE Fmld)
        set(FMLUME_BUNDLE_ID info.cheebow.fmlume.dev)
    else()
        set(FMLUME_PRODUCT_NAME "FMLUME")
        set(FMLUME_PLUGIN_CODE Fmlu)
        set(FMLUME_BUNDLE_ID info.cheebow.fmlume)
    endif()
    message(STATUS "FMLUME plug-in: ${FMLUME_PRODUCT_NAME} (code ${FMLUME_PLUGIN_CODE}, ${FMLUME_BUNDLE_ID})")

    juce_add_plugin(FMLUME
        # Company, manufacturer code and bundle-id prefix are shared with POLYBRISK (same maker).
        COMPANY_NAME "CHEEBOW"
        BUNDLE_ID ${FMLUME_BUNDLE_ID}
        PLUGIN_MANUFACTURER_CODE Chbw
        PLUGIN_CODE ${FMLUME_PLUGIN_CODE}
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "${FMLUME_PRODUCT_NAME}"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        VST3_CATEGORIES Instrument Synth
        NEEDS_WEB_BROWSER FALSE
        NEEDS_CURL FALSE
        ICON_BIG "${CMAKE_SOURCE_DIR}/assets/branding/icon-1024.png"
        COPY_PLUGIN_AFTER_BUILD ${FMLUME_COPY_PLUGIN})

    file(GLOB_RECURSE FMLUME_PLUGIN_SOURCES CONFIGURE_DEPENDS
        Source/Plugin/*.cpp Source/Plugin/*.h Source/UI/*.cpp Source/UI/*.h)
    target_sources(FMLUME PRIVATE ${FMLUME_PLUGIN_SOURCES})
    if(FMLUME_DEV_BUILD)
        target_compile_definitions(FMLUME PRIVATE FMLUME_DEV_BUILD=1)
    endif()
    target_compile_definitions(FMLUME PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(FMLUME
        PRIVATE FMLUMECore juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
    if(TARGET FMLUMEFactoryData)
        target_link_libraries(FMLUME PRIVATE FMLUMEFactoryData)
        target_compile_definitions(FMLUME PRIVATE FMLUME_HAS_FACTORY_DATA=1)
    endif()

    if(FMLUME_BUILD_UI_SNAPSHOT)
        juce_add_gui_app(fmlume-uisnap PRODUCT_NAME "fmlume-uisnap")
        target_sources(fmlume-uisnap PRIVATE tools/UiSnapshot/main.cpp ${FMLUME_PLUGIN_SOURCES})
        target_compile_definitions(fmlume-uisnap PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
            FMLUME_HAS_FACTORY_DATA=1)
        target_link_libraries(fmlume-uisnap PRIVATE FMLUMECore FMLUMEFactoryData juce::juce_audio_utils
            PUBLIC juce::juce_recommended_config_flags)
    endif()
endif()

# ---------------------------------------------------------------- Tests / tools
if(FMLUME_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tools)
    add_subdirectory(tests)
endif()
