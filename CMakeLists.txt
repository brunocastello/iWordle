cmake_minimum_required(VERSION 3.10)
project(iWordle C)

set(CORE_DIR ${CMAKE_SOURCE_DIR}/src/core)
set(PLATFORM_MACOS9_DIR ${CMAKE_SOURCE_DIR}/src/platform/macos9)

add_application(iWordle
    ${CORE_DIR}/wordle_engine.c
    ${PLATFORM_MACOS9_DIR}/main.c
    ${PLATFORM_MACOS9_DIR}/iWordle.r
    TYPE "APPL"
    CREATOR "WRDL"
)

target_include_directories(iWordle PRIVATE ${CORE_DIR})

# Keep the PowerPC binary as small as possible.
set_target_properties(iWordle PROPERTIES COMPILE_OPTIONS -ffunction-sections)
set_target_properties(iWordle PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
