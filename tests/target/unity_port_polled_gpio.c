/***********************************************************************************************************************
 * @file unity_port_polled_gpio.c
 * @brief Unity output transport for the Bit-Banged UART TX firmware test build.
 *
 * Implements unity_putc() as a tiny BLOCKING, polled 8N1 software UART on the
 * board's VCOM TXD pin (the same pin the SBB bit-bangs in the application).
 *
 * Why not the SCI UART the RTC reference project uses?
 *   The Bit-Banged UART example projects configure only r_gpt + r_ioport: no
 *   SCI stack, no RTT.  The VCOM TXD pin is owned by the SBB itself, which is
 *   exactly what is NOT running in this image — the on-target unit tests link
 *   the mock HAL (see tests/test_rs_bb_uart_tx_core.c), so the pin is free and
 *   nothing else needs to be configured in configuration.xml.
 *
 * Timing (why SysTick, not R_BSP_SoftwareDelay)
 *   R_BSP_SoftwareDelay() is unsuitable for bit timing: it computes
 *   ns_per_cycle with INTEGER division (1e9/48e6 = 20, real 20.83 -> every
 *   delay ~4 % long) and documents a 75-200 cycle call overhead on Cortex-M23.
 *   Together that made each bit ~6-8 % too long, which corrupts frames.
 *
 *   Instead every bit is scheduled against ABSOLUTE deadlines measured from the
 *   start of the frame on the free-running SysTick down-counter (core clock,
 *   no interrupt): bit i starts at  i * SystemCoreClock / 9600  ticks after the
 *   start bit.  Call overhead and rounding therefore cannot accumulate.  This
 *   is the same absolute-deadline scheduling the jitter specification
 *   (tests/Bit_Banged_UART_TX_Jitter_Specification.txt) recommends.
 *
 * Baud: 9600.  115200 is deliberately not used here: at 8.68 us/bit a polled
 * loop's per-bit latency would be a large fraction of the bit.
 *
 * PFS write protection: R_BSP_PinWrite() needs R_BSP_PinAccessEnable() around
 * it (see rs_bb_uart_tx_hal.c).  Access is enabled ONCE per character rather
 * than once per bit to keep per-bit latency negligible.
 *
 * Line-endings: '\n' is sent as "\r\n" so terminals need no special setting.
 *
 * Build: the pin is supplied by firmware/meson.build as -DUNITY_PORT_TX_PIN=...
 **********************************************************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "bsp_api.h"
#include "r_ioport.h"

#ifndef UNITY_PORT_TX_PIN
 #error "UNITY_PORT_TX_PIN must be defined (e.g. -DUNITY_PORT_TX_PIN=BSP_IO_PORT_01_PIN_09)"
#endif

#define UNITY_PORT_BAUD        (9600U)
#define UNITY_PORT_FRAME_BITS  (10U)              /* start + 8 data + stop */
#define UNITY_PORT_TICK_MASK   (0x00FFFFFFUL)     /* SysTick is a 24-bit counter */
#define UNITY_PORT_IDLE_MS     (5U)               /* line held idle-high after init, before 1st frame */

/* Start of bit i, in SysTick ticks after the start of the frame.  Index 10 is
 * the END of the stop bit.  Computed once in 32-bit arithmetic:
 * 10 * SystemCoreClock must fit in 32 bits (true up to ~429 MHz). */
static uint32_t g_bit_start_ticks[UNITY_PORT_FRAME_BITS + 1U];
static uint32_t g_idle_ticks;
static bool     g_port_initialized = false;

static inline uint32_t port_elapsed(uint32_t start)
{
    /* SysTick counts DOWN; wrap-safe elapsed ticks since 'start'. */
    return (start - SysTick->VAL) & UNITY_PORT_TICK_MASK;
}

static void port_wait_ticks(uint32_t ticks)
{
    uint32_t const start = SysTick->VAL;

    while (port_elapsed(start) < ticks)
    {
        /* spin */
    }
}

static void port_init_once(void)
{
    if (g_port_initialized)
    {
        return;
    }

    /* Free-running 24-bit down-counter on the core clock, interrupt disabled. */
    SysTick->CTRL = 0U;
    SysTick->LOAD = UNITY_PORT_TICK_MASK;
    SysTick->VAL  = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    uint32_t const clk = SystemCoreClock;
    for (uint32_t i = 0U; i <= UNITY_PORT_FRAME_BITS; i++)
    {
        g_bit_start_ticks[i] = ((i * clk) + (UNITY_PORT_BAUD / 2U)) / UNITY_PORT_BAUD;   /* rounded */
    }
    g_idle_ticks = (clk / 1000U) * UNITY_PORT_IDLE_MS;

    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(UNITY_PORT_TX_PIN,
                 ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinAccessDisable();

    /* Hold the line idle-high long enough for the USB-serial bridge to see a
     * clean idle before the first start bit (the pin floats during reset). */
    port_wait_ticks(g_idle_ticks);

    g_port_initialized = true;
}

static void port_send_frame(uint8_t byte)
{
    /* 10-bit frame, LSB first: start(0), D0..D7, stop(1) */
    uint32_t const frame = 0x200U | ((uint32_t) byte << 1);

    R_BSP_PinAccessEnable();

    uint32_t const t0 = SysTick->VAL;
    for (uint32_t i = 0U; i < UNITY_PORT_FRAME_BITS; i++)
    {
        while (port_elapsed(t0) < g_bit_start_ticks[i])
        {
            /* spin until this bit's absolute start time */
        }
        R_BSP_PinWrite(UNITY_PORT_TX_PIN, (0U != ((frame >> i) & 1U)) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW);
    }
    while (port_elapsed(t0) < g_bit_start_ticks[UNITY_PORT_FRAME_BITS])
    {
        /* stop bit lasts a full bit period */
    }

    R_BSP_PinAccessDisable();
}

void unity_putc(int c)
{
    port_init_once();

    if ('\n' == c)
    {
        port_send_frame((uint8_t) '\r');
    }
    port_send_frame((uint8_t) c);
}
