cmake_minimum_required(VERSION 3.22)

project(Grotto VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# ---------------------------------------------------------------------------
# JUCE: use a local copy if you have one (-DGROTTO_JUCE_PATH=...),
# otherwise download JUCE 8 automatically.
# ---------------------------------------------------------------------------
set(GROTTO_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
option(GROTTO_BUILD_PLUGIN "Build the plugin (needs JUCE)" ON)
option(GROTTO_BUILD_TESTS "Build the offline tests" ON)

# The DSP engine: plain C++17, no JUCE, builds anywhere (Windows x64, Linux ARM64, macOS).
add_library(grotto_engine INTERFACE)
target_include_directories(grotto_engine INTERFACE Source/engine)

if(GROTTO_BUILD_TESTS)
    add_executable(grotto_render_test Tests/RenderTest.cpp)
    target_link_libraries(grotto_render_test PRIVATE grotto_engine)
endif()

if(GROTTO_BUILD_PLUGIN)
    if(GROTTO_JUCE_PATH)
        add_subdirectory(${GROTTO_JUCE_PATH} JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.4
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(GROTTO_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND GROTTO_FORMATS AU)
    endif()

    juce_add_plugin(Grotto
        COMPANY_NAME "Kevin Kaye"
        COMPANY_WEBSITE ""
        BUNDLE_ID "uk.kevinkaye.grotto"
        PLUGIN_MANUFACTURER_CODE Kkay
        PLUGIN_CODE Grto
        FORMATS ${GROTTO_FORMATS}
        PRODUCT_NAME "Grotto"
        DESCRIPTION "Cavernous reverb made of a swarm of short delays"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Reverb)

    juce_add_binary_data(GrottoData SOURCES Resources/RobotoCondensed-Bold.ttf)

    set(GROTTO_SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PedalControl.cpp
        Source/Keeper.cpp)

    target_sources(Grotto PRIVATE ${GROTTO_SOURCES})
    target_include_directories(Grotto PRIVATE Source)

    set(GROTTO_DEFS
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_compile_definitions(Grotto PUBLIC ${GROTTO_DEFS})

    target_link_libraries(Grotto
        PRIVATE
            GrottoData
            grotto_engine
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Footswitch test: the real processor, driven by plain MIDI press and release messages.
    if(GROTTO_BUILD_TESTS)
        juce_add_console_app(grotto_foot_test PRODUCT_NAME "grotto_foot_test")
        target_sources(grotto_foot_test PRIVATE Tests/FootTest.cpp ${GROTTO_SOURCES})
        target_include_directories(grotto_foot_test PRIVATE Source)
        target_compile_definitions(grotto_foot_test PRIVATE ${GROTTO_DEFS} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(grotto_foot_test
            PRIVATE
                GrottoData
                grotto_engine
                juce::juce_audio_utils
                juce::juce_dsp
            PUBLIC
                juce::juce_recommended_config_flags)
    endif()
endif()
