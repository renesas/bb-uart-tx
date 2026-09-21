#include "unity_fixture.h"

static void RunAllTests(void)
{
    RUN_TEST_GROUP(BbUartRingBuffer);
    RUN_TEST_GROUP(BbUartEngine);
    RUN_TEST_GROUP(BbUartEngineFrame);
    RUN_TEST_GROUP(BbUartConfig);
    RUN_TEST_GROUP(BbUartLifecycle);
    RUN_TEST_GROUP(BbUartWrite);
}

int main(int argc, const char * argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
