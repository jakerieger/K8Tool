cmake_minimum_required(VERSION 3.26)
project(K8Tool VERSION 3.0.0 LANGUAGES CXX RC)

if (NOT MSVC)
    message(FATAL_ERROR "K8Tool requires the MSVC toolchain")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach (OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach ()

option(K8TOOL_BUILD_TESTS "Build the K8Tool core tests" ON)
# Removing libraries needs administrator rights. Turn this off to run under a debugger that isn't elevated; scanning
# still works, but removal and relocation will fail.
option(K8TOOL_REQUIRE_ADMIN "Ask for administrator rights when K8Tool starts" ON)

set(K8TOOL_XENUI_DIR "" CACHE PATH "Local XenUI checkout to build against instead of fetching one")
if (K8TOOL_XENUI_DIR)
    set(FETCHCONTENT_SOURCE_DIR_XENUI "${K8TOOL_XENUI_DIR}" CACHE PATH "" FORCE)
else ()
    unset(FETCHCONTENT_SOURCE_DIR_XENUI CACHE)
endif ()

include(FetchContent)
FetchContent_Declare(XenUI
        GIT_REPOSITORY https://github.com/jakerieger/XenUI.git
        GIT_TAG master
)
set(XENUI_BUILD_TESTS OFF)
set(XENUI_BUILD_SHOWCASE OFF)
set(XENUI_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(XenUI)

configure_file(Resource/K8ToolVersion.h.in "${CMAKE_BINARY_DIR}/Generated/K8ToolVersion.h" @ONLY)
if (K8TOOL_REQUIRE_ADMIN)
    set(K8TOOL_EXECUTION_LEVEL requireAdministrator)
else ()
    set(K8TOOL_EXECUTION_LEVEL asInvoker)
endif ()
configure_file(Resource/K8Tool.manifest.in "${CMAKE_BINARY_DIR}/Generated/K8Tool.manifest" @ONLY)

# Everything K8Tool does to the registry and disk, with no UI, so it can be tested on its own.
add_library(K8ToolCore STATIC
        Source/Core/Backups.cpp
        Source/Core/Backups.hpp
        Source/Core/Constants.hpp
        Source/Core/Environment.cpp
        Source/Core/Environment.hpp
        Source/Core/Export.cpp
        Source/Core/Export.hpp
        Source/Core/Files.cpp
        Source/Core/Files.hpp
        Source/Core/Format.cpp
        Source/Core/Format.hpp
        Source/Core/Library.hpp
        Source/Core/Log.cpp
        Source/Core/Log.hpp
        Source/Core/Progress.hpp
        Source/Core/Registry.cpp
        Source/Core/Registry.hpp
        Source/Core/Relocation.cpp
        Source/Core/Relocation.hpp
        Source/Core/Removal.cpp
        Source/Core/Removal.hpp
        Source/Core/Scanner.cpp
        Source/Core/Scanner.hpp
        Source/Core/ServiceCenter.cpp
        Source/Core/ServiceCenter.hpp
        Source/Core/Text.cpp
        Source/Core/Text.hpp
        Source/Core/Update.cpp
        Source/Core/Update.hpp
        Vendor/tinyxml2.cpp
)
XenUI_ConfigureTarget(K8ToolCore)
target_include_directories(K8ToolCore PUBLIC Source)
target_include_directories(K8ToolCore SYSTEM PUBLIC Vendor)
target_link_libraries(K8ToolCore PUBLIC advapi32 shell32 ole32 winhttp)
# The vendored libraries are not ours to fix.
set_source_files_properties(Vendor/tinyxml2.cpp PROPERTIES COMPILE_OPTIONS "/W0")
set_source_files_properties(Source/Core/Backups.cpp PROPERTIES COMPILE_OPTIONS "/W0")

add_executable(K8Tool WIN32
        Resource/K8Tool.rc
        "${CMAKE_BINARY_DIR}/Generated/K8Tool.manifest"
        Source/App/AboutDialog.cpp
        Source/App/AboutDialog.hpp
        Source/App/AppModel.cpp
        Source/App/AppModel.hpp
        Source/App/LogWindow.cpp
        Source/App/LogWindow.hpp
        Source/App/MainView.cpp
        Source/App/MainView.hpp
        Source/App/RelocateDialog.cpp
        Source/App/RelocateDialog.hpp
        Source/App/RemoveDialog.cpp
        Source/App/RemoveDialog.hpp
        Source/App/TaskDialogs.cpp
        Source/App/TaskDialogs.hpp
        Source/App/TaskRunner.hpp
        Source/main.cpp
)
XenUI_ConfigureTarget(K8Tool)
target_include_directories(K8Tool PRIVATE Source Resource "${CMAKE_BINARY_DIR}/Generated")
# The UAC level comes from Resource/K8Tool.manifest, not the linker's default asInvoker.
target_link_options(K8Tool PRIVATE /ENTRY:mainCRTStartup /MANIFESTUAC:NO)
target_link_libraries(K8Tool PRIVATE K8ToolCore XenUI::XenUI)

if (K8TOOL_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.11
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR header-only-no-cmake
    )
    FetchContent_MakeAvailable(doctest)

    add_executable(K8ToolTests
            Tests/Main.cpp
            Tests/CoreTests.cpp
            Tests/FileTests.cpp
            Tests/RegistryTests.cpp
            Tests/TestEnvironment.hpp
    )
    XenUI_ConfigureTarget(K8ToolTests)
    target_include_directories(K8ToolTests SYSTEM PRIVATE "${doctest_SOURCE_DIR}")
    target_link_libraries(K8ToolTests PRIVATE K8ToolCore)
    add_test(NAME K8ToolTests COMMAND K8ToolTests)
endif ()
