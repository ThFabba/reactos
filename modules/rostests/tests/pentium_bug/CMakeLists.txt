
add_executable(pentium_bug_test pentium_bug.c)
set_module_type(pentium_bug_test win32cui)
target_link_libraries(pentium_bug_test ${PSEH_LIB})
add_importlibs(pentium_bug_test msvcrt kernel32 ntdll)
