/* $Id: dllmain.c,v 1.26 2002/07/04 19:56:37 dwelch Exp $
 * 
 *  Entry Point for win32k.sys
 */

#undef WIN32_LEAN_AND_MEAN
#define WIN32_NO_STATUS
#include <windows.h>
#include <ddk/ntddk.h>
#include <ddk/winddi.h>
#include <ddk/service.h>

#include <napi/win32.h>
#include <win32k/win32k.h>

#include <include/winsta.h>
#include <include/class.h>
#include <include/window.h>

extern SSDT Win32kSSDT[];
extern SSPT Win32kSSPT[];
extern ULONG Win32kNumberOfSysCalls;

/*
 * This definition doesn't work
 */
// WINBOOL STDCALL DllMain(VOID)
NTSTATUS
STDCALL
DllMain (
  IN	PDRIVER_OBJECT	DriverObject,
  IN	PUNICODE_STRING	RegistryPath)
{
  NTSTATUS Status;
  BOOLEAN Result;

  /*
   * Register user mode call interface
   * (system service table index = 1)
   */
  Result = KeAddSystemServiceTable (Win32kSSDT, NULL,
				    Win32kNumberOfSysCalls, Win32kSSPT, 1);
  if (Result == FALSE)
  {
    DbgPrint("Adding system services failed!\n");
    return STATUS_UNSUCCESSFUL;
  }

  /*
   * Register our per-process and per-thread structures.
   */
  PsEstablishWin32Callouts(0, 0, 0, 0, sizeof(W32THREAD), sizeof(W32PROCESS));

  Status = InitWindowStationImpl();
  if (!NT_SUCCESS(Status))
  {
    DbgPrint("Failed to initialize window station implementation!\n");
    return STATUS_UNSUCCESSFUL;
  }

  Status = InitClassImpl();
  if (!NT_SUCCESS(Status))
  {
    DbgPrint("Failed to initialize window class implementation!\n");
    return STATUS_UNSUCCESSFUL;
  }

  Status = InitWindowImpl();
  if (!NT_SUCCESS(Status))
  {
    DbgPrint("Failed to initialize window implementation!\n");
    return STATUS_UNSUCCESSFUL;
  }

  Status = InitInputImpl();
  if (!NT_SUCCESS(Status))
    {
      DbgPrint("Failed to initialize input implementation.\n");
      return(Status);
    }

  Status = MsqInitializeImpl();
  if (!NT_SUCCESS(Status))
    {
      DbgPrint("Failed to initialize message queue implementation.\n");
      return(Status);
    }

  return STATUS_SUCCESS;
}


BOOLEAN
STDCALL
W32kInitialize (VOID)
{
  InitGdiObjectHandleTable ();

  // Create surface used to draw the internal font onto
  CreateCellCharSurface();

  // Create stock objects, ie. precreated objects commonly used by win32 applications
  CreateStockObjects();

  // Initialize FreeType library
  if(!InitFontSupport()) return FALSE;

  return TRUE;
}

/* EOF */
