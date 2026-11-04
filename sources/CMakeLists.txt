# Specify the minimum version of CMake 
cmake_minimum_required(VERSION 3.21)

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
    platform.h
    workflowtypes.h
    renamescript.h
    sceneinfotag.h
    projectsettings_common.h
)

set(SOURCES
main.cpp
platform.cpp
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
# QTDIR is optional; normal CMake prefix/package discovery also works.
set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
if(QTDIR)
    list(PREPEND CMAKE_PREFIX_PATH "${QTDIR}")
endif()

# Add Qt libraries. QtCore and QtGui libraries will be 
# added accordingly as the Widgets depends on them. 
find_package( Qt6 6.8 REQUIRED
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

if(WIN32)
    set(RC_FILE ppm.rc)
endif()

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
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${HEADERS} ${SOURCES} ${RESOURCES} ${RC_FILE})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if(APPLE)
    set_source_files_properties(macos/ppm.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(${PROJECT_NAME} PRIVATE macos/ppm.icns)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Svg)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Svg)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks"
        MACOSX_BUNDLE_BUNDLE_NAME "PPM"
        MACOSX_BUNDLE_ICON_FILE "ppm.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.opentoonz.ppm"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    # Use plugins from the same Qt package as the linked frameworks. Global
    # Homebrew plugin paths can point to a different or incomplete Qt install.
    set(PPM_MACOS_PLUGINS "")
    foreach(plugin QCocoaIntegrationPlugin QMacStylePlugin QSvgPlugin
                   QSvgIconPlugin QJpegPlugin QGifPlugin QTiffPlugin QWebpPlugin
                   QSecureTransportBackendPlugin)
        if(TARGET Qt6::${plugin})
            string(APPEND PPM_MACOS_PLUGINS
                "\"$<TARGET_FILE:Qt6::${plugin}>\"\n")
        endif()
    endforeach()
    configure_file(macos/deploy.cmake.in
                   "${CMAKE_CURRENT_BINARY_DIR}/deploy-macos.cmake.in" @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/deploy-macos-$<CONFIG>.cmake"
         INPUT "${CMAKE_CURRENT_BINARY_DIR}/deploy-macos.cmake.in")
    add_custom_target(deploy_macos
        COMMAND ${CMAKE_COMMAND} -P
                "${CMAKE_CURRENT_BINARY_DIR}/deploy-macos-$<CONFIG>.cmake"
        DEPENDS ${PROJECT_NAME}
        VERBATIM
    )

    # Keep editable configuration and production data outside the app bundle.
    # Do not overwrite an existing local configuration when reconfiguring.
    set(PPM_CONFIG_DIRS "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/macos")
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        list(APPEND PPM_CONFIG_DIRS "${CMAKE_BINARY_DIR}/${config}")
    endforeach()
    foreach(config_dir IN LISTS PPM_CONFIG_DIRS)
        if(NOT EXISTS "${config_dir}/conf.ini")
            file(RELATIVE_PATH PPM_STUFF_PATH "${config_dir}"
                 "${CMAKE_SOURCE_DIR}/../stuff")
            configure_file(macos/conf.ini.in "${config_dir}/conf.ini" @ONLY)
        endif()
    endforeach()
endif()

# バージョン番号は、上の project() の VERSION だけで管理する（「PPM について」に表示される）
target_compile_definitions(${PROJECT_NAME} PRIVATE PPM_VERSION="${PROJECT_VERSION}")

# PPM のライセンスの正本は、リポジトリ直下の LICENSE。
# 「PPM について」と配布物で使うため、Stuff フォルダの LICENSE フォルダにコピーする
# （LICENSE を書き換えると、次のビルドのときに CMake が再実行されてコピーし直される）
configure_file(${CMAKE_SOURCE_DIR}/../LICENSE
               ${CMAKE_SOURCE_DIR}/../stuff/LICENSE/LICENSE.txt COPYONLY)

# link Qt library. Core and Gui will be linked accordingly
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network Qt6::Core5Compat Qt6::Concurrent Qt6::Qml)

add_definitions(
    -DNOMINMAX
    -D_USE_MATH_DEFINES
)

# qt_allow_non_utf8_sources(${PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Set a default build type to Release
if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()
endif()

# Focused platform behavior tests, enabled explicitly by developers.
option(PPM_BUILD_TESTS "Build platform behavior tests" OFF)
if(PPM_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_executable(ppm_platform_tests tests/platform_test.cpp platform.cpp)
    set_target_properties(ppm_platform_tests PROPERTIES AUTOMOC ON)
    target_compile_features(ppm_platform_tests PRIVATE cxx_std_17)
    target_compile_options(ppm_platform_tests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    target_link_libraries(ppm_platform_tests PRIVATE Qt6::Test)
    add_test(NAME ppm_platform_tests COMMAND ppm_platform_tests)
    add_executable(ppm_sceneinfotag_tests tests/sceneinfotag_test.cpp sceneinfotag.cpp)
    set_target_properties(ppm_sceneinfotag_tests PROPERTIES AUTOMOC ON)
    target_compile_features(ppm_sceneinfotag_tests PRIVATE cxx_std_17)
    target_link_libraries(ppm_sceneinfotag_tests PRIVATE Qt6::Test Qt6::Gui)
    add_test(NAME ppm_sceneinfotag_tests COMMAND ppm_sceneinfotag_tests)
endif()
