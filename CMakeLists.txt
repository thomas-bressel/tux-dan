# TuxDAN: cartridges for the CPC Dandanator mini, made of snapshots and
# ROMs with a menu to choose them, as a TuxAPE plugin (see PLUGINS.md at
# TuxAPE's root).
cmake_minimum_required(VERSION 3.21)

project(TuxDAN
    VERSION 0.1.0
    DESCRIPTION "Cartridges for the CPC Dandanator mini: a plugin for TuxAPE"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The engine goes into the plugin, a shared library.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Qt, for the whole project: TuxAPE's, or an earlier Qt 6 (checked by
# tuxape_add_plugin).
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# TuxAPE's plugin SDK: plugin.h and TuxAPEPlugin.cmake, all the plugin
# needs of TuxAPE. TuxDAN asks for its version 5: the game being played
# saved as a snapshot, and a cartridge put in TuxAPE's Dandanator.
set(TUXAPE_SDK "${CMAKE_SOURCE_DIR}/../TuxAPE/sdk" CACHE PATH "TuxAPE's sdk folder")
if(NOT EXISTS "${TUXAPE_SDK}/plugin.h" OR NOT EXISTS "${TUXAPE_SDK}/TuxAPEPlugin.cmake")
    message(FATAL_ERROR "TuxAPE's SDK is not in ${TUXAPE_SDK}: set TUXAPE_SDK to TuxAPE's sdk folder.")
endif()
include("${TUXAPE_SDK}/TuxAPEPlugin.cmake")

option(TUXDAN_BUILD_TESTS "Build the test programs" ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Snapshots read, blocks packed, the cartridge made and read back, its Z80
# code (z80/cartridge.asm, assembled by tools/assemble.cpp into
# src/engine/z80code.h): plain C++, without Qt nor TuxAPE.
add_subdirectory(src/engine)
# What TuxAPE loads: the window, and the plugin's class.
add_subdirectory(src/plugin)

# The archive users install: dist/tuxdan-<version>-<platform>.zip, with the
# plugin's translations built into its library; signed when the author's
# secret key is named (TUXAPE_SIGN_KEY).
tuxape_add_plugin(tuxdan
    ID tuxdan
    NAME "TuxDAN – Dandanator Cartridge Maker"
    VERSION ${PROJECT_VERSION}
    AUTHOR "TuxAPE"
    LICENCE "MIT"
    DESCRIPTION "Makes cartridges for the CPC Dandanator mini from snapshots and ROMs, with a menu to choose the game on the CPC; adds the game being played in TuxAPE, and tries the cartridge in TuxAPE's Dandanator."
    API 5
    FILES LICENSE
    TRANSLATIONS translations/tuxdan_en.ts translations/tuxdan_fr.ts
    TRANSLATE_TARGETS tuxdan_ui)

if(TUXDAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
