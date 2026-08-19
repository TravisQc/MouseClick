if (NOT DEFINED OUTPUT_FILE OR NOT EXISTS "${OUTPUT_FILE}")
    message(FATAL_ERROR "Release output was not found: ${OUTPUT_FILE}")
endif()

if (DEFINED BUILD_CONFIG AND NOT BUILD_CONFIG STREQUAL "" AND
    NOT BUILD_CONFIG STREQUAL "Release")
    message(STATUS "Skipping Release product gate for ${BUILD_CONFIG}")
    return()
endif()

file(SIZE "${OUTPUT_FILE}" OUTPUT_SIZE)
set(SIZE_LIMIT 131072)
if (OUTPUT_SIZE GREATER SIZE_LIMIT)
    message(FATAL_ERROR
        "MouseClick.exe is ${OUTPUT_SIZE} bytes; the limit is ${SIZE_LIMIT} bytes")
endif()

if (NOT DEFINED DUMPBIN_EXECUTABLE OR NOT EXISTS "${DUMPBIN_EXECUTABLE}")
    find_program(DUMPBIN_EXECUTABLE dumpbin)
endif()
if (NOT DUMPBIN_EXECUTABLE)
    message(FATAL_ERROR "dumpbin is required for MSVC Release PE verification")
endif()

function(run_dumpbin mode output_name)
    execute_process(
        COMMAND "${DUMPBIN_EXECUTABLE}" "${mode}" "${OUTPUT_FILE}"
        OUTPUT_VARIABLE dump_output
        ERROR_VARIABLE dump_errors
        RESULT_VARIABLE dump_result
    )
    if (NOT dump_result EQUAL 0 OR dump_output STREQUAL "")
        message(FATAL_ERROR "dumpbin ${mode} failed: ${dump_errors}")
    endif()
    set(${output_name} "${dump_output}" PARENT_SCOPE)
endfunction()

run_dumpbin(/headers HEADERS)
run_dumpbin(/imports IMPORTS)
run_dumpbin(/loadconfig LOAD_CONFIG)
string(REPLACE "\r" "" HEADERS "${HEADERS}")
string(TOLOWER "${HEADERS}" HEADERS_LOWER)
string(TOLOWER "${IMPORTS}" IMPORTS_LOWER)
string(TOLOWER "${LOAD_CONFIG}" LOAD_CONFIG_LOWER)

set(SECTION_REPORT "")
foreach (SECTION text rdata data pdata rsrc reloc)
    string(REGEX MATCH
        "\\.${SECTION} name\n[^\n]*\n[^\n]*\n[ \t]*([0-9a-fA-F]+) size of raw data"
        SECTION_MATCH "${HEADERS}")
    if (SECTION_MATCH)
        set(SECTION_HEX "${CMAKE_MATCH_1}")
        math(EXPR SECTION_BYTES "0x${SECTION_HEX}")
        string(APPEND SECTION_REPORT " ${SECTION}=${SECTION_BYTES}")
    endif()
endforeach()
if (SECTION_REPORT STREQUAL "")
    message(FATAL_ERROR "Unable to parse tracked PE section sizes")
endif()

foreach (REQUIRED_TEXT
         "subsystem (windows gui)"
         "high entropy virtual addresses"
         "dynamic base"
         "nx compatible")
    string(FIND "${HEADERS_LOWER}" "${REQUIRED_TEXT}" REQUIRED_INDEX)
    if (REQUIRED_INDEX EQUAL -1)
        message(FATAL_ERROR "Required PE property is missing: ${REQUIRED_TEXT}")
    endif()
endforeach()
string(REGEX MATCH
    "0[ \t]+\\[[ \t]*0\\] rva \\[size\\] of thread storage directory"
    EMPTY_TLS_DIRECTORY "${HEADERS_LOWER}")
if (NOT EMPTY_TLS_DIRECTORY)
    message(FATAL_ERROR "The Release product contains a TLS initialization directory")
endif()
string(REGEX MATCH "[1-9a-f][0-9a-f]* security cookie"
    SECURITY_COOKIE "${LOAD_CONFIG_LOWER}")
if (NOT SECURITY_COOKIE)
    message(FATAL_ERROR "An initialized /GS security-cookie address was not found")
endif()

string(REGEX MATCHALL "[a-z0-9._-]+\\.dll" IMPORTED_DLLS "${IMPORTS_LOWER}")
if (NOT IMPORTED_DLLS)
    message(FATAL_ERROR "Unable to parse the Release import table")
endif()
set(ALLOWED_DLLS
    user32.dll gdi32.dll advapi32.dll comctl32.dll dwmapi.dll
    ole32.dll uxtheme.dll kernel32.dll)
foreach (IMPORTED_DLL IN LISTS IMPORTED_DLLS)
    if (NOT IMPORTED_DLL IN_LIST ALLOWED_DLLS)
        message(FATAL_ERROR "Non-system or unexpected DLL import: ${IMPORTED_DLL}")
    endif()
endforeach()

if (NOT DEFINED MAP_FILE OR NOT EXISTS "${MAP_FILE}")
    message(FATAL_ERROR "The detailed Release linker map was not produced")
endif()
file(READ "${MAP_FILE}" LINK_MAP)
string(TOLOWER "${LINK_MAP}" LINK_MAP_LOWER)
string(REGEX MATCH "([0-9a-f]+) image base" IMAGE_BASE_MATCH "${HEADERS_LOWER}")
set(IMAGE_BASE_HEX "${CMAKE_MATCH_1}")
string(REGEX MATCH "([0-9a-f]+) entry point" ENTRY_POINT_MATCH "${HEADERS_LOWER}")
set(ENTRY_POINT_HEX "${CMAKE_MATCH_1}")
string(REGEX MATCH "customentry[ \t]+([0-9a-f]+)[ \t]+f" CUSTOM_ENTRY_MATCH
    "${LINK_MAP_LOWER}")
set(CUSTOM_ENTRY_HEX "${CMAKE_MATCH_1}")
if (IMAGE_BASE_HEX STREQUAL "" OR ENTRY_POINT_HEX STREQUAL "" OR
    CUSTOM_ENTRY_HEX STREQUAL "")
    message(FATAL_ERROR "Unable to resolve the PE entry point to CustomEntry")
endif()
math(EXPR EXPECTED_ENTRY "0x${IMAGE_BASE_HEX} + 0x${ENTRY_POINT_HEX}")
math(EXPR ACTUAL_CUSTOM_ENTRY "0x${CUSTOM_ENTRY_HEX}")
if (NOT EXPECTED_ENTRY EQUAL ACTUAL_CUSTOM_ENTRY)
    message(FATAL_ERROR "The PE entry point does not resolve to CustomEntry")
endif()

foreach (REQUIRED_SYMBOL customentry __security_init_cookie __security_cookie)
    string(FIND "${LINK_MAP_LOWER}" "${REQUIRED_SYMBOL}" REQUIRED_SYMBOL_INDEX)
    if (REQUIRED_SYMBOL_INDEX EQUAL -1)
        message(FATAL_ERROR "Required Release symbol is missing: ${REQUIRED_SYMBOL}")
    endif()
endforeach()
foreach (FORBIDDEN_SYMBOL
         maincrtstartup wwinmaincrtstartup winmaincrtstartup
         _initterm atexit __dyn_tls_init __xc_a __xc_z)
    string(FIND "${LINK_MAP_LOWER}" "${FORBIDDEN_SYMBOL}" FORBIDDEN_SYMBOL_INDEX)
    if (NOT FORBIDDEN_SYMBOL_INDEX EQUAL -1)
        message(FATAL_ERROR "CRT startup or initializer dependency found: ${FORBIDDEN_SYMBOL}")
    endif()
endforeach()
foreach (FORBIDDEN_LIBRARY
         msvcrt.lib msvcrtd.lib libcmt.lib libcmtd.lib libcpmt.lib libcpmtd.lib
         msvcprt.lib msvcprtd.lib vcruntime.lib vcruntimed.lib)
    string(FIND "${LINK_MAP_LOWER}" "${FORBIDDEN_LIBRARY}" FORBIDDEN_LIBRARY_INDEX)
    if (NOT FORBIDDEN_LIBRARY_INDEX EQUAL -1)
        message(FATAL_ERROR "Prohibited runtime library linked: ${FORBIDDEN_LIBRARY}")
    endif()
endforeach()
foreach (PACKER_MARKER upx0 upx1 upx2 .packed mpress themida)
    string(FIND "${HEADERS_LOWER}" "${PACKER_MARKER}" PACKER_INDEX)
    if (NOT PACKER_INDEX EQUAL -1)
        message(FATAL_ERROR "Executable packing layer detected: ${PACKER_MARKER}")
    endif()
endforeach()

if (NOT DEFINED PROJECT_FILE OR NOT EXISTS "${PROJECT_FILE}")
    message(FATAL_ERROR "The generated MSVC product project is unavailable")
endif()
file(READ "${PROJECT_FILE}" PROJECT_CONTENT)
string(TOLOWER "${PROJECT_CONTENT}" PROJECT_CONTENT_LOWER)
foreach (PROJECT_SETTING
         "<optimization>minspace</optimization>"
         "<inlinefunctionexpansion>onlyexplicitinline</inlinefunctionexpansion>"
         "<intrinsicfunctions>true</intrinsicfunctions>"
         "<stringpooling>true</stringpooling>"
         "<functionlevellinking>true</functionlevellinking>"
         "<wholeprogramoptimization>true</wholeprogramoptimization>"
         "<entrypointsymbol>customentry</entrypointsymbol>"
         "<ignorealldefaultlibraries>true</ignorealldefaultlibraries>"
         "<linktimecodegeneration>uselinktimecodegeneration</linktimecodegeneration>"
         "<optimizereferences>true</optimizereferences>"
         "<enablecomdatfolding>true</enablecomdatfolding>")
    string(FIND "${PROJECT_CONTENT_LOWER}" "${PROJECT_SETTING}" PROJECT_SETTING_INDEX)
    if (PROJECT_SETTING_INDEX EQUAL -1)
        message(FATAL_ERROR "Required Release build setting is missing: ${PROJECT_SETTING}")
    endif()
endforeach()

if (NOT DEFINED RESOURCE_INSPECTOR OR NOT EXISTS "${RESOURCE_INSPECTOR}")
    message(FATAL_ERROR "The PE resource inspector is unavailable")
endif()
if (NOT DEFINED POWERSHELL_EXECUTABLE OR NOT EXISTS "${POWERSHELL_EXECUTABLE}")
    message(FATAL_ERROR "PowerShell is required for Release resource verification")
endif()
execute_process(
    COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
            -File "${RESOURCE_INSPECTOR}" -Executable "${OUTPUT_FILE}"
    OUTPUT_VARIABLE RESOURCE_REPORT
    ERROR_VARIABLE RESOURCE_ERRORS
    RESULT_VARIABLE RESOURCE_RESULT
)
if (NOT RESOURCE_RESULT EQUAL 0 OR NOT RESOURCE_REPORT MATCHES
    "icon=present version=present manifest=present")
    message(FATAL_ERROR "Required PE resource verification failed: ${RESOURCE_ERRORS}")
endif()

list(JOIN IMPORTED_DLLS ", " IMPORT_REPORT)
message(STATUS "MouseClick.exe total: ${OUTPUT_SIZE} bytes (limit ${SIZE_LIMIT})")
message(STATUS "MouseClick.exe raw PE sections:${SECTION_REPORT}")
message(STATUS "MouseClick.exe imports: ${IMPORT_REPORT}")
message(STATUS "MouseClick.exe entry/security/resources: CustomEntry, /GS cookie, GUI, ASLR, high-entropy VA, NX, icon, version, manifest")
