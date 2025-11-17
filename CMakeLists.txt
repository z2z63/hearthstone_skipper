cmake_minimum_required(VERSION 3.30)
project(hearththone_skipper)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

include(ExternalProject)

#qt_standard_project_setup()
# 防止AUTO_MOC属性污染依赖库，参考 https://doc.qt.io/qt-6/qt-standard-project-setup.html#description , 手动设置 AUTO_MOC


file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)


qt_add_executable(skipper ${SRC} ${HEADERS})
qt_add_executable(qcurl_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/qcurl_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qcurl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qcurl.h
)
target_include_directories(qcurl_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(skipper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(skipper PROPERTIES
        AUTOMOC ON
)

set_target_properties(qcurl_test PROPERTIES
        AUTOMOC ON
)

#set(CMAKE_CXX_FLAGS_DEBUG "-fsanitize=undefined,address -fno-omit-frame-pointer")

target_link_libraries(skipper PRIVATE Qt6::Widgets Qt6::Network)
target_link_libraries(qcurl_test PRIVATE Qt6::Widgets)

set_target_properties(skipper PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
)
set_target_properties(skipper PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/info.plist)

ExternalProject_Add(
        libyaml-cpp
        PREFIX ${CMAKE_BUILD_TYPE}
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
        GIT_SHALLOW true
        INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libyaml-cpp-${CMAKE_BUILD_TYPE}/usr
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libyaml-cpp-${CMAKE_BUILD_TYPE}/usr -DBUILD_SHARED_LIBS=ON
)
ExternalProject_Add(
        libspdlog
        PREFIX ${CMAKE_BUILD_TYPE}
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.1
        GIT_SHALLOW true
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libspdlog-${CMAKE_BUILD_TYPE}/usr
)

ExternalProject_Add(
        libcurl
        PREFIX ${CMAKE_BUILD_TYPE}
        URL https://curl.se/download/curl-8.17.0.zip
        CMAKE_ARGS -DBUILD_CURL_EXE=OFF -DBUILD_EXAMPLES=OFF -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF
        -DBUILD_TESTING=OFF -DCURL_USE_LIBPSL=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libcurl-${CMAKE_BUILD_TYPE}/usr
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DENABLE_DEBUG=ON -DCMAKE_C_FLAGS=-fsanitize=undefined,address\ -g
)

# 添加 yaml-cpp 依赖
target_include_directories(skipper PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libyaml-cpp-${CMAKE_BUILD_TYPE}/usr/include)
target_link_directories(skipper PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libyaml-cpp-${CMAKE_BUILD_TYPE}/usr/lib)
target_link_libraries(skipper PRIVATE yaml-cpp)

# 添加 spdlog 依赖
target_include_directories(skipper PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libspdlog-${CMAKE_BUILD_TYPE}/usr/include)
target_link_directories(skipper PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libspdlog-${CMAKE_BUILD_TYPE}/usr/lib)
target_link_libraries(skipper PRIVATE spdlog)

# 添加 libcurl 依赖
target_include_directories(skipper PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libcurl-${CMAKE_BUILD_TYPE}/usr/include)
target_link_directories(skipper PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libcurl-${CMAKE_BUILD_TYPE}/usr/lib)
target_link_libraries(skipper PRIVATE curl)

add_dependencies(skipper libyaml-cpp libspdlog libcurl)


# 添加 libcurl 依赖
#target_include_directories(qcurl_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libcurl-${CMAKE_BUILD_TYPE}/usr/include)
#target_link_directories(qcurl_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libcurl-${CMAKE_BUILD_TYPE}/usr/lib)
find_package(CURL REQUIRED PATHS ${CMAKE_CURRENT_BINARY_DIR}/libcurl-${CMAKE_BUILD_TYPE}/usr)
target_link_libraries(qcurl_test PRIVATE CURL::libcurl)

add_dependencies(qcurl_test libcurl)

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

message(STATUS APP_VERSION=${APP_VERSION})
message(STATUS QT_VERSION=${Qt6_VERSION})
