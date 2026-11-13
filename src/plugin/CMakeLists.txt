set(CMAKE_AUTOMOC ON)

# The window is a library, so that tests can drive it; the plugin itself
# is no more than what TuxAPE loads and talks to.
add_library(tuxdan_ui STATIC
    contrast.cpp
    contrast.h
    danwindow.cpp
    danwindow.h
    icons.cpp
    icons.h
)
target_include_directories(tuxdan_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TUXAPE_SDK})
target_link_libraries(tuxdan_ui PUBLIC tuxdan_engine Qt6::Widgets)

# What TuxAPE loads: the plugin's class, and no more. tuxape_add_plugin()
# names it after the plugin's id.
add_library(tuxdan MODULE
    tuxdanplugin.cpp
    tuxdanplugin.h
)
target_link_libraries(tuxdan PRIVATE tuxdan_ui)
