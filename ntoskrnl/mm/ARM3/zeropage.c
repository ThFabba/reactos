/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/mm/ARM3/zeropage.c
 * PURPOSE:         ARM Memory Manager Zero Page Thread Support
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

#define MODULE_INVOLVED_IN_ARM3
#include <mm/ARM3/miarm.h>

/* GLOBALS ********************************************************************/

KEVENT MmZeroingPageEvent;

/* PRIVATE FUNCTIONS **********************************************************/

VOID
NTAPI
MiFindInitializationCode(OUT PVOID *StartVa,
OUT PVOID *EndVa);

VOID
NTAPI
MiFreeInitializationCode(IN PVOID StartVa,
IN PVOID EndVa);

VOID
NTAPI
MmZeroPageThread(VOID)
{
    PKTHREAD Thread = KeGetCurrentThread();
    PVOID StartAddress, EndAddress;
    PVOID WaitObjects[2];
    KIRQL OldIrql;
    PVOID ZeroAddress;
    PFN_NUMBER PageIndex, FreePage;
    PMMPFN Pfn1;
    LARGE_INTEGER StartCounter, EndCounter, Frequency;
    ULONG PageCount;

    /* Get the discardable sections to free them */
    MiFindInitializationCode(&StartAddress, &EndAddress);
    if (StartAddress) MiFreeInitializationCode(StartAddress, EndAddress);
    DPRINT("Free non-cache pages: %lx\n", MmAvailablePages + MiMemoryConsumers[MC_CACHE].PagesUsed);

    /* Set our priority to 0 */
    Thread->BasePriority = 0;
    KeSetPriorityThread(Thread, 0);

    /* Setup the wait objects */
    WaitObjects[0] = &MmZeroingPageEvent;
//    WaitObjects[1] = &PoSystemIdleTimer; FIXME: Implement idle timer

    while (TRUE)
    {
        KeWaitForMultipleObjects(1, // 2
                                 WaitObjects,
                                 WaitAny,
                                 WrFreePage,
                                 KernelMode,
                                 FALSE,
                                 NULL,
                                 NULL);
        OldIrql = MiAcquirePfnLock();

        PageCount = 0;
        StartCounter = KeQueryPerformanceCounter(NULL);
        while (TRUE)
        {
            if (!MmFreePageListHead.Total)
            {
                KeClearEvent(&MmZeroingPageEvent);
                MiReleasePfnLock(OldIrql);
                break;
            }

            PageIndex = MmFreePageListHead.Flink;
            ASSERT(PageIndex != LIST_HEAD);
            Pfn1 = MiGetPfnEntry(PageIndex);
            MI_SET_USAGE(MI_USAGE_ZERO_LOOP);
            MI_SET_PROCESS2("Kernel 0 Loop");
            FreePage = MiRemoveAnyPage(MI_GET_PAGE_COLOR(PageIndex));

            /* The first global free page should also be the first on its own list */
            if (FreePage != PageIndex)
            {
                KeBugCheckEx(PFN_LIST_CORRUPT,
                             0x8F,
                             FreePage,
                             PageIndex,
                             0);
            }

            Pfn1->u1.Flink = LIST_HEAD;
            MiReleasePfnLock(OldIrql);

            ZeroAddress = MiMapPagesInZeroSpace(Pfn1, 1);
            ASSERT(ZeroAddress);
            RtlZeroMemory(ZeroAddress, PAGE_SIZE);
            MiUnmapPagesInZeroSpace(ZeroAddress, 1);
            PageCount++;

            OldIrql = MiAcquirePfnLock();

            MiInsertPageInList(&MmZeroedPageListHead, PageIndex);
        }
        if (PageCount > 16)
        {
            EndCounter = KeQueryPerformanceCounter(&Frequency);
            EndCounter.QuadPart -= StartCounter.QuadPart;
            EndCounter.QuadPart *= 1000000000;
            EndCounter.QuadPart /= Frequency.QuadPart;
            DPRINT1("Zeroed %lu pages (%lu MB) in %I64u ns @ %I64u ns/page (total)\n", PageCount, PageCount / (1024 * 1024 / PAGE_SIZE), EndCounter.QuadPart, EndCounter.QuadPart / PageCount);
        }
    }
}

/* EOF */
