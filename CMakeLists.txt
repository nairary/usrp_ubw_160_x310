cmake_minimum_required(VERSION 3.24)
project(usrp_ubw_160_x310 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Match the release CRT used by official UHD Windows SDKs and x64-windows vcpkg.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
option(USRP_BUILD_APP "Build the UHD/GLFW spectrum analyzer" ON)
include(CTest)
find_package(Threads REQUIRED)

add_library(wifi_detection STATIC LStfDetector.cpp WiFiDemodulator.cpp Sc16Reader.cpp)
target_include_directories(wifi_detection PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(wifi_detection PUBLIC Threads::Threads)
add_executable(wifi_detect tools/wifi_detect.cpp)
target_link_libraries(wifi_detect PRIVATE wifi_detection)
if(BUILD_TESTING)
    add_executable(wifi_tests tests/wifi_tests.cpp)
    target_link_libraries(wifi_tests PRIVATE wifi_detection)
    add_test(NAME wifi_unit COMMAND wifi_tests)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME wifi_offline_cli COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/offline_cli_test.py" "$<TARGET_FILE:wifi_detect>")
    endif()
endif()

if(USRP_BUILD_APP)
    if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Select an x64 generator/triplet: this application requires Windows x64 SDKs.")
    endif()
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(UHD REQUIRED MODULE)
    find_package(Boost 1.70 REQUIRED COMPONENTS program_options)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(FFTW3f CONFIG QUIET)
    if(NOT TARGET FFTW3::fftw3f)
        # Distribution SDKs without an FFTW CMake config (e.g. Debian).
        find_path(FFTW3F_INCLUDE_DIR fftw3.h REQUIRED)
        find_library(FFTW3F_LIBRARY NAMES fftw3f libfftw3f-3 REQUIRED)
        add_library(FFTW3::fftw3f UNKNOWN IMPORTED)
        set_target_properties(FFTW3::fftw3f PROPERTIES IMPORTED_LOCATION "${FFTW3F_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${FFTW3F_INCLUDE_DIR}")
    endif()

    add_library(usrp_gui STATIC
        ext/imgui/imgui.cpp ext/imgui/imgui_draw.cpp ext/imgui/imgui_tables.cpp
        ext/imgui/imgui_widgets.cpp ext/imgui/imgui_demo.cpp
        ext/imgui/imgui_impl_glfw.cpp ext/imgui/imgui_impl_opengl3.cpp
        ext/imgui/implot.cpp ext/imgui/implot_items.cpp ext/imgui/implot_demo.cpp)
    target_include_directories(usrp_gui PUBLIC ext ext/imgui)
    target_link_libraries(usrp_gui PUBLIC glfw OpenGL::GL PRIVATE ${CMAKE_DL_LIBS})
    # glad.c is unused: the backend above supplies its own GL loader.
    add_executable(usrp main.cpp Application.cpp USRPDevice.cpp ReaTimeFFT.cpp PlotFunctions.cpp)
    target_link_libraries(usrp PRIVATE wifi_detection usrp_gui UHD::uhd FFTW3::fftw3f Boost::program_options)
    set_target_properties(usrp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:usrp>")
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT usrp)

    if(MSVC)
        target_compile_options(usrp PRIVATE /utf-8 /EHsc)
        target_compile_definitions(usrp PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_definitions(usrp_gui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(usrp_gui PRIVATE /utf-8 /EHsc)
        target_compile_options(wifi_detection PRIVATE /utf-8 /EHsc)
        set(USRP_RUNTIME_DIRS "" CACHE STRING "Extra matching SDK runtime directories (semicolon-separated)")
        get_filename_component(_uhd_bin "${UHD_RUNTIME_DLL}" DIRECTORY)
        get_filename_component(_linker_dir "${CMAKE_LINKER}" DIRECTORY)
        set(_runtime_dirs "${_uhd_bin}" ${USRP_RUNTIME_DIRS})
        if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
            list(APPEND _runtime_dirs "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        endif()
        add_custom_command(TARGET usrp POST_BUILD
            COMMAND "${CMAKE_COMMAND}" "-Dexecutable=$<TARGET_FILE:usrp>"
                "-Ddumpbin=${_linker_dir}/dumpbin.exe" "-Dsearch_dirs=${_runtime_dirs}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake"
            VERBATIM)
    endif()
endif()
