cmake_minimum_required(VERSION 3.20)
project(TagTune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Require TagLib via pkg-config so the app writes tags into audio files (no JSON sidecars)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(TAGLIB QUIET taglib)
endif()

# If pkg-config didn't find taglib, try common Homebrew locations and re-run pkg-config there
if(NOT TAGLIB_FOUND)
    if(EXISTS "/opt/homebrew/opt/taglib/lib/pkgconfig/taglib.pc")
        set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/opt/taglib/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        pkg_check_modules(TAGLIB QUIET taglib)
    elseif(EXISTS "/usr/local/opt/taglib/lib/pkgconfig/taglib.pc")
        set(ENV{PKG_CONFIG_PATH} "/usr/local/opt/taglib/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        pkg_check_modules(TAGLIB QUIET taglib)
    endif()
endif()

if(NOT TAGLIB_FOUND)
    message(FATAL_ERROR "TagLib was not found. Install it (e.g. 'brew install taglib') or set PKG_CONFIG_PATH so CMake can find taglib.pc.")
endif()

add_executable(TagTune
    main.cpp
    src/MetadataService.cpp
    src/MainWindow.cpp
    src/TagEditorWidget.cpp
    src/TrackTableModel.cpp
)

if(APPLE)
    set_target_properties(TagTune PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "TagTune"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.retr0a.tagtune"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

target_include_directories(TagTune PRIVATE src)

target_link_libraries(TagTune PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

message(STATUS "TagLib found — enabling in-file tag editing")
target_include_directories(TagTune PRIVATE ${TAGLIB_INCLUDE_DIRS})
target_link_directories(TagTune PRIVATE ${TAGLIB_LIBRARY_DIRS})
target_link_libraries(TagTune PRIVATE ${TAGLIB_LIBRARIES})
target_compile_definitions(TagTune PRIVATE HAVE_TAGLIB=1)

install(TARGETS TagTune
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

