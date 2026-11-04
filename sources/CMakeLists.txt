# Specify the minimum version of CMake 
cmake_minimum_required(VERSION 3.5)

# Specify the project name
project(ppm VERSION 1.0)

set(MOC_HEADERS
    mywindow.h
    workflow.h
    usersettingspopup.h
    actionconfirmationdialog.h
    googlegateway.h
    googlechatgateway.h
    retakedialog.h
    usersettings.h
    projectsettingsdialog.h
    projectsettingsdialog_flowchart.h
    flowchartscene.h
    mainflowchartscene.h
    sceneknowledgepanel.h
    registerscenedialog.h
    aboutpopup.h
    )

set(HEADERS
    ${MOC_HEADERS}
    workflowtypes.h
    renamescript.h
    sceneinfotag.h
    projectsettings_common.h
)

set(SOURCES
main.cpp
mywindow.cpp
workflow.cpp
usersettingspopup.cpp
actionconfirmationdialog.cpp
googlegateway.cpp
googlechatgateway.cpp
renamescript.cpp
retakedialog.cpp
usersettings.cpp
sceneinfotag.cpp
projectsettingsdialog.cpp
projectsettings_common.cpp
projectsettingsdialog_flowchart.cpp
flowchartscene.cpp
mainflowchartscene.cpp
sceneknowledgepanel.cpp
registerscenedialog.cpp
aboutpopup.cpp
)

#-----------------------------------------------------------------------------
# Third Party & System Libs
# specify qtdir
    set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
    if(NOT EXISTS ${QTDIR})
        message("Specify QTDIR properly")
        return()
    endif()
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QTDIR};${QTDIR}/lib/cmake")

if(BUILD_ENV_APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10)
endif()

# Add Qt libraries. QtCore and QtGui libraries will be 
# added accordingly as the Widgets depends on them. 
find_package( Qt6 REQUIRED
    Widgets
    LinguistTools
    Network
    Core5Compat
    Concurrent
    Qml
)

# ビルドディレクトリを include ディレクトリに追加するオプションを有効にする。
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(MOC_SOURCES)

# Resource file(s)
set(RESOURCES ppm_resources.qrc)

set(RC_FILE ppm.rc)

qt6_add_resources(SOURCES ${RESOURCES})

# generate Qt translations and messages
set(LANGUAGES ja)
# 翻訳対象のファイルを設定する。
SET(TRANSLATION_TARGET_FILES
    ${HEADERS}
    ${SOURCES}
)

set(translation)
foreach(lang ${LANGUAGES})
    set(name "${CMAKE_SOURCE_DIR}/translations/PPM_${lang}.ts")
    list(APPEND translation ${name})
    set_source_files_properties(${name} PROPERTIES
        OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/../stuff/loc"
    )
endforeach()

qt6_create_translation(message ${translation} ${TRANSLATION_TARGET_FILES})

add_custom_target("translation_PPM" DEPENDS ${message})
set_target_properties("translation_PPM" PROPERTIES
    EXCLUDE_FROM_DEFAULT_BUILD TRUE
)



qt6_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})

set(SOURCES
    ${SOURCES}
    ${MOC_SOURCES}
)


# 実行ファイルを追加する。
#add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RC_FILE})
add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RESOURCES} ${RC_FILE})

# バージョン番号は、上の project() の VERSION だけで管理する（「PPM について」に表示される）
target_compile_definitions(${PROJECT_NAME} PRIVATE PPM_VERSION="${PROJECT_VERSION}")

# PPM のライセンスの正本は、リポジトリ直下の LICENSE。
# 「PPM について」と配布物で使うため、Stuff フォルダの LICENSE フォルダにコピーする
# （LICENSE を書き換えると、次のビルドのときに CMake が再実行されてコピーし直される）
configure_file(${CMAKE_SOURCE_DIR}/../LICENSE
               ${CMAKE_SOURCE_DIR}/../stuff/LICENSE/LICENSE.txt COPYONLY)

# link Qt library. Core and Gui will be linked accordingly
target_link_libraries(${PROJECT_NAME} Qt6::Widgets Qt6::Network Qt6::Core5Compat Qt6::Concurrent Qt6::Qml)

add_definitions(
    -DNOMINMAX
    -D_USE_MATH_DEFINES
)

# qt_allow_non_utf8_sources(${PROJECT_NAME})

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Set a default build type to Release
if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()
endif()
