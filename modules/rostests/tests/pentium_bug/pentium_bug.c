#include <stdio.h>
#include <pseh/pseh2.h>

int main(void)
{
    _SEH2_TRY
    {
        /* LOCK CMPXCHG8B EAX */
#ifdef _MSC_VER
        __asm _emit 0F0h
        __asm _emit 00Fh
        __asm _emit 0C7h
        __asm _emit 0C8h
#else
        __asm__ __volatile__(".byte 0xf0, 0x0f, 0xc7, 0xc8")
#endif
        printf("No exception\n");
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        printf("Exception: 0x%lx\n", _SEH2_GetExceptionCode());
    }
    _SEH2_END;

    return 0;
}
