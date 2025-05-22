cmake_minimum_required(VERSION 3.21...3.31)

# Default build type value.
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type")

project(pxr-work
    VERSION 0.25.5
    HOMEPAGE_URL "https://github.com/untwine/pxr-work"
    LANGUAGES C CXX
)

if (NOT "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "Default C++ standard")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default options.
option(BUILD_TESTS "Build tests" ON)
option(BUILD_SHARED_LIBS "Build Shared Library" ON)
option(BUILD_PYTHON_BINDINGS "Build Python Bindings" ON)
option(ENABLE_PRECOMPILED_HEADERS "Enable precompiled headers." OFF)

if (NOT BUILD_SHARED_LIBS)
    add_compile_definitions(PXR_STATIC)
endif()

include(GNUInstallDirs)

if (WIN32)
    # Prevent WinDef.h from defining min/max macros.
    add_definitions(-DNOMINMAX)

    # Exclude unnecessary Windows APIs for faster builds and fewer macro conflicts.
    add_definitions(-DWIN32_LEAN_AND_MEAN)

    # Disable /Zc:inline in VS 2019+ to retain "arch_ctor_<name>" symbols in release.
    # https://developercommunity.visualstudio.com/t/914943
    if (MSVC_VERSION GREATER_EQUAL 1920)
        set(CMAKE_CXX_FLAGS "/Zc:inline- ${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "/Zc:inline ${CMAKE_CXX_FLAGS}")
    endif()

    # Disable harmless Visual Studio warning C4003 (non-standard preprocessor).
    # https://developercommunity.visualstudio.com/t/364698
    set(CMAKE_CXX_FLAGS "/wd4003 ${CMAKE_CXX_FLAGS}")
endif()

# Update build type from environment for CMake < 3.22
if (DEFINED ENV{CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE}
        CACHE STRING "Specifies the build type" FORCE)
endif()

find_package(pxr-tf 0.25.5 REQUIRED)
find_package(pxr-trace 0.25.5 REQUIRED)
find_package(TBB 2017.0 REQUIRED)

if(BUILD_PYTHON_BINDINGS)
    add_compile_definitions(PXR_PYTHON_SUPPORT_ENABLED=1)
    find_package(pxr-boost 0.25.5 REQUIRED)

    if (NOT CMAKE_INSTALL_PYTHON_LIBDIR)
        set(py_version "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
        set(CMAKE_INSTALL_PYTHON_LIBDIR
            "${CMAKE_INSTALL_LIBDIR}/python${py_version}/site-packages"
            CACHE INTERNAL "Installation Python library path.")
    endif ()
endif()

add_subdirectory(src)

# Build and setup tests if required.
if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "cmake/pxr-work-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pxr-work-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/pxr-work
)

write_basic_package_version_file(
    "pxr-work-config-version.cmake"
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/pxr-work-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/pxr-work-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/pxr-work
)
