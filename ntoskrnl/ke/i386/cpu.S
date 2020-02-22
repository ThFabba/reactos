/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:
 * COPYRIGHT:   Copyright 2020 Thomas Faber (thomas.faber@reactos.org)
 */

/* INCLUDES ******************************************************************/

#include <asm.inc>
#include <ks386.inc>

/* FUNCTIONS ****************************************************************/
.code

/*++
 * KiXMMIZeroPagesNoSave
 *
 * \brief
 *     Zero one or more pages using SSE instructions.
 *
 * VOID FASTCALL KiXMMIZeroPagesNoSave(PVOID Address, SIZE_T Size);
 *
 * \param Address
 *     Pointer to the pages to zero (page-aligned).
 *
 * \param Size
 *     Number of bytes to zero (a multiple of PAGE_SIZE).
 *
 *--*/
ALIGN 16
PUBLIC @KiXMMIZeroPagesNoSave@8
.PROC @KiXMMIZeroPagesNoSave@8
    FPO 0, 0, 0, 0, 0, FRAME_FPO

    /* We'll write 16 bytes at a time */
    shr edx, 4
    xorps xmm0, xmm0

    /* The non-temporal store will avoid caching this memory unnecessarily.
     * MOVNTPS is the only 16 byte NT move available with regular SSE.
     */
l:
    movntps [ecx], xmm0
    add ecx, 16
    dec edx
    jnz l

#ifdef CONFIG_SMP
    /* Guarantee cache coherence between multiple CPUs */
    sfence
#endif

    ret

.ENDP

END
