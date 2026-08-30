cmake_minimum_required(VERSION 3.28)

project(boulderdash VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(BOULDERDASH_DEPLOY_QT_RUNTIME
    "Deploy the Qt runtime next to boulderdash after a Windows build"
    ON
)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(QT_ROOT "" CACHE PATH "Path to a Qt 6 kit (for example C:/Qt/6.11.1/msvc2022_64)")

# Keep command-line builds convenient on the reference Windows workstation while
# allowing CI and other developers to select Qt through QT_ROOT or CMAKE_PREFIX_PATH.
if(WIN32 AND NOT QT_ROOT)
    file(GLOB _qt_candidate_roots LIST_DIRECTORIES true "C:/Qt/6.*/msvc2022_64")
    if(_qt_candidate_roots)
        list(SORT _qt_candidate_roots COMPARE NATURAL ORDER DESCENDING)
        list(GET _qt_candidate_roots 0 QT_ROOT)
        message(STATUS "Qt 6 kit detected at '${QT_ROOT}'")
    endif()
endif()

if(QT_ROOT)
    if(NOT EXISTS "${QT_ROOT}")
        message(FATAL_ERROR "QT_ROOT does not exist: '${QT_ROOT}'")
    endif()

    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Multimedia Widgets)

set(BOULDERDASH_ENGINE_SOURCES
    "${PROJECT_ROOT}/src/engine/CaveDefinition.cpp"
    "${PROJECT_ROOT}/src/engine/CaveGrid.cpp"
    "${PROJECT_ROOT}/src/engine/CavePreparation.cpp"
    "${PROJECT_ROOT}/src/engine/CaveScanner.cpp"
    "${PROJECT_ROOT}/src/engine/AmoebaRules.cpp"
    "${PROJECT_ROOT}/src/engine/FallingObjectRules.cpp"
    "${PROJECT_ROOT}/src/engine/EnemyRules.cpp"
    "${PROJECT_ROOT}/src/engine/GameSession.cpp"
    "${PROJECT_ROOT}/src/engine/MagicWallRules.cpp"
    "${PROJECT_ROOT}/src/engine/PseudoRandom.cpp"
    "${PROJECT_ROOT}/src/engine/ProgressionRules.cpp"
    "${PROJECT_ROOT}/src/engine/RockfordRules.cpp"
    "${PROJECT_ROOT}/src/engine/SnapshotText.cpp"
    "${PROJECT_ROOT}/src/engine/TimeBasedRandom.cpp"
)

set(BOULDERDASH_ENGINE_HEADERS
    "${PROJECT_ROOT}/src/engine/CaveDefinition.h"
    "${PROJECT_ROOT}/src/engine/CaveGrid.h"
    "${PROJECT_ROOT}/src/engine/CavePreparation.h"
    "${PROJECT_ROOT}/src/engine/CaveScanner.h"
    "${PROJECT_ROOT}/src/engine/AmoebaRules.h"
    "${PROJECT_ROOT}/src/engine/CaveUpdateContext.h"
    "${PROJECT_ROOT}/src/engine/FallingObjectRules.h"
    "${PROJECT_ROOT}/src/engine/EnemyRules.h"
    "${PROJECT_ROOT}/src/engine/EngineTiming.h"
    "${PROJECT_ROOT}/src/engine/EngineTypes.h"
    "${PROJECT_ROOT}/src/engine/GameSession.h"
    "${PROJECT_ROOT}/src/engine/GameState.h"
    "${PROJECT_ROOT}/src/engine/MagicWallRules.h"
    "${PROJECT_ROOT}/src/engine/ObjectCodes.h"
    "${PROJECT_ROOT}/src/engine/PseudoRandom.h"
    "${PROJECT_ROOT}/src/engine/ProgressionRules.h"
    "${PROJECT_ROOT}/src/engine/RockfordRules.h"
    "${PROJECT_ROOT}/src/engine/SnapshotText.h"
    "${PROJECT_ROOT}/src/engine/TimeBasedRandom.h"
)

add_library(boulderdash-engine STATIC
    ${BOULDERDASH_ENGINE_SOURCES}
    ${BOULDERDASH_ENGINE_HEADERS}
)

set_target_properties(boulderdash-engine PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

target_compile_features(boulderdash-engine PUBLIC cxx_std_17)

target_include_directories(boulderdash-engine
    PUBLIC
        "${PROJECT_ROOT}/src"
)

if(WIN32)
    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    if(_qt_qmake_executable)
        get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
    endif()
endif()

if(WIN32 AND BOULDERDASH_DEPLOY_QT_RUNTIME)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${_qt_bin_dir}"
        NO_CACHE
    )

    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt was not found. Set QT_ROOT or disable BOULDERDASH_DEPLOY_QT_RUNTIME.")
    endif()
endif()

function(boulderdash_deploy_qt_runtime target)
    if(WIN32 AND BOULDERDASH_DEPLOY_QT_RUNTIME AND WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-quick-import
                --no-opengl-sw
                --compiler-runtime
                "$<TARGET_FILE:${target}>"
            COMMENT "Deploying the Qt runtime for ${target}"
            VERBATIM
        )
    endif()
endfunction()

set(BOULDERDASH_SOURCES
    "${PROJECT_ROOT}/src/resources/c64/c64-tiles.qrc"
    "${PROJECT_ROOT}/src/audio/AudioController.cpp"
    "${PROJECT_ROOT}/src/app/C64HudPresentation.cpp"
    "${PROJECT_ROOT}/src/app/CaveView.cpp"
    "${PROJECT_ROOT}/src/app/FixedStepRunner.cpp"
    "${PROJECT_ROOT}/src/app/KeyboardInput.cpp"
    "${PROJECT_ROOT}/src/app/MainWindow.cpp"
    "${PROJECT_ROOT}/src/main.cpp"
)

set(BOULDERDASH_HEADERS
    "${PROJECT_ROOT}/src/audio/AudioController.h"
    "${PROJECT_ROOT}/src/app/C64HudPresentation.h"
    "${PROJECT_ROOT}/src/app/CaveView.h"
    "${PROJECT_ROOT}/src/app/FixedStepRunner.h"
    "${PROJECT_ROOT}/src/app/KeyboardInput.h"
    "${PROJECT_ROOT}/src/app/MainWindow.h"
)

qt_add_executable(boulderdash WIN32
    ${BOULDERDASH_SOURCES}
    ${BOULDERDASH_HEADERS}
)

target_compile_features(boulderdash PRIVATE cxx_std_17)

target_include_directories(boulderdash PRIVATE "${PROJECT_ROOT}/src")

target_link_libraries(boulderdash
    PRIVATE
        boulderdash-engine
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Widgets
)

if(MSVC)
    target_compile_options(boulderdash PRIVATE /MP /EHsc /Zc:__cplusplus /permissive-)
endif()

source_group(TREE "${PROJECT_ROOT}/src" PREFIX "src" FILES
    ${BOULDERDASH_SOURCES}
    ${BOULDERDASH_HEADERS}
)

boulderdash_deploy_qt_runtime(boulderdash)
install(TARGETS boulderdash RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
set_property(DIRECTORY "${PROJECT_ROOT}" PROPERTY VS_STARTUP_PROJECT boulderdash)
