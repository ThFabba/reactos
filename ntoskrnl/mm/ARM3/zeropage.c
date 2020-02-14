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
    ULONG Color, StartColor;
    ULONG MaxPagesToZero, PagesToZero;
    PMMPFN Pfn1;
    PMMPFN FirstPfn;
    LARGE_INTEGER StartCounter, EndCounter, Frequency;
    LARGE_INTEGER StartZero, EndZero, TotalZero;
    ULONG PageCount;

    /* Get the discardable sections to free them */
    MiFindInitializationCode(&StartAddress, &EndAddress);
    if (StartAddress) MiFreeInitializationCode(StartAddress, EndAddress);
    DPRINT("Free non-cache pages: %lx\n", MmAvailablePages + MiMemoryConsumers[MC_CACHE].PagesUsed);

    OldIrql = MiAcquirePfnLock();
    ASSERT(MmSecondaryColors == MmSecondaryColorMask + 1);
    MaxPagesToZero = min(MmSecondaryColors, 32);
    if (MmResidentAvailablePages - MmSystemLockPagesCount > MaxPagesToZero)
    {
        //InterlockedExchangeAddSizeT(&MmResidentAvailablePages, -MaxPagesToZero);
    }
    else
    {
        MaxPagesToZero = 1;
    }
    DPRINT1("Max pages to zero: %lu (resident available %lu, lock %lu)\n", MaxPagesToZero, MmResidentAvailablePages, MmSystemLockPagesCount);
    MiReleasePfnLock(OldIrql);

    /* Set our priority to 0 */
    Thread->BasePriority = 0;
    KeSetPriorityThread(Thread, 0);

    /* Setup the wait objects */
    WaitObjects[0] = &MmZeroingPageEvent;
//    WaitObjects[1] = &PoSystemIdleTimer; FIXME: Implement idle timer

    Color = 0;

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
        TotalZero.QuadPart = 0;
        StartCounter = KeQueryPerformanceCounter(NULL);
        while (TRUE)
        {
            if (!MmFreePageListHead.Total)
            {
                KeClearEvent(&MmZeroingPageEvent);
                MiReleasePfnLock(OldIrql);
                break;
            }

            FirstPfn = (PMMPFN)LIST_HEAD;
            StartColor = Color;
            PagesToZero = 0;
            do
            {
                PageIndex = MmFreePagesByColor[FreePageList][Color].Flink;
                if (PageIndex != LIST_HEAD)
                {
                    Pfn1 = MiGetPfnEntry(PageIndex);
                    if (Pfn1->u3.e1.PageLocation != FreePageList ||
                        Pfn1->u3.e2.ReferenceCount != 0)
                    {
                        KeBugCheckEx(PFN_LIST_CORRUPT,
                                     0x8D,
                                     PageIndex,
                                     Pfn1->u3.e2.ReferenceCount | (Pfn1->u3.e2.ShortFlags << 16),
                                     (ULONG_PTR)Pfn1->PteAddress);
                    }

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

                    Pfn1->u1.Flink = (ULONG_PTR)FirstPfn;
                    ++PagesToZero;
                    Pfn1->u3.e1.PageLocation = BadPageList;
                    FirstPfn = Pfn1;
                }
                Color = MI_GET_PAGE_COLOR(Color + 1) | (Color & ~MmSecondaryColorMask);
            } while (PagesToZero != MaxPagesToZero && Color != StartColor);
            ASSERT(FirstPfn != (PMMPFN)LIST_HEAD);

            //Pfn1->u1.Flink = LIST_HEAD;
            MiReleasePfnLock(OldIrql);

            ZeroAddress = MiMapPagesInZeroSpace(FirstPfn, PagesToZero);
            ASSERT(ZeroAddress);
            StartZero = KeQueryPerformanceCounter(NULL);
            RtlFillMemoryUlong(ZeroAddress, PagesToZero << PAGE_SHIFT, 0);
            EndZero = KeQueryPerformanceCounter(NULL);
            TotalZero.QuadPart += EndZero.QuadPart - StartZero.QuadPart;
            MiUnmapPagesInZeroSpace(ZeroAddress, PagesToZero);
            PageCount += PagesToZero;

            OldIrql = MiAcquirePfnLock();

            do
            {
                PageIndex = MiGetPfnEntryIndex(FirstPfn);
                FirstPfn = (PMMPFN)FirstPfn->u1.Flink;
                MiInsertPageInList(&MmZeroedPageListHead, PageIndex);
            } while (FirstPfn != (PMMPFN)LIST_HEAD);
        }
        if (PageCount > 16)
        {
            EndCounter = KeQueryPerformanceCounter(&Frequency);
            EndCounter.QuadPart -= StartCounter.QuadPart;
            EndCounter.QuadPart *= 1000000000;
            EndCounter.QuadPart /= Frequency.QuadPart;
            TotalZero.QuadPart *= 1000000000;
            TotalZero.QuadPart /= Frequency.QuadPart;
            DPRINT1("Zeroed %lu pages (%lu MB) in %I64u ns (%I64u for zero) @ %I64u ns/page (total)\n", PageCount, PageCount / (1024 * 1024 / PAGE_SIZE), EndCounter.QuadPart, TotalZero.QuadPart, EndCounter.QuadPart / PageCount);
        }
    }
}

/* EOF */
