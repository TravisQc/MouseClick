cmake_minimum_required(VERSION 3.23)

project(MouseClick VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

if (NOT WIN32)
    message(FATAL_ERROR "MouseClick is a Windows-only application")
endif()

if (CMAKE_SIZEOF_VOID_P LESS 8)
    message(FATAL_ERROR "MouseClick requires an x64 toolchain")
endif()

set(MOUSECLICK_SOURCES
    src/domain.cpp
    src/settings.cpp
    src/click_worker.cpp
    src/hotkey.cpp
    src/ui.cpp
    src/main.cpp
    src/MouseClick.rc
)

add_executable(MouseClick WIN32 ${MOUSECLICK_SOURCES})

target_include_directories(MouseClick PRIVATE src)
target_compile_definitions(MouseClick PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000006
)

if (MSVC)
    target_compile_options(MouseClick PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHs-
        /EHc-
        /GR-
        $<$<CONFIG:Release>:/O1>
        $<$<CONFIG:Release>:/Ob1>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/GF>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
        $<$<CONFIG:Release>:/GL>
    )
    set_source_files_properties(src/main.cpp PROPERTIES
        COMPILE_OPTIONS "$<$<CONFIG:Release>:/GS-;/GL->"
    )
    set_source_files_properties(src/runtime_support.cpp PROPERTIES
        COMPILE_OPTIONS "$<$<CONFIG:Release>:/GS-;/Oi-;/GL->"
    )
    target_compile_definitions(MouseClick PRIVATE
        $<$<CONFIG:Release>:MOUSECLICK_CUSTOM_ENTRY>
    )

    get_filename_component(MSVC_BIN_DIRECTORY "${CMAKE_CXX_COMPILER}" DIRECTORY)
    cmake_path(ABSOLUTE_PATH MSVC_BIN_DIRECTORY
        NORMALIZE OUTPUT_VARIABLE MSVC_BIN_DIRECTORY)
    find_program(MSVC_DUMPBIN_EXECUTABLE NAMES dumpbin
        HINTS "${MSVC_BIN_DIRECTORY}" REQUIRED)
    cmake_path(SET MSVC_LIBCMT_LIBRARY
        "${MSVC_BIN_DIRECTORY}/../../../lib/x64/libcmt.lib")
    cmake_path(NORMAL_PATH MSVC_LIBCMT_LIBRARY)
    set(MSVC_GS_HANDLER_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/gshandler.obj")
    if (NOT EXISTS "${MSVC_LIBCMT_LIBRARY}")
        message(FATAL_ERROR "MSVC static support library was not found: ${MSVC_LIBCMT_LIBRARY}")
    endif()
    execute_process(
        COMMAND "${CMAKE_AR}" /list "${MSVC_LIBCMT_LIBRARY}"
        OUTPUT_VARIABLE MSVC_LIBCMT_MEMBERS
        RESULT_VARIABLE MSVC_LIBCMT_LIST_RESULT
    )
    if (NOT MSVC_LIBCMT_LIST_RESULT EQUAL 0)
        message(FATAL_ERROR "Unable to inspect MSVC static support library")
    endif()
    string(REGEX MATCH "[^\r\n]*[\\\\/]gshandler\\.obj"
        MSVC_GS_HANDLER_MEMBER "${MSVC_LIBCMT_MEMBERS}")
    if (MSVC_GS_HANDLER_MEMBER STREQUAL "")
        message(FATAL_ERROR "The MSVC x64 GS unwind support object was not found")
    endif()
    execute_process(
        COMMAND "${CMAKE_AR}" "/extract:${MSVC_GS_HANDLER_MEMBER}"
                "/out:${MSVC_GS_HANDLER_OBJECT}" "${MSVC_LIBCMT_LIBRARY}"
        RESULT_VARIABLE MSVC_GS_HANDLER_EXTRACT_RESULT
    )
    if (NOT MSVC_GS_HANDLER_EXTRACT_RESULT EQUAL 0 OR
        NOT EXISTS "${MSVC_GS_HANDLER_OBJECT}")
        message(FATAL_ERROR "Unable to extract the minimal MSVC x64 GS unwind support")
    endif()
    target_sources(MouseClick PRIVATE
        $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime_support.cpp>
        $<$<CONFIG:Release>:${MSVC_GS_HANDLER_OBJECT}>
    )

    target_link_options(MouseClick PRIVATE
        /MANIFEST:NO
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        $<$<CONFIG:Release>:/INCREMENTAL:NO>
        $<$<CONFIG:Release>:/ENTRY:CustomEntry>
        $<$<CONFIG:Release>:/NODEFAULTLIB>
        $<$<CONFIG:Release>:/VERBOSE:LIB>
        $<$<CONFIG:Release>:/MAP:$<TARGET_FILE_DIR:MouseClick>/MouseClick.map>
    )
endif()

target_link_libraries(MouseClick PRIVATE
    user32
    gdi32
    advapi32
    comctl32
    dwmapi
    ole32
    uxtheme
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:BufferOverflowU>
)

if (NOT MSVC)
    target_link_options(MouseClick PRIVATE -municode)
endif()

if (MSVC)
    find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
    add_custom_command(TARGET MouseClick POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DOUTPUT_FILE=$<TARGET_FILE:MouseClick>
            -DBUILD_CONFIG=$<CONFIG>
            -DDUMPBIN_EXECUTABLE=${MSVC_DUMPBIN_EXECUTABLE}
            -DMAP_FILE=$<TARGET_FILE_DIR:MouseClick>/MouseClick.map
            -DPROJECT_FILE=${CMAKE_CURRENT_BINARY_DIR}/MouseClick.vcxproj
            -DRESOURCE_INSPECTOR=${CMAKE_CURRENT_SOURCE_DIR}/cmake/inspect_resources.ps1
            -DPOWERSHELL_EXECUTABLE=${POWERSHELL_EXECUTABLE}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_release.cmake
        VERBATIM
    )
endif()

enable_testing()

add_executable(MouseClickCoreTests
    tests/core_tests.cpp
    src/domain.cpp
    src/settings.cpp
    src/click_worker.cpp
)

target_include_directories(MouseClickCoreTests PRIVATE src)
target_compile_definitions(MouseClickCoreTests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)
target_link_libraries(MouseClickCoreTests PRIVATE user32)

if (MSVC)
    target_compile_options(MouseClickCoreTests PRIVATE /W4 /permissive- /utf-8 /EHsc /GR-)
endif()

add_test(NAME MouseClickCoreTests COMMAND MouseClickCoreTests)

add_executable(MouseClickSmokeHarness EXCLUDE_FROM_ALL
    tests/smoke_harness.cpp
)
target_link_libraries(MouseClickSmokeHarness PRIVATE user32)
if (MSVC)
    target_compile_options(MouseClickSmokeHarness PRIVATE /W4 /permissive- /utf-8 /EHsc /GR-)
endif()

install(TARGETS MouseClick RUNTIME DESTINATION .)
