# =============================================================================
#  NUBIS - Granular Texture Engine (VST3)  -  by 21bayyona
# =============================================================================
cmake_minimum_required(VERSION 3.22)

project(NUBIS VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the .vst3 runs on any Windows 10/11 x64 machine without
# installing the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(NUBIS_BUILD_INSTRUMENT  "Also build the instrument flavour (Nubis Instrument)" ON)
option(NUBIS_BUILD_TESTS       "Build the standalone DSP test executable" ON)
option(NUBIS_COPY_AFTER_BUILD  "Copy the VST3 into the system VST3 folder after building (needs admin rights on Windows)" OFF)
if(WIN32)
    option(NUBIS_BUILD_STANDALONE "Also build a standalone .exe for quick testing" ON)
else()
    option(NUBIS_BUILD_STANDALONE "Also build a standalone app for quick testing" OFF)
endif()
option(NUBIS_LINUX_MINIMAL_X11 "Linux CI builds: do not require Xrandr/Xinerama/Xcursor headers" OFF)
set(NUBIS_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

# -----------------------------------------------------------------------------
# JUCE: local path > ./JUCE sub-folder > download (JUCE 8.0.4)
# -----------------------------------------------------------------------------
if(NUBIS_JUCE_DIR AND EXISTS "${NUBIS_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${NUBIS_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# -----------------------------------------------------------------------------
# Embedded resources (fonts, SIL OFL)
# -----------------------------------------------------------------------------
juce_add_binary_data(NubisData
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES
        Resources/Fonts/Outfit-Regular.ttf
        Resources/Fonts/Outfit-Bold.ttf
        Resources/Fonts/GeistMono-Regular.ttf)
set_target_properties(NubisData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(NUBIS_DSP_SOURCES
    Source/dsp/DspUtils.h
    Source/dsp/Grain.h            Source/dsp/Grain.cpp
    Source/dsp/SpectralBlur.h     Source/dsp/SpectralBlur.cpp
    Source/dsp/GranularEngine.h   Source/dsp/GranularEngine.cpp
    Source/dsp/ModulationEngine.h Source/dsp/ModulationEngine.cpp
    Source/dsp/EffectsEngine.h    Source/dsp/EffectsEngine.cpp)

set(NUBIS_PLUGIN_SOURCES
    Source/Parameters.h           Source/Parameters.cpp
    Source/PresetManager.h        Source/PresetManager.cpp
    Source/FactoryPresets.cpp
    Source/PluginProcessor.h      Source/PluginProcessor.cpp
    Source/PluginEditor.h         Source/PluginEditor.cpp
    Source/ui/Theme.h             Source/ui/Theme.cpp
    Source/ui/NubisLookAndFeel.h  Source/ui/NubisLookAndFeel.cpp
    Source/ui/Widgets.h           Source/ui/Widgets.cpp
    Source/ui/GrainVisualizer.h   Source/ui/GrainVisualizer.cpp
    Source/ui/ModulationPanel.h   Source/ui/ModulationPanel.cpp
    Source/ui/EffectsPanel.h      Source/ui/EffectsPanel.cpp)

# -----------------------------------------------------------------------------
function(nubis_add_plugin target productName pluginCode isInstrument)
    set(formats VST3)
    if(NUBIS_BUILD_STANDALONE)
        list(APPEND formats Standalone)
    endif()

    if(isInstrument)
        set(isSynth TRUE)
        set(categories Instrument Synth)
        set(bundle com.bayyona21.nubis.instrument)
    else()
        set(isSynth FALSE)
        set(categories Fx Modulation)
        set(bundle com.bayyona21.nubis.fx)
    endif()

    juce_add_plugin(${target}
        COMPANY_NAME             "21bayyona"
        BUNDLE_ID                ${bundle}
        PLUGIN_MANUFACTURER_CODE Bayy
        PLUGIN_CODE              ${pluginCode}
        PRODUCT_NAME             "${productName}"
        FORMATS                  ${formats}
        IS_SYNTH                 ${isSynth}
        NEEDS_MIDI_INPUT         TRUE
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES          ${categories}
        COPY_PLUGIN_AFTER_BUILD  ${NUBIS_COPY_AFTER_BUILD})

    target_sources(${target} PRIVATE ${NUBIS_DSP_SOURCES} ${NUBIS_PLUGIN_SOURCES})

    target_compile_definitions(${target} PUBLIC
        NUBIS_INSTRUMENT=$<BOOL:${isInstrument}>
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=0)

    if(NUBIS_LINUX_MINIMAL_X11)
        target_compile_definitions(${target} PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
    endif()

    set(modules juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics juce::juce_dsp)
    if(NUBIS_BUILD_STANDALONE)
        list(APPEND modules juce::juce_audio_utils)
    endif()

    target_link_libraries(${target}
        PRIVATE NubisData ${modules}
        PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

nubis_add_plugin(Nubis "Nubis" Nbfx FALSE)
if(NUBIS_BUILD_INSTRUMENT)
    nubis_add_plugin(NubisInstrument "Nubis Instrument" Nbin TRUE)
endif()

# -----------------------------------------------------------------------------
# DSP unit tests (no JUCE needed)
# -----------------------------------------------------------------------------
if(NUBIS_BUILD_TESTS)
    add_executable(NubisDspTests Tests/DspTests.cpp ${NUBIS_DSP_SOURCES})
    target_compile_features(NubisDspTests PRIVATE cxx_std_17)
    enable_testing()
    add_test(NAME NubisDspTests COMMAND NubisDspTests)
endif()

# -----------------------------------------------------------------------------
# Host-level tests (optional): load the built .vst3 like a DAW, and render the
# editor to a PNG with live audio.
# -----------------------------------------------------------------------------
option(NUBIS_BUILD_HOST_TESTS "Build the VST3 host stress test and UI snapshot tools" OFF)
if(NUBIS_BUILD_HOST_TESTS)
    set(nubis_tool_defs JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(NUBIS_LINUX_MINIMAL_X11)
        list(APPEND nubis_tool_defs JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
    endif()

    juce_add_console_app(NubisHostTest PRODUCT_NAME "NubisHostTest")
    target_sources(NubisHostTest PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(NubisHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 ${nubis_tool_defs})
    target_link_libraries(NubisHostTest PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)

    juce_add_console_app(NubisUiSnapshot PRODUCT_NAME "NubisUiSnapshot")
    target_sources(NubisUiSnapshot PRIVATE Tests/UiSnapshot.cpp ${NUBIS_DSP_SOURCES} ${NUBIS_PLUGIN_SOURCES})
    target_compile_definitions(NubisUiSnapshot PRIVATE ${nubis_tool_defs} NUBIS_INSTRUMENT=0 JUCE_MODAL_LOOPS_PERMITTED=1
        "JucePlugin_Name=\"Nubis\"" "JucePlugin_VersionString=\"${PROJECT_VERSION}\"")
    target_link_libraries(NubisUiSnapshot PRIVATE NubisData juce::juce_audio_processors juce::juce_audio_formats
        juce::juce_gui_basics juce::juce_dsp juce::juce_recommended_config_flags)
endif()
