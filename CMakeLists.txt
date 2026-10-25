cmake_minimum_required(VERSION 3.10)
project(iWordle C)

set(CORE_DIR ${CMAKE_SOURCE_DIR}/src/core)
set(PLATFORM_MACOS9_DIR ${CMAKE_SOURCE_DIR}/src/platform/macos9)
set(PLATFORM_SYSTEM7_DIR ${CMAKE_SOURCE_DIR}/src/platform/system7)

# The plain Retro68 toolchain file (m68k, System 7) sets CMAKE_SYSTEM_NAME to
# "Retro68"; the Carbon PowerPC toolchain used for Mac OS 9 does not.
if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
    add_application(iWordle
        ${CORE_DIR}/wordle_engine.c
        ${CORE_DIR}/wordle_stats.c
        ${PLATFORM_SYSTEM7_DIR}/main.c
        ${PLATFORM_SYSTEM7_DIR}/iWordle.r
        ${PLATFORM_MACOS9_DIR}/icons.r
        TYPE "APPL"
        CREATOR "WRDL"
    )
    message(STATUS "iWordle front end: system7 (68k)")
else()
    add_application(iWordle
        ${CORE_DIR}/wordle_engine.c
        ${CORE_DIR}/wordle_stats.c
        ${PLATFORM_MACOS9_DIR}/main.c
        ${PLATFORM_MACOS9_DIR}/iWordle.r
        ${PLATFORM_MACOS9_DIR}/icons.r
        TYPE "APPL"
        CREATOR "WRDL"
    )
    message(STATUS "iWordle front end: macos9 (Carbon PPC)")
endif()

target_include_directories(iWordle PRIVATE ${CORE_DIR})

# Keep the binary as small as possible.
set_target_properties(iWordle PROPERTIES COMPILE_OPTIONS -ffunction-sections)
set_target_properties(iWordle PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
