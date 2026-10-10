cmake_minimum_required(VERSION 3.22)

# En Mac: binario universal (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(RemacdentFX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(COMMON_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

# ---------------- Lazo: compresor de bus ----------------
juce_add_plugin(Lazo
    COMPANY_NAME                "Remacdent"
    BUNDLE_ID                   "com.remacdent.lazo"
    PLUGIN_MANUFACTURER_CODE    "Rmcd"
    PLUGIN_CODE                 "Lzo1"
    PRODUCT_NAME                "Lazo"
    FORMATS                     AU VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(Lazo PRIVATE Lazo/LazoProcessor.cpp Lazo/LazoEditor.cpp)
target_compile_definitions(Lazo PUBLIC ${COMMON_DEFS})
target_link_libraries(Lazo PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------- Hueco: espacio espectral por sidechain ----------------
juce_add_plugin(Hueco
    COMPANY_NAME                "Remacdent"
    BUNDLE_ID                   "com.remacdent.hueco"
    PLUGIN_MANUFACTURER_CODE    "Rmcd"
    PLUGIN_CODE                 "Hco1"
    PRODUCT_NAME                "Hueco"
    FORMATS                     AU VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(Hueco PRIVATE Hueco/HuecoProcessor.cpp Hueco/HuecoEditor.cpp)
target_compile_definitions(Hueco PUBLIC ${COMMON_DEFS})
target_link_libraries(Hueco PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------- Pruebas (opcional) ----------------
option(FX_BUILD_TESTS "Compilar pruebas" OFF)
if(FX_BUILD_TESTS)
    juce_add_console_app(FxTest PRODUCT_NAME "FxTest")
    target_sources(FxTest PRIVATE tests/FxTest.cpp
        Lazo/LazoProcessor.cpp Lazo/LazoEditor.cpp Hueco/HuecoProcessor.cpp Hueco/HuecoEditor.cpp)
    target_compile_definitions(FxTest PRIVATE ${COMMON_DEFS} REMACDENT_TEST=1)
    target_link_libraries(FxTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
