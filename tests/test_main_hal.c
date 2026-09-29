#include "unity_fixture.h"

/* Host-only executable (unittest_hal_on_pc): real hal.c + fake FSP layer. */
static void RunAllTests(void)
{
    RUN_TEST_GROUP(BbUartHal);
    RUN_TEST_GROUP(BbUartIsr);
    RUN_TEST_GROUP(BbUartWaveform);
}

int main(int argc, const char * argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
