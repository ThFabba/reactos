
add_definitions(-Dlint)

list(APPEND SOURCE
    cmds.c
    cmdtab.c
    domacro.c
    fake.c
    ftp.c
    main.c
    ruserpass.c
    precomp.h)

add_executable(ftp ${SOURCE} ftp.rc)
set_module_type(ftp win32cui)
target_link_libraries(ftp oldnames)
add_importlibs(ftp ws2_32 iphlpapi msvcrt kernel32 ntdll)
add_pch(ftp precomp.h SOURCE)
add_cd_file(TARGET ftp DESTINATION reactos/system32 FOR all)
