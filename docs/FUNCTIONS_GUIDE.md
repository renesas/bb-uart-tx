# Functions Guide — Public and Internal Functions

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).
> For call sequences and how these functions fit together in an application, see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md).

This guide documents every function in `rs_bb_uart_tx` — both the **public** functions an application calls directly, and the **internal/private** functions used only inside the module. Each function is explicitly tagged:

| Tag | Meaning |
|---|---|
| 🟢 **PUBLIC** | Declared in `rs_bb_uart_tx.h`. Part of the supported application-facing interface. Naming: `rs_bb_uart_tx_OperationName()`. |
| 🔒 **INTERNAL** | Non-`static` linkage but declared in an internal header (`rs_bb_uart_tx_ctrl.h`, `rs_bb_uart_tx_hal.h`, `rs_bb_uart_tx_engine.h`, `rs_bb_uart_tx_ring_buffer.h`) explicitly marked "not part of the public API." Callable from other files inside the module, but not intended for application use. |
| ⚫ **PRIVATE** | `static` — file-scoped, callable only from within the `.c` file that defines it. |
| 🟢🔒 **PUBLIC BRIDGE** | Declared in `rs_bb_uart_tx.h` (so it is application-facing) but **defined** in the board-specific configuration file, alongside the board's preset GPT instance data. |

Only 🟢 **PUBLIC** and 🟢🔒 **PUBLIC BRIDGE** functions are safe to call from application code. 🔒 **INTERNAL** and ⚫ **PRIVATE** functions are documented here for engineers reading, debugging, or extending the module's source — calling them directly from application code voids the state-gating and calling-context guarantees described for the public functions.

---

## Table of Contents

1. [Start Here: The Minimal Call Sequence](#1-start-here-the-minimal-call-sequence)
2. [Configuration Bridge](#2-configuration-bridge)
3. [Operational Functions](#3-operational-functions)
4. [Query Functions](#4-query-functions)
5. [Complete Public Function Reference Table](#5-complete-public-function-reference-table)
6. [Internal Functions — `rs_bb_uart_tx.c`](#6-internal-functions--rs_bb_uart_txc)
7. [Internal Functions — `rs_bb_uart_tx_ring_buffer.c` / `.h`](#7-internal-functions--rs_bb_uart_tx_ring_bufferc--h)
8. [Internal Functions — `rs_bb_uart_tx_engine.c` / `.h`](#8-internal-functions--rs_bb_uart_tx_enginec--h)
9. [Internal Functions — `rs_bb_uart_tx_hal.c` / `.h`](#9-internal-functions--rs_bb_uart_tx_halc--h)
10. [Complete Internal Function Reference Table](#10-complete-internal-function-reference-table)

---

# Public Functions

## 1. Start Here: The Minimal Call Sequence

```c
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>   // pick your board

rs_bb_uart_tx_cfg_t    g_cfg;
rs_bb_uart_tx_status_t status;

status = rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0_preset, BSP_IO_PORT_04_PIN_11);
status = rs_bb_uart_tx_ConfigCheck(&g_cfg);   // validates AND opens on success
if (RS_BB_UART_TX_STATUS_SUCCESS == status)
{
    (void) rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\r\n");
}
```

There is no separate `Open()`/`Start()` call — see [BIT_BANGED_UART_TX-USER_MANUAL.md](BIT_BANGED_UART_TX-USER_MANUAL.md#21-design-pattern-single-call-simple-use) for why `ConfigCheck()` performs double duty.

---

## 2. Configuration Bridge

`rs_bb_uart_tx_ConfigGen()` and `rs_bb_uart_tx_ConfigCheck()` are declared in `rs_bb_uart_tx.h` but **defined** in the board-specific pre-set configuration source file, alongside the `*_preset` GPT instance data they are most commonly paired with:

```c
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.h>   // FPB-RA2E3
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>   // FPB-RA4E2
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra6e2.h>   // FPB-RA6E2
```

They are defined **unconditionally**, regardless of `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE`, so the same functions remain available whether the caller binds the board's `*_preset` instance or an FSP Stack Configurator-generated instance (e.g. `g_timer0`).

### 🟢🔒 `rs_bb_uart_tx_ConfigGen()` — PUBLIC BRIDGE

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGen(rs_bb_uart_tx_cfg_t     * p_cfg,
                                                timer_instance_t const * p_gpt_instance,
                                                bsp_io_port_pin_t         tx_pin);
```

Assigns the GPT HAL instance and TX pin to a configuration structure, and fills in Phase 1 defaults for every other field: `RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE` (115200), `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` (512), 8N1, and `RS_BB_UART_TX_CFG_GPT_IPL`. Override any of these on `*p_cfg` after this call returns, before calling `ConfigCheck()`.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Configuration populated |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_cfg` or `p_gpt_instance` is `NULL` |

*Callable from any context before `ConfigCheck()`. Not ISR-safe.*

### 🟢🔒 `rs_bb_uart_tx_ConfigCheck()` — PUBLIC BRIDGE

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigCheck(rs_bb_uart_tx_cfg_t const * p_cfg);
```

Validates that the baud rate is within the supported range, the GPT instance pointer is present, and the buffer-size field matches the compiled buffer size. On success, **also** commits the configuration and opens the GPT and GPIO HAL resources (see [BIT_BANGED_UART_TX-USER_MANUAL.md](BIT_BANGED_UART_TX-USER_MANUAL.md#21-design-pattern-single-call-simple-use)).

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Configuration valid; module is now READY |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_cfg` or `p_cfg->p_gpt_instance` is `NULL` |
| `RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE` | `baud_rate` outside `RS_BB_UART_TX_CFG_MIN/MAX_BAUD_RATE` |
| `RS_BB_UART_TX_STATUS_INVALID_BUFFER_SIZE` | `p_cfg->buffer_size` does not match the compiled `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Module is already READY/TRANSMITTING/ERROR (not idempotent) |
| `RS_BB_UART_TX_STATUS_TIMER_ERROR` | GPT HAL open/period-set failed |
| `RS_BB_UART_TX_STATUS_GPIO_ERROR` | GPIO pin configuration failed |

*Valid once, before the first `Write()`/`WriteString()` call. Not ISR-safe.*

### 🟢 `rs_bb_uart_tx_ConfigGetSize()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGetSize(size_t * p_size);
```

Returns the size, in bytes, of the internal control block. Useful for confirming control-block sizing (e.g. against `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES`) without requiring `ConfigCheck()` to have run first. Since the module is a singleton, no control-block pointer argument is needed.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Size returned in `*p_size` |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_size` is `NULL` |

*Valid in any lifecycle state, including before `ConfigGen()`. Not ISR-safe.*

---

## 3. Operational Functions

### 🟢 `rs_bb_uart_tx_Write()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_Write(uint8_t const * p_data, uint32_t length);
```

Enqueues raw bytes for transmission. Non-blocking; returns immediately. Copies bytes into the ring buffer using an all-or-nothing policy — if the full request does not fit, nothing is written and `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` is returned. Starts (or continues) asynchronous transmission via the GPT timer if data is queued. `length == 0` is a no-op success regardless of `p_data`.

**Success means the data was accepted into the transmit queue — not that it has finished transmitting on the TX pin.**

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | All bytes accepted into the queue (or `length` was 0) |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_data` is `NULL` and `length > 0` |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Module is not READY/TRANSMITTING |
| `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` | Insufficient free space for the whole request |
| `RS_BB_UART_TX_STATUS_TIMER_ERROR` | Propagated if the GPT timer failed to start |

*Valid from startup context (after `ConfigCheck()` succeeds) and bare-metal main-loop/task context. **NOT** valid from ISR context in Phase 1.*

### 🟢 `rs_bb_uart_tx_WriteString()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_WriteString(char const * p_str);
```

Enqueues a `NUL`-terminated ASCII string. Calculates the length via `strlen()`, then delegates to `rs_bb_uart_tx_Write()` — same all-or-nothing policy, same calling-context restriction. The terminating `NUL` is never transmitted.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | All characters accepted into the queue |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_str` is `NULL` |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Propagated from `Write()` |
| `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` | Propagated from `Write()` |

*Same calling-context restriction as `rs_bb_uart_tx_Write()`.*

---

## 4. Query Functions

### 🟢 `rs_bb_uart_tx_StatusGet()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_StatusGet(rs_bb_uart_tx_state_t * p_state);
```

Populates a caller-allocated snapshot of runtime status: lifecycle state, configured baud rate, ring-buffer occupancy (`queued_bytes`/`free_bytes`), and four cumulative diagnostic counters (`total_bytes_accepted`, `total_bytes_transmitted`, `buffer_full_count`, `invalid_call_count`).

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Status populated |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_state` is `NULL` |

*Valid in any lifecycle state, including before `ConfigCheck()` (fields read as zero/UNCONFIGURED). Callable from main-loop/task context; not guaranteed ISR-safe for control-flow decisions — it reads multiple fields without a critical section, which is acceptable for a diagnostics snapshot but not for a decision that must be atomic with respect to the GPT ISR.*

---

## 5. Complete Public Function Reference Table

| Function | Category | Blocking? | Possible Return Values | Primary User |
|---|---|---|---|---|
| `rs_bb_uart_tx_ConfigGen()` | Configuration | No | `SUCCESS`, `INVALID_POINTER` | Application/example setup code |
| `rs_bb_uart_tx_ConfigCheck()` | Configuration | No | `SUCCESS`, `INVALID_POINTER`, `UNSUPPORTED_BAUD_RATE`, `INVALID_BUFFER_SIZE`, `INVALID_STATE`, `TIMER_ERROR`, `GPIO_ERROR` | Application/example setup code |
| `rs_bb_uart_tx_ConfigGetSize()` | Configuration | No | `SUCCESS`, `INVALID_POINTER` | Tooling, diagnostics |
| `rs_bb_uart_tx_Write()` | Operation | No | `SUCCESS`, `INVALID_POINTER`, `INVALID_STATE`, `TX_BUFFER_FULL`, `TIMER_ERROR` | Application |
| `rs_bb_uart_tx_WriteString()` | Operation | No | `SUCCESS`, `INVALID_POINTER`, `INVALID_STATE`, `TX_BUFFER_FULL` | Application |
| `rs_bb_uart_tx_StatusGet()` | Query | No | `SUCCESS`, `INVALID_POINTER` | Application, diagnostics |

The names above omit the common `RS_BB_UART_TX_STATUS_` prefix for readability.

---

# Internal Functions

## 6. Internal Functions — `rs_bb_uart_tx.c`

### 🔒 `rs_bb_uart_tx_open_internal()` — INTERNAL

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_open_internal(rs_bb_uart_tx_cfg_t const * p_cfg);
```

Declared in `rs_bb_uart_tx_ctrl.h`, defined in `rs_bb_uart_tx.c`. Binds `p_cfg` into the singleton control block, resets the ring buffer and TX bit-engine, opens the GPIO TX pin and GPT timer HAL resources, and transitions the lifecycle state UNCONFIGURED/CONFIGURED → READY. Called exclusively from the board configuration file's `rs_bb_uart_tx_ConfigCheck()` implementation on successful validation.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Opened; state → READY |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Already READY/TRANSMITTING/ERROR |
| `RS_BB_UART_TX_STATUS_GPIO_ERROR` | GPIO pin configuration failed; state → ERROR |
| `RS_BB_UART_TX_STATUS_TIMER_ERROR` | GPT HAL open/period-set failed; state → ERROR |

### ⚫ `rs_bb_uart_tx_start_internal()` — PRIVATE

```c
static rs_bb_uart_tx_status_t rs_bb_uart_tx_start_internal(rs_bb_uart_tx_ctrl_t * p_ctrl);
```

Starts (or continues) asynchronous transmission. Called from `Write()`/`WriteString()` after a successful enqueue. A no-op returning `SUCCESS` if already TRANSMITTING (the GPT ISR will naturally pick up newly-queued bytes); otherwise starts the GPT timer and transitions to TRANSMITTING.

---

## 7. Internal Functions — `rs_bb_uart_tx_ring_buffer.c` / `.h`

`rs_bb_uart_tx_ring_buffer.h` is explicitly marked *"Internal header — not part of the public API. Not included by `rs_bb_uart_tx.h`."* All five functions are 🔒 **INTERNAL**.

| Function | Purpose | Calling Context |
|---|---|---|
| `rs_bb_uart_tx_ring_buffer_reset(p_rb)` | Resets a ring buffer to the empty state. Called once from `rs_bb_uart_tx_open_internal()` | Before the GPT ISR is enabled only — not ISR-safe by itself |
| `rs_bb_uart_tx_ring_buffer_enqueue(p_rb, p_data, length)` | All-or-nothing enqueue; lock-free (no critical section — see [ARCHITECTURE.md](ARCHITECTURE.md#5-ring-buffer-concurrency-model)) | Main-loop/task context only — matches `Write()`'s restriction |
| `rs_bb_uart_tx_ring_buffer_dequeue(p_rb, p_byte)` | Dequeue a single byte; lock-free, O(1), no division | GPT ISR context only, via the TX engine |
| `rs_bb_uart_tx_ring_buffer_count_get(p_rb)` | Bytes currently queued (single volatile read) | Any context (diagnostics snapshot) |
| `rs_bb_uart_tx_ring_buffer_free_get(p_rb)` | Bytes of free space remaining | Any context (diagnostics snapshot) |

---

## 8. Internal Functions — `rs_bb_uart_tx_engine.c` / `.h`

`rs_bb_uart_tx_engine.h` is explicitly marked *"Internal header — not part of the public API."* All three functions are 🔒 **INTERNAL**. This file never includes FSP headers — it calls back into the HAL shim to actually drive the GPIO pin, so it is unit-testable on the host.

| Function | Purpose | Calling Context |
|---|---|---|
| `rs_bb_uart_tx_engine_reset(p_engine)` | Resets the TX bit-engine to `TX_IDLE`, idle-high output pending. Called once from `rs_bb_uart_tx_open_internal()` | Before the GPT ISR is enabled |
| `rs_bb_uart_tx_engine_output_get(p_engine)` | Returns the pin level for the tick about to run. O(1), no ring-buffer access — call FIRST in the ISR | GPT ISR context only, while TRANSMITTING |
| `rs_bb_uart_tx_engine_advance(p_engine, p_rb)` | Prepares the *next* tick's output and advances the FSM; dequeues from the ring buffer when a new byte is needed. Call AFTER the GPIO write | GPT ISR context only, while TRANSMITTING |

See [ARCHITECTURE.md](ARCHITECTURE.md#3-tx-bit-engine-pipeline-and-data-flow) for the full pipeline design and the bit-level state machine these two functions implement.

---

## 9. Internal Functions — `rs_bb_uart_tx_hal.c` / `.h`

`rs_bb_uart_tx_hal.h` is explicitly marked *"Internal header — not part of the public API."* This is the **only** file that includes GPT-specific FSP headers and drives the TX pin via direct register/BSP pin control. Seven 🔒 **INTERNAL** functions plus the ISR-context callback.

### 🔒 GPT timer adapter — INTERNAL

| Function | Purpose |
|---|---|
| `rs_bb_uart_tx_hal_gpt_open(p_ctrl)` | Opens the GPT instance, queries its actual applied clock frequency via `infoGet()`, computes and applies the bit-period count for the configured baud rate via `periodSet()` (round-to-nearest — see [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md)). Does **not** start the timer. |
| `rs_bb_uart_tx_hal_gpt_start(p_ctrl)` | Resets the counter to 0, then starts the timer — every frame's first bit period is a full, consistent duration regardless of when `Start()` is called relative to any prior count state. |
| `rs_bb_uart_tx_hal_gpt_stop(p_ctrl)` | Stops the timer. Called from GPT ISR context once the TX engine returns to `TX_IDLE`. ISR-safe — a simple register write, not blocking. |
| `rs_bb_uart_tx_hal_gpt_close(p_ctrl)` | Closes the GPT instance. Not currently called by any Phase 1 code path (no public `Close()`-equivalent exists); provided for completeness. |

### 🔒 GPIO adapter — INTERNAL

| Function | Purpose |
|---|---|
| `rs_bb_uart_tx_hal_gpio_open(p_ctrl)` | Configures the TX pin as a push-pull output, idling high, via `R_BSP_PinAccessEnable()`/`R_BSP_PinCfg()`/`R_BSP_PinAccessDisable()` — not a full `ioport_instance_t`, since a single statically-configured pin doesn't need that generality. |
| `rs_bb_uart_tx_hal_gpio_write(p_ctrl, level_high)` | Drives the TX pin. Called once per GPT ISR tick. **Every call is bracketed by `R_BSP_PinAccessEnable()`/`PinAccessDisable()`** — on this FSP/MCU combination, `R_BSP_PinWrite()` writes to the same write-protected PmnPFS register `R_BSP_PinCfg()` uses, so the write would be silently dropped otherwise. These enable/disable calls are a lightweight, reference-counted pair explicitly documented as ISR-safe. |
| `rs_bb_uart_tx_hal_gpio_close(p_ctrl)` | Returns the TX pin to a safe idle-high state (best-effort; does not revert pin muxing — that is an application-level decision). Not currently called by any Phase 1 code path; provided for completeness. |

### 🔒 GPT ISR-context callback — INTERNAL

| Function | Purpose |
|---|---|
| `rs_bb_uart_tx_hal_gpt_callback(p_args)` | Registered as the GPT instance's `p_callback`. `p_args->p_context` is the owning `rs_bb_uart_tx_ctrl_t *`. Implements the two-half tick sequence described in [ARCHITECTURE.md](ARCHITECTURE.md#3-tx-bit-engine-pipeline-and-data-flow). Defensively returns immediately without touching engine/ring-buffer state if the lifecycle state is not TRANSMITTING (guards against a spurious/late interrupt). |

---

## 10. Complete Internal Function Reference Table

| Function | Tag | File | Calling Context |
|---|---|---|---|
| `rs_bb_uart_tx_open_internal()` | 🔒 INTERNAL | `rs_bb_uart_tx.c` | Board config file's `ConfigCheck()` only |
| `rs_bb_uart_tx_start_internal()` | ⚫ PRIVATE | `rs_bb_uart_tx.c` | `Write()`/`WriteString()` only |
| `rs_bb_uart_tx_ring_buffer_reset()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Before GPT ISR enabled |
| `rs_bb_uart_tx_ring_buffer_enqueue()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Main-loop/task context |
| `rs_bb_uart_tx_ring_buffer_dequeue()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | GPT ISR context |
| `rs_bb_uart_tx_ring_buffer_count_get()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Any |
| `rs_bb_uart_tx_ring_buffer_free_get()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Any |
| `rs_bb_uart_tx_engine_reset()` | 🔒 INTERNAL | `rs_bb_uart_tx_engine.c` | Before GPT ISR enabled |
| `rs_bb_uart_tx_engine_output_get()` | 🔒 INTERNAL | `rs_bb_uart_tx_engine.c` | GPT ISR context, TRANSMITTING only |
| `rs_bb_uart_tx_engine_advance()` | 🔒 INTERNAL | `rs_bb_uart_tx_engine.c` | GPT ISR context, TRANSMITTING only |
| `rs_bb_uart_tx_hal_gpt_open()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Called by `rs_bb_uart_tx_open_internal()` |
| `rs_bb_uart_tx_hal_gpt_start()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Called by `rs_bb_uart_tx_start_internal()` |
| `rs_bb_uart_tx_hal_gpt_stop()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | GPT ISR context |
| `rs_bb_uart_tx_hal_gpt_close()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Not called in Phase 1 |
| `rs_bb_uart_tx_hal_gpio_open()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Called by `rs_bb_uart_tx_open_internal()` |
| `rs_bb_uart_tx_hal_gpio_write()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | GPT ISR context |
| `rs_bb_uart_tx_hal_gpio_close()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Not called in Phase 1 |
| `rs_bb_uart_tx_hal_gpt_callback()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | GPT ISR — registered as `p_callback` |
| `rs_bb_uart_tx_ConfigGen()` | 🟢🔒 PUBLIC BRIDGE | `rs_bb_uart_tx_data_fpb_<board>.c` | Any, before `ConfigCheck()` |
| `rs_bb_uart_tx_ConfigCheck()` | 🟢🔒 PUBLIC BRIDGE | `rs_bb_uart_tx_data_fpb_<board>.c` | Any, once |

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
