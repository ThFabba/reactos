
include_directories(${REACTOS_SOURCE_DIR}/sdk/lib/crt/include)
spec2def(msvcrt.dll msvcrt.spec)

# libmsvcrt should have a few extra objects in it, as defined in msvcrtex
generate_import_lib(libmsvcrt_real msvcrt.dll msvcrt.spec)
# The library needs to have at least one object, so we just add an empty C file
add_library(libmsvcrt empty.c)
add_dependencies(libmsvcrt msvcrtex libmsvcrt_real)
# target_link_libraries on a static library simply propagates the dependency
# to the final non-static target, so doesn't do what we want.
# Adding $<TARGET_FILE:libmsvcrt_real> as an input file to the library seems
# to be ignored by CMake, so we just add it as a command line option (and
# add the above dependency to make sure the file exists). This seems to work
# for both GCC and MSVC
set_property(TARGET libmsvcrt APPEND PROPERTY STATIC_LIBRARY_OPTIONS $<TARGET_FILE:msvcrtex> $<TARGET_FILE:libmsvcrt_real>)
if(MSVC)
    set_target_properties(libmsvcrt PROPERTIES LINKER_LANGUAGE "IMPLIB")
endif()
set_target_properties(libmsvcrt PROPERTIES PREFIX "")
target_compile_definitions(libmsvcrt INTERFACE _DLL __USE_CRTIMP)

add_definitions(
    -DUSE_MSVCRT_PREFIX
    -D_MSVCRT_
    -D_MSVCRT_LIB_
    -D_MT
    -D_CTYPE_DISABLE_MACROS
    -D_NO_INLINING
    -DCRTDLL
    -D__MINGW_IMPORT="")

list(APPEND SOURCE
    dllmain.c
    stubs.c
    msvcrt.rc
    ${CMAKE_CURRENT_BINARY_DIR}/msvcrt_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/msvcrt.def)

add_library(msvcrt MODULE ${SOURCE})
set_module_type(msvcrt win32dll ENTRYPOINT DllMain 12)
add_dependencies(msvcrt psdk)
target_link_libraries(msvcrt crt wine ${PSEH_LIB})

if(MSVC)
    # export of deleting destructor "name"
    target_link_options(msvcrt PRIVATE "/ignore:4102")
    set_property(TARGET libmsvcrt_real APPEND PROPERTY STATIC_LIBRARY_OPTIONS "/ignore:4102")
endif()

add_importlibs(msvcrt kernel32 ntdll)
add_cd_file(TARGET msvcrt DESTINATION reactos/system32 FOR all)
