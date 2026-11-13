# The tests of the engine: every engine_*.cpp of this folder is a program of
# its own, linked with the engine and nothing else.
file(GLOB TUXDAN_ENGINE_TESTS CONFIGURE_DEPENDS engine_*.cpp)
foreach(source ${TUXDAN_ENGINE_TESTS})
    get_filename_component(test ${source} NAME_WE)
    add_executable(${test} ${source})
    target_link_libraries(${test} PRIVATE tuxdan_engine)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# The plugin as TuxAPE loads it: found as a library, started with a host
# of the test's own, its window shown; in English, then in French.
add_executable(plugin_window plugin_window.cpp)
target_include_directories(plugin_window PRIVATE ${TUXAPE_SDK})
target_link_libraries(plugin_window PRIVATE Qt6::Widgets Qt6::Test)
add_dependencies(plugin_window tuxdan)
target_compile_definitions(plugin_window PRIVATE TUXDAN_PLUGIN="$<TARGET_FILE:tuxdan>")
add_test(NAME plugin_window COMMAND plugin_window)
set_tests_properties(plugin_window PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The window, driven as a user drives it.
add_executable(gui_dan gui_dan.cpp)
target_link_libraries(gui_dan PRIVATE tuxdan_ui Qt6::Test)
add_test(NAME gui_dan COMMAND gui_dan)
set_tests_properties(gui_dan PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
