cmake_minimum_required(VERSION 3.16)
project(AnotherSimpleEditor LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base stc)
include(${wxWidgets_USE_FILE})

add_executable(another-simple-editor
    src/main.cpp
    src/editor_frame.cpp
    src/editor_behavior.cpp
    src/editor_file.cpp
    src/editor_find.cpp
    src/editor_settings.cpp)
target_link_libraries(another-simple-editor PRIVATE ${wxWidgets_LIBRARIES})

if(UNIX AND NOT APPLE AND "${wxWidgets_DEFINITIONS}" MATCHES "__WXGTK__")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK_FONT_RENDERING REQUIRED IMPORTED_TARGET
                      fontconfig gtk+-3.0 pangocairo pangoft2>=1.48)
    target_link_libraries(another-simple-editor PRIVATE PkgConfig::GTK_FONT_RENDERING)
    if(BUILD_TESTING)
        add_executable(pixel-font-rendering-test tests/pixel_font_rendering_test.cpp)
        target_link_libraries(pixel-font-rendering-test PRIVATE ${wxWidgets_LIBRARIES} PkgConfig::GTK_FONT_RENDERING)
        add_test(NAME pixel-font-rendering COMMAND pixel-font-rendering-test)
        set_tests_properties(pixel-font-rendering PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()

# Translation catalogs are optional at build time. The application falls back
# to its source strings when no catalog exists for the active system locale.
find_program(MSGFMT_EXECUTABLE msgfmt)
if(MSGFMT_EXECUTABLE)
    file(GLOB TRANSLATION_FILES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/locale/*.po")
    set(COMPILED_TRANSLATIONS)
    foreach(translation IN LISTS TRANSLATION_FILES)
        get_filename_component(language ${translation} NAME_WE)
        set(catalog_dir "${CMAKE_CURRENT_BINARY_DIR}/locale/${language}/LC_MESSAGES")
        set(catalog "${catalog_dir}/another-simple-editor.mo")
        add_custom_command(
            OUTPUT "${catalog}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${catalog_dir}"
            COMMAND ${MSGFMT_EXECUTABLE} --check -o "${catalog}" "${translation}"
            DEPENDS "${translation}"
            VERBATIM)
        list(APPEND COMPILED_TRANSLATIONS "${catalog}")
        install(FILES "${catalog}"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/locale/${language}/LC_MESSAGES")
    endforeach()
    if(COMPILED_TRANSLATIONS)
        add_custom_target(another-simple-editor-translations ALL DEPENDS ${COMPILED_TRANSLATIONS})
    endif()
else()
    message(STATUS "msgfmt not found; building without translation catalogs")
endif()

install(TARGETS another-simple-editor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
