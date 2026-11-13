add_library(tuxdan_engine STATIC
    cartridge.cpp
    cartridge.h
    pack.cpp
    pack.h
    snapshot.cpp
    snapshot.h
    z80code.h
)
target_include_directories(tuxdan_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tuxdan_engine PUBLIC TUXDAN_VERSION="${PROJECT_VERSION}")
