cmake_minimum_required(VERSION 3.30)
project(hearththone_skipper)

set(CMAKE_CXX_STANDARD 20)


find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
qt_standard_project_setup()

file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
message(STATUS SRC=${SRC})

qt_add_executable(skipper ${SRC})
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
        GIT_TAG yaml-cpp-0.7.0
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
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
        TARGET skipper
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})