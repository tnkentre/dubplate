# Required CMake version
cmake_minimum_required(VERSION 3.15)

# ------------------------------
# Project setting
# ------------------------------
# Define target name
set(TARGET_NAME ${YAVC_PROJECT_NAME})

# Source files
set(SOURCES
  dsptop.c
  FBvsb.c
  tcanalysis.c
  turntable.c
  vsb.c
)

# ------------------------------
# YAVC libraries
# ------------------------------
# Libraries
set(LIBRARIES
  ${YAVC_LIBS}
)

# ------------------------------
# Build static library
# ------------------------------
add_library               (${TARGET_NAME}_LIB STATIC)
set_target_properties     (${TARGET_NAME}_LIB PROPERTIES OUTPUT_NAME ${TARGET_NAME})
target_sources            (${TARGET_NAME}_LIB PRIVATE ${SOURCES})
target_compile_options    (${TARGET_NAME}_LIB PRIVATE ${YAVC_C_FLAGS})
target_link_libraries     (${TARGET_NAME}_LIB PRIVATE ${LIBRARIES})
install(TARGETS ${TARGET_NAME}_LIB)
install(FILES ${YAVC_PATH}/src/template/dsptop.h DESTINATION include)

# ------------------------------
# Build WAVIO app
# ------------------------------
add_executable            (${TARGET_NAME}_WAVIO)
set_target_properties     (${TARGET_NAME}_WAVIO PROPERTIES OUTPUT_NAME ${TARGET_NAME}_wavio)
target_sources            (${TARGET_NAME}_WAVIO PRIVATE ${SOURCES})
target_compile_definitions(${TARGET_NAME}_WAVIO PRIVATE BUILD_WAVIO)
target_compile_options    (${TARGET_NAME}_WAVIO PRIVATE ${YAVC_C_FLAGS})
target_link_libraries     (${TARGET_NAME}_WAVIO PRIVATE ${LIBRARIES})
install(TARGETS ${TARGET_NAME}_WAVIO)

# ------------------------------
# Build JACK app
# ------------------------------
if(WITH_JACK)
add_executable            (${TARGET_NAME}_JACK)
set_target_properties     (${TARGET_NAME}_JACK PROPERTIES OUTPUT_NAME ${TARGET_NAME}_jack)
target_sources            (${TARGET_NAME}_JACK PRIVATE ${SOURCES})
target_compile_definitions(${TARGET_NAME}_JACK PRIVATE BUILD_JACK)
target_compile_options    (${TARGET_NAME}_JACK PRIVATE ${YAVC_C_FLAGS})
target_link_libraries     (${TARGET_NAME}_JACK PRIVATE ${LIBRARIES}
  $<$<BOOL:${WIN32}>:libjack64>
  $<$<NOT:$<BOOL:${WIN32}>>:jack>)
install(TARGETS ${TARGET_NAME}_JACK)
endif(WITH_JACK)

# ------------------------------
# Build Plugins (VST and AU)
# ------------------------------
if(WITH_JUCE)
juce_add_plugin(${TARGET_NAME}_PLUGIN
  # VERSION ...                               # Set this if the plugin version is different to the project version
  # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
  # ICON_SMALL ...
  COMPANY_NAME "Tanaka Enterprises"           # Specify the name of the plugin's author
  # IS_SYNTH TRUE/FALSE                       # Is this a synth or an effect?
  # NEEDS_MIDI_INPUT TRUE/FALSE               # Does the plugin need midi input?
  # NEEDS_MIDI_OUTPUT TRUE/FALSE              # Does the plugin need midi output?
  # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
  # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
  # COPY_PLUGIN_AFTER_BUILD TRUE/FALSE        # Should the plugin be installed to a default location after building?
  PLUGIN_MANUFACTURER_CODE Tnke               # A four-character manufacturer id with at least one upper-case character
  PLUGIN_CODE Dbpt                            # A unique four-character plugin id with exactly one upper-case character
                                              # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
  FORMATS AU VST3 Standalone                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
  PRODUCT_NAME "${TARGET_NAME}")              # The name of the final executable, which can differ from the target name

juce_generate_juce_header (${TARGET_NAME}_PLUGIN)
target_sources            (${TARGET_NAME}_PLUGIN PRIVATE
  ${YAVC_PATH}/src/template/juce_plugin_wrapper.cpp
  ${YAVC_PATH}/src/template/juce_editor_wrapper.cpp
)
target_compile_definitions(${TARGET_NAME}_PLUGIN PUBLIC
  # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
  JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
  JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
  JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(${TARGET_NAME}_PLUGIN 
  PRIVATE 
  ${TARGET_NAME}_LIB
  # AudioPluginData           # If we'd created a binary data target, we'd link to it here
  juce::juce_audio_utils
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)

install(DIRECTORY
  ${CMAKE_BINARY_DIR}/src/${TARGET_NAME}_PLUGIN_artefacts/${CMAKE_BUILD_TYPE}/Standalone
  ${CMAKE_BINARY_DIR}/src/${TARGET_NAME}_PLUGIN_artefacts/${CMAKE_BUILD_TYPE}/VST3
  $<$<BOOL:${APPLE}>:${CMAKE_BINARY_DIR}/src/${TARGET_NAME}_PLUGIN_artefacts/${CMAKE_BUILD_TYPE}/AU>
  DESTINATION bin
  USE_SOURCE_PERMISSIONS
)

endif(WITH_JUCE)
