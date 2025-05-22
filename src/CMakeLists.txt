add_library(work
    pxr/work/detachedTask.cpp
    pxr/work/dispatcher.cpp
    pxr/work/threadLimits.cpp
    pxr/work/utils.cpp
)

target_include_directories(work
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(work
    PUBLIC
        pxr::tf
        pxr::trace
        TBB::tbb
)

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

install(
    TARGETS work EXPORT ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    FILES
        pxr/work/api.h
        pxr/work/detachedTask.h
        pxr/work/dispatcher.h
        pxr/work/loops.h
        pxr/work/reduce.h
        pxr/work/singularTask.h
        pxr/work/sort.h
        pxr/work/threadLimits.h
        pxr/work/utils.h
        pxr/work/withScopedParallelism.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/pxr/work
)

install(EXPORT ${PROJECT_NAME}
    FILE pxr-work-targets.cmake
    NAMESPACE pxr::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/pxr-work
)
