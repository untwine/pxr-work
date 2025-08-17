if (USE_WORK_TBB)
    set(PXR_WORK_IMPL_HEADER [["pxr/work/workTBB/impl.h"]])
else()
    set(PXR_WORK_IMPL_HEADER "<${PXR_WORK_IMPL}/impl.h>")
endif()

configure_file(
    pxr/work/pxr.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/pxr/work/pxr.h
    @ONLY
)

configure_file(
    pxr/work/impl.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/pxr/work/impl.h
    @ONLY
)

add_library(work
    pxr/work/dispatcher.cpp
    pxr/work/taskGraph.cpp
    pxr/work/taskGraph_defaultImpl.cpp
    pxr/work/threadLimits.cpp
    pxr/work/utils.cpp
)

target_include_directories(work
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Compile features can only raise the effective standard, so this can't be
# undercut by a toolchain file that pre-sets CMAKE_CXX_STANDARD lower.
target_compile_features(work PUBLIC cxx_std_17)

target_link_libraries(work
    PUBLIC
        pxr::arch
        pxr::tf
        pxr::trace
        TBB::tbb
)

if(NOT WIN32 AND NOT SKBUILD)
    set_target_properties(work PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(BUILD_PYTHON_BINDINGS)
    target_sources(work
        PRIVATE
            pxr/work/moduleDeps.cpp
    )

    target_link_libraries(work
        PUBLIC
            pxr::boost-python
    )

    add_subdirectory(python)
endif()

if (ENABLE_PRECOMPILED_HEADERS)
    target_precompile_headers(work
        PRIVATE
            pxr/work/pch.h
    )
endif()

set_target_properties(work
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "PxrWork"
)

target_compile_definitions(work
    PRIVATE
        WORK_EXPORTS=1
        MFB_PACKAGE_MODULE=Work
        MFB_PACKAGE_NAME=work
        MFB_ALT_PACKAGE_NAME=work
)

target_sources(work
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/pxr/work/pxr.h
            ${CMAKE_CURRENT_BINARY_DIR}/pxr/work/impl.h
            pxr/work/api.h
            pxr/work/detachedTask.h
            pxr/work/dispatcher.h
            pxr/work/isolatingDispatcher.h
            pxr/work/loops.h
            pxr/work/reduce.h
            pxr/work/singularTask.h
            pxr/work/sort.h
            pxr/work/taskGraph.h
            pxr/work/taskGraph_defaultImpl.h
            pxr/work/threadLimits.h
            pxr/work/utils.h
            pxr/work/withScopedParallelism.h
            pxr/work/zeroAllocator.h
)

if (USE_WORK_TBB)
    target_sources(work
        PRIVATE
            pxr/work/workTBB/detachedTask_impl.cpp
            pxr/work/workTBB/dispatcher_impl.cpp
            pxr/work/workTBB/isolatingDispatcher_impl.cpp
            pxr/work/workTBB/taskGraph_impl.cpp
            pxr/work/workTBB/threadLimits_impl.cpp
    )
    
    target_sources(work
        PUBLIC
            FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS
                ${CMAKE_CURRENT_SOURCE_DIR}
            FILES
                pxr/work/workTBB/detachedTask_impl.h
                pxr/work/workTBB/dispatcher_impl.h
                pxr/work/workTBB/impl.h
                pxr/work/workTBB/isolatingDispatcher_impl.h
                pxr/work/workTBB/loops_impl.h
                pxr/work/workTBB/reduce_impl.h
                pxr/work/workTBB/sort_impl.h
                pxr/work/workTBB/taskGraph_impl.h
                pxr/work/workTBB/tbb_version.h
                pxr/work/workTBB/threadLimits_impl.h
                pxr/work/workTBB/withScopedParallelism_impl.h
    )
else()
    find_package("${PXR_WORK_IMPL}" CONFIG REQUIRED)
    target_link_libraries(work
        PUBLIC
            "${PXR_WORK_IMPL}::${PXR_WORK_IMPL}"
    )
endif()

install(
    TARGETS work EXPORT ${PROJECT_NAME}
    FILE_SET public_headers
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(EXPORT ${PROJECT_NAME}
    FILE pxr-work-targets.cmake
    NAMESPACE pxr::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/pxr-work
)
