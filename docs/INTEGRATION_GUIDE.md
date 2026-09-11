# Integration Guide

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).
> This guide covers **how to wire the module into your application** — initialization order and calling-context rules. For **adapting the core logic to a different output or use case**, see [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md). For the functions referenced below, see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md).

## Table of Contents

1. [Minimal Integration](#1-minimal-integration)
2. [Calling-Context Rules](#2-calling-context-rules)
3. [Sizing the Ring Buffer for Your Traffic Pattern](#3-sizing-the-ring-buffer-for-your-traffic-pattern)
4. [Handling `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`](#4-handling-rs_bb_uart_tx_status_tx_buffer_full)
5. [Bare-Metal Main-Loop Integration Pattern (Reference)](#5-bare-metal-main-loop-integration-pattern-reference)
6. [Single-Instance Constraint](#6-single-instance-constraint)

---

## 1. Minimal Integration

```c
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>   // pick your board's preset header

/* Configuration structure — must remain valid (static/global storage) for
 * the life of the application; the singleton retains a pointer to it. */
static rs_bb_uart_tx_cfg_t g_bb_uart_tx_cfg;

void app_init(void)
{
    rs_bb_uart_tx_status_t status;

    /* 1. Populate defaults + bind the GPT instance and TX pin */
    status = rs_bb_uart_tx_ConfigGen(&g_bb_uart_tx_cfg, &g_timer0_preset, BSP_IO_PORT_04_PIN_11);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status) { /* handle: NULL pointer, a programming error */ }

    /* 2. Optional: override any ConfigGen() default before ConfigCheck() */
    // g_bb_uart_tx_cfg.baud_rate = 9600U;

    /* 3. Validate AND open — module is READY on success */
    status = rs_bb_uart_tx_ConfigCheck(&g_bb_uart_tx_cfg);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status) { /* handle: bad baud rate, GPT/GPIO HAL failure */ }

    /* 4. From here on, your application calls only: */
    rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\r\n");
}
```

There are no separate `Open()`/`Start()`/`Stop()`/`Close()` public calls — see [BIT_BANGED_UART_TX-USER_MANUAL.md](BIT_BANGED_UART_TX-USER_MANUAL.md#21-design-pattern-single-call-simple-use) for why, and [ARCHITECTURE.md](ARCHITECTURE.md#1-lifecycle-state-machine) for the internal state machine this sequence drives.

---

## 2. Calling-Context Rules

| Function | Valid Calling Context | Notes |
|---|---|---|
| `rs_bb_uart_tx_ConfigGen()` | Startup / main-loop / task context | Not ISR-safe |
| `rs_bb_uart_tx_ConfigCheck()` | Startup / main-loop / task context | Not ISR-safe; call once |
| `rs_bb_uart_tx_ConfigGetSize()` | Any context | Not ISR-safe (no reason to call from an ISR) |
| `rs_bb_uart_tx_Write()` / `WriteString()` | Startup / main-loop / task context **only** | **NOT valid from ISR context in Phase 1.** If your event source is an interrupt, stage the data (e.g. a flag, a small application-owned queue) and call `Write()`/`WriteString()` from the resulting main-loop/task processing, not from the ISR itself |
| `rs_bb_uart_tx_StatusGet()` | Any context | Callable from an ISR for a best-effort diagnostic read, but not guaranteed atomic — do not base control-flow decisions on a value read this way from within the GPT ISR itself |

**Why `Write()`/`WriteString()` are not ISR-safe in Phase 1:** the ring buffer's enqueue side (`head`) is the producer side by design, and the producer is expected to be main-loop/task context — see [ARCHITECTURE.md](ARCHITECTURE.md#5-ring-buffer-concurrency-model). The GPT ISR is the sole *consumer*. If you need to trigger output from an interrupt, have the ISR set a flag or push into your own small application-level structure, and drain that from your main loop by calling `Write()`/`WriteString()` there.

---

## 3. Sizing the Ring Buffer for Your Traffic Pattern

`RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` (default 512, must be a power of two) determines how much data can be queued before a `Write()`/`WriteString()` call returns `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`. Usable capacity is one byte less than the configured size (see [ARCHITECTURE.md](ARCHITECTURE.md#5-ring-buffer-concurrency-model) for why).

Sizing guidance:

- **Larger than your single largest expected `Write()`/`WriteString()` call**, plus enough headroom for whatever else might already be queued and draining. The reference demo's ~300-byte startup banner, sent as one call, is why the default is 512 rather than the design doc's nominal 256 — see [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md#5-ram-consumption).
- **Drain rate is fixed by baud rate**, not by buffer size: at 115200 baud, roughly one byte transmits every ~87 µs (10 bit periods per byte, 8N1). A full 512-byte buffer drains in a little under 45 ms at 115200 baud, and proportionally longer at lower baud rates — plan buffer size and call frequency together if your application transmits continuously.
- Reduce the buffer size (256/128/64) to save RAM in applications with small, infrequent messages and no risk of the reference demo's startup-banner burst pattern.

---

## 4. Handling `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`

Phase 1's buffer-full policy is fixed: **return error, never overwrite, never silently drop, never block.** Your application must decide what to do on `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`:

- **Retry later** (e.g. next main-loop iteration) if the message can wait.
- **Drop and count** if losing an occasional diagnostic line is acceptable — `rs_bb_uart_tx_StatusGet()`'s `buffer_full_count` field already gives you this count for free without extra bookkeeping.
- **Reduce message size or frequency** if buffer-full events are frequent — this usually indicates the buffer is undersized for your traffic pattern relative to the configured baud rate (see [Section 3](#3-sizing-the-ring-buffer-for-your-traffic-pattern)).

There is no blocking-write option in Phase 1 — see [BIT_BANGED_UART_TX-USER_MANUAL.md](BIT_BANGED_UART_TX-USER_MANUAL.md#known-limitations-phase-1-by-design).

---

## 5. Bare-Metal Main-Loop Integration Pattern (Reference)

The reference demo (`rs_bb_uart_tx_demo.c`) demonstrates the simplest integration pattern for a bare-metal (no-RTOS) application:

```c
void rs_bb_uart_tx_demo_run(void)
{
    /* ... ConfigGen() / ConfigCheck() as in Section 1 ... */

    demo_print_banner();   /* single WriteString() call */

    while (1)
    {
        R_BSP_SoftwareDelay(DEMO_HEARTBEAT_PERIOD_MS, BSP_DELAY_UNITS_MILLISECONDS);
        heartbeat_count++;

        if (0U == (heartbeat_count % DEMO_STATUS_EVERY_N_HEARTBEATS))
        {
            demo_print_status(heartbeat_count);   /* StatusGet() + WriteString() */
        }
        else
        {
            demo_print_heartbeat(heartbeat_count); /* WriteString() */
        }
    }
}
```

This pattern generalizes directly: any main-loop/task-context code path that wants to emit output calls `WriteString()`/`Write()` and checks the return status; the GPT ISR handles everything else asynchronously in the background.

---

## 6. Single-Instance Constraint

`rs_bb_uart_tx` is a **singleton**: exactly one GPIO pin, one GPT channel, one instance per application, internally owned as a static control block (`g_bb_uart_tx_ctrl`). There is no caller-managed control block to declare or pass around, and no supported way to run two independent instances in the same application (e.g. two different TX pins at two different baud rates simultaneously) in Phase 1.

If your application genuinely needs two independent software UART outputs, you would need to duplicate and rename the entire `src/rs_bb_uart_tx/` module (a distinct short name, distinct prefix, distinct static control block) — this is an architectural change beyond Phase 1's scope, not a supported configuration option. See [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md) for what does and does not adapt easily.

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md). For adapting the core logic to a different use case, see [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md).*
