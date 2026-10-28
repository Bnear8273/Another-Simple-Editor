cmake_minimum_required(VERSION 3.16)
project(SingleCodeEditor LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(singlecode-editor src/main.cpp)
target_link_libraries(singlecode-editor PRIVATE ${wxWidgets_LIBRARIES})

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
        set(catalog "${catalog_dir}/singlecode.mo")
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
        add_custom_target(singlecode-translations ALL DEPENDS ${COMPILED_TRANSLATIONS})
    endif()
else()
    message(STATUS "msgfmt not found; building without translation catalogs")
endif()

install(TARGETS singlecode-editor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
