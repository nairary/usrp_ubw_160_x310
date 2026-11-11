# Run after linking under MSVC. Resolve transitive SDK DLLs as well as vcpkg DLLs.
if(NOT EXISTS "${executable}")
    message(FATAL_ERROR "Runtime staging: executable not found: ${executable}")
endif()
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${dumpbin}")
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${executable}"
    DIRECTORIES ${search_dirs}
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    PRE_EXCLUDE_REGEXES "[Aa][Pp][Ii]-[Mm][Ss]-.*" "[Ee][Xx][Tt]-[Mm][Ss]-.*"
    POST_EXCLUDE_REGEXES ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*")
get_filename_component(destination "${executable}" DIRECTORY)
foreach(dll IN LISTS resolved)
    get_filename_component(source_dir "${dll}" DIRECTORY)
    if(NOT source_dir STREQUAL destination)
        file(COPY "${dll}" DESTINATION "${destination}")
    endif()
endforeach()
if(unresolved)
    message(FATAL_ERROR "Missing runtime DLLs: ${unresolved}. Set USRP_RUNTIME_DIRS to the matching UHD/Boost runtime directories, then rebuild.")
endif()
