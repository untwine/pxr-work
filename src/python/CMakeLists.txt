add_library(pyWork SHARED
    module.cpp
    wrapThreadLimits.cpp
)

set_target_properties(pyWork
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME _work
        RUNTIME_OUTPUT_DIRECTORY "$<CONFIG>/pxr/Work"
        LIBRARY_OUTPUT_DIRECTORY "$<CONFIG>/pxr/Work"
        ARCHIVE_OUTPUT_DIRECTORY "$<CONFIG>/pxr/Work"
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(pyWork
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(pyWork
    PUBLIC
        work
        pxr::boost-python
)

if(SKBUILD AND NOT WIN32)
    set(_rpath_prefix "$ORIGIN")
    if(APPLE)
        set(_rpath_prefix "@loader_path")
    endif()
    set_target_properties(pyWork
        PROPERTIES
            INSTALL_RPATH "${_rpath_prefix}/../.libs"
            INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()

if(WIN32)
    set_target_properties(pyWork PROPERTIES SUFFIX ".pyd")
elseif(APPLE)
    set_target_properties(pyWork PROPERTIES SUFFIX ".so")
endif()

target_compile_definitions(
    pyWork
    PRIVATE
        BOOST_BIND_GLOBAL_PLACEHOLDERS
        BOOST_PYTHON_NO_PY_SIGNATURES
        MFB_PACKAGE_MODULE=Work
        MFB_PACKAGE_NAME=work
        MFB_ALT_PACKAGE_NAME=work
)

file(
    GENERATE
    OUTPUT "$<TARGET_FILE_DIR:pyWork>/__init__.py"
    INPUT "__init__.py"
)

install(
    TARGETS pyWork EXPORT ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PYTHON_LIBDIR}/pxr/Work
    RUNTIME DESTINATION ${CMAKE_INSTALL_PYTHON_LIBDIR}/pxr/Work
)
install(
    FILES __init__.py
    DESTINATION ${CMAKE_INSTALL_PYTHON_LIBDIR}/pxr/Work
)
