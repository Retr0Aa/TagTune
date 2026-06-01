cmake_minimum_required(VERSION 3.20)
project(TagTune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(TagLib CONFIG REQUIRED)

add_executable(TagTune
    main.cpp
    src/MetadataService.cpp
    src/MainWindow.cpp
    src/TagEditorWidget.cpp
    src/TrackTableModel.cpp
)

target_include_directories(TagTune PRIVATE src)

target_link_libraries(TagTune PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    TagLib::tag
)

target_compile_definitions(TagTune PRIVATE HAVE_TAGLIB=1)

if (WIN32)
    set(QT_DEPLOY_TOOL "C:/Qt/6.11.1/msvc2022_64/bin/windeployqt.exe")

    add_custom_command(TARGET TagTune POST_BUILD
        COMMAND ${QT_DEPLOY_TOOL} $<TARGET_FILE:TagTune>
        COMMENT "Deploying Qt DLLs with windeployqt"
    )
endif()

if(APPLE)
    set_target_properties(TagTune PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "TagTune"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.retr0a.tagtune"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

install(TARGETS TagTune
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)