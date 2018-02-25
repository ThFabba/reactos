#define STANDALONE
#include <apitest.h>

extern void func_info(void);
extern void func_send(void);

const struct test winetest_testlist[] =
{
    { "info", func_info },
    { "send", func_send },
    { 0, 0 }
};
