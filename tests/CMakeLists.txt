# The tests of the engine: every engine_*.cpp of this folder is a program of
# its own, linked with the engine and nothing else.
file(GLOB TUXDAN_ENGINE_TESTS CONFIGURE_DEPENDS engine_*.cpp)
foreach(source ${TUXDAN_ENGINE_TESTS})
    get_filename_component(test ${source} NAME_WE)
    add_executable(${test} ${source})
    target_link_libraries(${test} PRIVATE tuxdan_engine)
    add_test(NAME ${test} COMMAND ${test})
endforeach()
