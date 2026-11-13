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

option(TUXDAN_BUILD_TESTS "Build the test programs" ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Snapshots read, blocks packed, the cartridge made and read back, its Z80
# code (z80/cartridge.asm, assembled by tools/assemble.cpp into
# src/engine/z80code.h): plain C++, without Qt nor TuxAPE.
add_subdirectory(src/engine)

if(TUXDAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
