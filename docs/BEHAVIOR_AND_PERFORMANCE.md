# Demonstrated Behavior and Performance

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Functional Behavior Demonstrated](#1-functional-behavior-demonstrated)
2. [Bit-Period Timing Reference](#2-bit-period-timing-reference)
3. [Baud-Rate Accuracy and Jitter](#3-baud-rate-accuracy-and-jitter)
4. [ISR and Enqueue Performance](#4-isr-and-enqueue-performance)
5. [RAM Consumption](#5-ram-consumption)
6. [No Dynamic Allocation, No Floating Point](#6-no-dynamic-allocation-no-floating-point)
7. [On-Board VCOM Baud-Range Caveat](#7-on-board-vcom-baud-range-caveat)

---

## 1. Functional Behavior Demonstrated

- Generated-configuration path (`ConfigGen()` → `ConfigCheck()`) bringing the module from UNCONFIGURED to READY on all three boards
- A single `WriteString()` call transmitting a multi-line startup banner, sent as one call (not several back-to-back calls) so the entire banner is queued before the GPT timer starts — see [Section 4](#4-isr-and-enqueue-performance)
- A bare-metal main-loop heartbeat, printing `[NNNNNN] Heartbeat OK` once per second via `R_BSP_SoftwareDelay()` + `WriteString()`
- A periodic `StatusGet()` snapshot printed every 5th heartbeat instead of the plain heartbeat line, demonstrating `queued`/`free`/`tx`/`ok`/`full` diagnostic counters
- Default 115200 baud, 8N1, LSB-first framing, verified against the on-board J-Link VCOM path with no external adapter required, on all three reference boards
- Non-blocking `Write()`/`WriteString()` behavior: each call returns as soon as data is queued, without waiting for the GPT ISR to drain it

---

## 2. Bit-Period Timing Reference

The nominal bit period is `1 / baud_rate`. Reference values used throughout this module's design and validation:

| Baud Rate | Bit Period |
|---|---|
| 1200 | 833.33 µs |
| 2400 | 416.67 µs |
| 4800 | 208.33 µs |
| 9600 | 104.17 µs |
| 19200 | 52.08 µs |
| 38400 | 26.04 µs |
| 57600 | 17.36 µs |
| 115200 | 8.68 µs |

The actual GPT period register value is derived at `ConfigCheck()` time (inside `rs_bb_uart_tx_hal_gpt_open()`) using integer-only, round-to-nearest arithmetic — never floating point:

```
period_counts = round(clock_frequency / baud_rate) - 1
```

implemented as:

```c
period_counts = (clock_frequency + (baud_rate / 2U)) / baud_rate;
if (period_counts > 0U) { period_counts -= 1U; }
```

`clock_frequency` is queried at runtime from the opened GPT instance via `timer_api_t::infoGet()` — not assumed from a compile-time clock-tree constant — so this same code runs unmodified regardless of which prescaler/divider a given board's GPT configuration uses. Only the *input* clock frequency changes per board; see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) for each board's actual clock tree and divider choice. Rounding to nearest (rather than truncating) minimizes baud-rate error at every supported rate — truncation alone would bias the calculated period consistently short.

| Board | GPT Clock Source | Divider | Effective Clock | Period Counts @ 115200 baud (approx.) |
|---|---|---|---|---|
| FPB-RA2E3 | HOCO 48 MHz → PCLKD | `TIMER_SOURCE_DIV_1` (undivided) | 48 MHz | ~417 |
| FPB-RA4E2 | HOCO/PLL → PCLKD | `TIMER_SOURCE_DIV_4` | ~100 MHz ÷ 4 = ~25 MHz | ~217 |
| FPB-RA6E2 | HOCO 20 MHz × PLL 10.0 → PCLKD/2 | `TIMER_SOURCE_DIV_4` | ~100 MHz ÷ 4 = ~25 MHz | ~217 |

FPB-RA2E3's undivided clock gives it *better* timing resolution than the other two boards at the same baud rate (~417 counts vs. ~217) — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) for why each board's divider choice differs.

---

## 3. Baud-Rate Accuracy and Jitter

**Accuracy requirement:** measured baud-rate error shall not exceed **±3%** of the configured baud rate (`RS_BB_UART_TX_CFG_BAUD_ERROR_PPT_MAX = 30` parts-per-thousand), under idle system conditions, representative bare-metal application load, and representative interrupt load — validated across all three reference boards.

**Jitter guidance:** a recommended design target for bit-to-bit jitter is **≤ ±10%** of the ideal bit period. This is a characterization/architecture-review target, not necessarily a hard pass/fail release gate — finalize acceptance criteria during your own validation planning if jitter is critical to your application.

**Primary jitter risk:** interrupt latency. Mitigations already in the design:
- The GPT ISR (`rs_bb_uart_tx_hal_gpt_callback()`) writes the GPIO pin as its *first* action, before any ring-buffer access — see [ARCHITECTURE.md](ARCHITECTURE.md#3-tx-bit-engine-pipeline-and-data-flow).
- `RS_BB_UART_TX_CFG_GPT_IPL` (default 6 on FPB-RA4E2/FPB-RA6E2; hardcoded to 1 on FPB-RA2E3 — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)) lets you raise the bit-period interrupt's preemption priority relative to other application interrupts if jitter under interrupt load is a concern.

**Recommended measurement equipment:** a logic analyzer or oscilloscope on the TX pin directly gives the most accurate picture of actual bit timing, independent of any USB/VCOM path — see [Section 7](#7-on-board-vcom-baud-range-caveat) for why the VCOM bridge itself is not always representative at the low end of the baud range.

---

## 4. ISR and Enqueue Performance

**`rs_bb_uart_tx_Write()`/`WriteString()` enqueue cost:** a bounded-time, all-or-nothing memory copy into the ring buffer (no division, no critical section — see [ARCHITECTURE.md](ARCHITECTURE.md#5-ring-buffer-concurrency-model)), followed by a GPT-start call only if the module was previously idle. Both calls are non-blocking and return once the data is queued, not once it has transmitted.

**GPT ISR overhead, per bit period:**
1. `rs_bb_uart_tx_engine_output_get()` — O(1), no ring-buffer access.
2. `rs_bb_uart_tx_hal_gpio_write()` — one `R_BSP_PinAccessEnable()`/`PinWrite()`/`PinAccessDisable()` sequence, documented as ISR-safe.
3. `rs_bb_uart_tx_engine_advance()` — prepares the *next* tick's output; dequeues from the ring buffer only when a new byte is needed. This step has a full bit period of slack (as little as ~8.68 µs at 115200 baud), not the sub-microsecond budget of step 2.

No allocation, no blocking, and no application-level callback occurs anywhere in the ISR path. See [ARCHITECTURE.md](ARCHITECTURE.md#3-tx-bit-engine-pipeline-and-data-flow) for the full per-tick sequence and the rationale for splitting the pin write from the ring-buffer access.

**Demo banner send pattern:** the reference demo intentionally builds its ~300-byte startup banner into one local buffer and sends it as a *single* `WriteString()` call, rather than several back-to-back calls. The first `WriteString()` call in a sequence starts the GPT timer; every call after that would enqueue more data while the timer is already running — unnecessary risk for no benefit. A single combined call enqueues everything up front, before the timer starts.

---

## 5. RAM Consumption

**No dynamic memory allocation.** The entire control block (`rs_bb_uart_tx_ctrl_t`, including the ring buffer and TX engine state) is a single static/global instance, `g_bb_uart_tx_ctrl`. Its size is verified at **build time** via `_Static_assert` against `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` (default 2048 bytes) — the build fails rather than silently exceeding budget.

**Default configuration (`RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES = 512`) control-block breakdown, approximate:**

| Component | Approx. Size |
|---|---|
| Ring buffer byte storage (`data[512]`) | 512 bytes |
| Ring buffer `head`/`tail` indices (2 × `volatile uint32_t`) | 8 bytes |
| TX bit-engine state (`state`, `current_byte`, `bit_index`, `next_output_high`) | ~4–8 bytes |
| Diagnostic counters (4 × `uint32_t`) | 16 bytes |
| Version, size, state, `p_cfg` pointer | ~16 bytes |
| **Total (approx.)** | **~560 bytes**, well under the 2048-byte limit |

Query the exact value on your build via `rs_bb_uart_tx_ConfigGetSize()` — it returns `sizeof(rs_bb_uart_tx_ctrl_t)` computed by the compiler for your exact configuration and target ABI, which is authoritative over the estimate above.

**Why the default buffer size is 512, not the design doc's nominal 256 bytes:** the demo application's own startup banner is roughly 304 bytes, sent as a single `WriteString()` call before the GPT timer has drained any of it. Hands-on hardware testing confirmed that 256 bytes causes `StatusGet()`'s `buffer_full_count` to read non-zero (rejected enqueue attempts) during every boot. 512 bytes comfortably accommodates the demo banner with headroom; if your application sends shorter bursts you may safely reduce `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` to 256, 128, or 64 to save RAM — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md).

---

## 6. No Dynamic Allocation, No Floating Point

- **No `malloc`/`free`** anywhere in `rs_bb_uart_tx.c`, `rs_bb_uart_tx_ring_buffer.c`, `rs_bb_uart_tx_engine.c`, or `rs_bb_uart_tx_hal.c`. All storage is static or stack-resident.
- **No floating-point arithmetic.** The GPT period calculation uses integer round-to-nearest division only (see [Section 2](#2-bit-period-timing-reference)).

---

## 7. On-Board VCOM Baud-Range Caveat

`rs_bb_uart_tx_ConfigCheck()` will accept and correctly configure any baud rate in the documented 1200–115200 range on the TX pin itself — the GPT divider math is sized to generate correct signal timing down to 1200 baud regardless of board. However, if you are observing output through the **default on-board J-Link VCOM bridge** in a terminal application (Tera Term, etc.), the *bridge's own firmware* — not this module — may not relay the full range end-to-end. This has been confirmed on **all three reference boards** (FPB-RA2E3, FPB-RA4E2, FPB-RA6E2): the on-board VCOM does not reliably support baud rates below 9600, consistent with all three boards sharing the same on-board J-Link OB debug-probe VCOM firmware family.

To exercise or validate the low end of the baud range (1200–4800) on any board:

- **Use a logic analyzer or oscilloscope directly on the TX pin.** This verifies the module's actual bit timing independent of the VCOM bridge entirely — the only way to observe true 1200 baud signaling through the on-board probe.
- **Use an external TTL-to-USB adapter** wired to a different GPIO, bypassing the on-board VCOM bridge entirely — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-using-an-external-ttl-to-usb-adapter) for wiring.

> This is a sub-9600-baud terminal-observability caveat about the on-board debug probe's VCOM firmware, not a functional limitation of `rs_bb_uart_tx` itself.

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
