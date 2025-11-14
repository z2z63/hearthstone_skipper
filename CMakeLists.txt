cmake_minimum_required(VERSION 3.30)
project(hearththone_skipper)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

#qt_standard_project_setup()
# 防止AUTO_MOC属性污染依赖库，参考 https://doc.qt.io/qt-6/qt-standard-project-setup.html#description , 手动设置 AUTO_MOC


file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

qt_add_executable(skipper ${SRC})
set_target_properties(skipper PROPERTIES
        AUTOMOC ON
)

target_link_libraries(skipper PRIVATE Qt6::Widgets Qt6::Network)

set_target_properties(skipper PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
)
set_target_properties(skipper PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/info.plist)

include(FetchContent)

FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.1
)

FetchContent_MakeAvailable(yaml-cpp)
FetchContent_MakeAvailable(spdlog)

target_link_libraries(skipper PUBLIC yaml-cpp::yaml-cpp) # The library or executable that require yaml-cpp library
target_link_libraries(skipper PRIVATE spdlog::spdlog)

install(TARGETS skipper
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(deploy_tool_options "")
if (APPLE)
    set(deploy_tool_options "-dmg")
endif ()

qt_generate_deploy_app_script(
        TARGET skipper
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS ${deploy_tool_options}
)
install(SCRIPT ${deploy_script})

execute_process(
        COMMAND git describe --tags
        OUTPUT_VARIABLE APP_VERSION
)

target_compile_definitions(skipper PRIVATE APP_VERSION="${APP_VERSION}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(skipper PRIVATE SKIPPER_DEBUG)
endif ()
