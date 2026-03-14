cmake_minimum_required(VERSION 3.30)
project(hearththone_skipper)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include(ExternalProject)

# qt_standard_project_setup()
# 防止AUTO_MOC属性污染依赖库 手动设置 AUTO_MOC
set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

qt_add_executable(skipper

)
target_sources(skipper PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app_settings.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/clash_config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config_aware_qeasy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config_deducer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/logger.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/setting_dialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/skipper.cpp
)

target_sources(skipper PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/app.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/app_settings.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/clash_config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/config_aware_qeasy.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/config_deducer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/logger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/setting_dialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/skipper.h
)
target_include_directories(skipper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

qt_add_library(qcurl STATIC ${SRC_ROOT}/qcurl.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/qcurl.h)
target_include_directories(qcurl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(skipper PROPERTIES
        AUTOMOC ON
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/info.plist
)
set_target_properties(qcurl PROPERTIES
        AUTOMOC ON
)


include(FetchContent)

FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
)
set(SPDLOG_INSTALL ON)
set(SPDLOG_BUILD_SHARED ON)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.1
)
set(BUILD_CURL_EXE OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_LIBCURL_DOCS OFF)
set(BUILD_MISC_DOCS OFF)
set(BUILD_TESTING OFF)
set(CURL_USE_LIBPSL OFF)
set(CURL_ENABLE_SSL OFF)
set(ENABLE_CURL_MANUAL OFF)
FetchContent_Declare(
        libcurl
        URL https://curl.se/download/curl-8.17.0.zip
)

FetchContent_MakeAvailable(yaml-cpp spdlog libcurl)


target_link_libraries(skipper PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets yaml-cpp spdlog qcurl)
target_link_libraries(qcurl PRIVATE Qt6::Core PUBLIC CURL::libcurl)

install(TARGETS skipper
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
        TARGET skipper
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
if (NOT APP_VERSION)    # shallow clone 的情况下无法获取到 tag
    execute_process(
            COMMAND git describe --tags
            OUTPUT_VARIABLE APP_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()
if (NOT APP_VERSION)
    set(APP_VERSION "unknown")
endif ()
target_compile_definitions(skipper PRIVATE APP_VERSION="${APP_VERSION}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(skipper PRIVATE SKIPPER_DEBUG)
endif ()


install(CODE [[
    file(COPY ${CMAKE_INSTALL_PREFIX}/lib/libcurl.dylib DESTINATION ${CMAKE_INSTALL_PREFIX}/skipper.app/Contents/Frameworks FOLLOW_SYMLINK_CHAIN)
    file(COPY ${CMAKE_INSTALL_PREFIX}/lib/libspdlog.dylib DESTINATION ${CMAKE_INSTALL_PREFIX}/skipper.app/Contents/Frameworks FOLLOW_SYMLINK_CHAIN)
    file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR resolved_dependencies
        UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies
        CONFLICTING_DEPENDENCIES_PREFIX conflict_dependencies
        EXECUTABLES ${CMAKE_INSTALL_PREFIX}/skipper.app/Contents/MacOS/skipper
        LIBRARIES ${CMAKE_INSTALL_PREFIX}/lib/libcurl.4.dylib
        BUNDLE_EXECUTABLE ${CMAKE_INSTALL_PREFIX}/skipper.app/Contents/MacOS/skipper
    )
    message(STATUS "resolved_dependencies=${resolved_dependencies}")
    message(STATUS "unresolved_dependencies=${unresolved_dependencies}")
    message(STATUS "conflict_dependencies=${conflict_dependencies}")
]])

message(STATUS APP_VERSION=${APP_VERSION})
message(STATUS QT_VERSION=${Qt6_VERSION})