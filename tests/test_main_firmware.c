/***********************************************************************************************************************
 * On-target Unity firmware entry point.
 *
 * This file replaces the RASC-generated ra_gen/main.c in the unit-test firmware
 * build (which would call hal_entry() and run the demo).  It is linked instead
 * of main.c so the BSP still starts normally (Reset_Handler -> SystemInit ->
 * main) but main() here runs the Unity groups.
 *
 * Only the groups that run on target are listed: the pure-logic groups and the
 * core groups (mock HAL).  BbUartHal / BbUartIsr / BbUartWaveform are host-only
 * (they need the fake R_BSP_Pin*) and are in tests/test_main_hal.c.
 *
 * Unity output is routed to the VCOM TXD pin by unity_putc() in
 * tests/target/unity_port_polled_gpio.c (9600 baud).
 *
 * The whole suite is re-run every RERUN_PAUSE_S seconds, forever.  The image
 * starts running the moment it is flashed (rfp-cli -run), usually before a
 * serial terminal is attached; a single run would already be over by then.
 * Repeating means the next complete run is always seen, whenever you connect.
 **********************************************************************************************************************/

#include "bsp_api.h"
#include "unity_fixture.h"

#define RERUN_PAUSE_S   (3U)

static void RunAllTests(void)
{
    RUN_TEST_GROUP(BbUartRingBuffer);
    RUN_TEST_GROUP(BbUartEngine);
    RUN_TEST_GROUP(BbUartEngineFrame);
    RUN_TEST_GROUP(BbUartConfig);
    RUN_TEST_GROUP(BbUartLifecycle);
    RUN_TEST_GROUP(BbUartWrite);
}

/* Argv passed to UnityMain — "-v" enables verbose per-test output over VCOM. */
static const char * run_argv[] = {
    __FILE__,
    "-v"
};

int main(void)
{
    for (;;)
    {
        UnityPrint("===== UNITY LOG BEGIN ======"); UNITY_PRINT_EOL();
        (void) UnityMain(2, run_argv, RunAllTests);
        UnityPrint("===== UNITY LOG END ======"); UNITY_PRINT_EOL();

        R_BSP_SoftwareDelay(RERUN_PAUSE_S, BSP_DELAY_UNITS_SECONDS);   /* accuracy irrelevant here */
    }
}
