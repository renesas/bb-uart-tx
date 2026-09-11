# Design Flow and Architecture

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Lifecycle State Machine](#1-lifecycle-state-machine)
2. [Public Function State Gating](#2-public-function-state-gating)
3. [TX Bit-Engine Pipeline and Data Flow](#3-tx-bit-engine-pipeline-and-data-flow)
4. [Module File Responsibilities](#4-module-file-responsibilities)
5. [Ring Buffer Concurrency Model](#5-ring-buffer-concurrency-model)

---

## 1. Lifecycle State Machine

The module implements a five-state internal lifecycle. Unlike modules that expose `Open()`/`Start()`/`Stop()`/`Close()` as public functions, every transition here happens as a side effect of a public function documented in [BIT_BANGED_UART_TX-USER_MANUAL.md](BIT_BANGED_UART_TX-USER_MANUAL.md#21-design-pattern-single-call-simple-use) — there is no public lifecycle API to call directly.

```
                 rs_bb_uart_tx_ConfigGen()      rs_bb_uart_tx_ConfigCheck()
UNCONFIGURED ───────────────────────────→ CONFIGURED ───────────────────────→ READY
                                                                                 │  ↑
                                                                    Write()/     │  │ queue drains
                                                              WriteString()      │  │ (GPT ISR,
                                                              queues data,       │  │  queue_now_empty)
                                                              starts GPT         ▼  │
                                                                             TRANSMITTING

     Any state, on unrecoverable HAL failure
              (GPT/GPIO open or start failure)
                         │
                         ▼
                      ERROR
```

| State | Description |
|---|---|
| `RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED` | Initial state. No valid configuration bound. `ConfigGen()` has not (yet) succeeded. |
| `RS_BB_UART_TX_LIFECYCLE_CONFIGURED` | `ConfigGen()` has populated `*p_cfg`, but `ConfigCheck()` has not yet been called (or a re-attempt is pending). No HAL resources opened yet. |
| `RS_BB_UART_TX_LIFECYCLE_READY` | `ConfigCheck()` succeeded: configuration validated, GPT and GPIO HAL resources opened, ring buffer and TX bit-engine reset. Idle, ready to transmit. |
| `RS_BB_UART_TX_LIFECYCLE_TRANSMITTING` | The GPT timer is running and its ISR is actively driving bits onto the TX pin. Entered when `Write()`/`WriteString()` successfully enqueues data while the module was idle (or already TRANSMITTING). Returns to READY automatically, from ISR context, once the ring buffer is confirmed empty. |
| `RS_BB_UART_TX_LIFECYCLE_ERROR` | Deterministic error state, entered from any state on an unrecoverable GPT/GPIO HAL failure. Requires re-initialization (a fresh `ConfigGen()`/`ConfigCheck()` sequence is not idempotent while already past `CONFIGURED` — see [Section 2](#2-public-function-state-gating)). |

**There is no public transition back out of ERROR, TRANSMITTING→READY (other than the automatic queue-drain transition above), or READY→UNCONFIGURED in Phase 1.** The internal `rs_bb_uart_tx_open_internal()` helper (called from the board configuration file's `ConfigCheck()` implementation) only accepts the UNCONFIGURED or CONFIGURED starting states — see [Known Limitations](BIT_BANGED_UART_TX-USER_MANUAL.md#known-limitations-phase-1-by-design).

---

## 2. Public Function State Gating

| Public Function | UNCONFIGURED | CONFIGURED | READY | TRANSMITTING | ERROR |
|---|---|---|---|---|---|
| `rs_bb_uart_tx_ConfigGen()` | ✓ (→ CONFIGURED) | ✓ (re-populates `*p_cfg`; does not itself change lifecycle state) | ✓ | ✓ | ✓ |
| `rs_bb_uart_tx_ConfigCheck()` | ✗ `INVALID_POINTER`¹ | ✓ (→ READY on success) | `INVALID_STATE` | `INVALID_STATE` | `INVALID_STATE` |
| `rs_bb_uart_tx_ConfigGetSize()` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `rs_bb_uart_tx_Write()` | `INVALID_STATE` | `INVALID_STATE` | ✓ (→ TRANSMITTING) | ✓ (queues more; stays TRANSMITTING) | `INVALID_STATE` |
| `rs_bb_uart_tx_WriteString()` | `INVALID_STATE` | `INVALID_STATE` | ✓ (→ TRANSMITTING) | ✓ | `INVALID_STATE` |
| `rs_bb_uart_tx_StatusGet()` | ✓ (fields read as zero/UNCONFIGURED) | ✓ | ✓ | ✓ | ✓ |

¹ `ConfigCheck()` called before a successful `ConfigGen()` will typically fail earlier, on `p_cfg->p_gpt_instance == NULL`, with `RS_BB_UART_TX_STATUS_INVALID_POINTER` — `ConfigGen()` is the call that transitions UNCONFIGURED → CONFIGURED.

Calling `ConfigCheck()` a second time after it has already succeeded (state READY, TRANSMITTING, or ERROR) returns `RS_BB_UART_TX_STATUS_INVALID_STATE` and increments the `invalid_call_count` diagnostic — this is not idempotent by design in Phase 1 (Architecture & Design: "Runtime remapping of GPIO or timer while transmission is active" is explicitly out of scope).

Every public function documents its own complete return-value list in [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md) and in `rs_bb_uart_tx.h`.

---

## 3. TX Bit-Engine Pipeline and Data Flow

```
Application (main-loop / task context)
  │
  └──→ rs_bb_uart_tx_Write(p_data, length) / rs_bb_uart_tx_WriteString(p_str)
            │
            ├── rs_bb_uart_tx_ring_buffer_enqueue()   ← all-or-nothing copy into
            │                                            the static SPSC ring buffer
            └── rs_bb_uart_tx_start_internal()         ← starts the GPT timer if the
                    │                                     module was idle (READY);
                    │                                     no-op if already TRANSMITTING
                    ↓
         (call returns here — non-blocking; data is queued, not yet transmitted)


GPT ISR — rs_bb_uart_tx_hal_gpt_callback(), fires once per bit period
  │
  ├── 1. rs_bb_uart_tx_engine_output_get(&engine)     ← O(1), NO ring-buffer access.
  │        Returns the pin level already decided by
  │        the *previous* tick's advance() call.
  │
  ├── 2. rs_bb_uart_tx_hal_gpio_write(p_ctrl, level)  ← GPIO write happens FIRST,
  │        R_BSP_PinAccessEnable()/PinWrite()/           with zero variable-cost work
  │        PinAccessDisable()                            ahead of it on every tick.
  │
  ├── 3. rs_bb_uart_tx_engine_advance(&engine, &rb)   ← runs AFTER the pin write.
  │        Prepares the level for the *next* tick and     Dequeues from the ring buffer
  │        advances the bit-level FSM (see below).        here when a new byte is needed —
  │                                                        this is safe because it no longer
  │                                                        affects the tick that already ran.
  │
  ├── if adv.byte_transmitted:
  │        total_bytes_transmitted++
  │
  └── if adv.queue_now_empty:
           rs_bb_uart_tx_hal_gpt_stop(p_ctrl)          ← stop interrupting once idle
           state = READY                                (TRANSMITTING → READY)
```

### 3.1 Why the Callback Is Split Into Two Halves

The GPT ISR callback deliberately performs `output_get()` (read the pre-decided pin level) **before** `advance()` (decide the *next* level, dequeuing from the ring buffer if needed). This pipelining means the GPIO write for the current tick never has to wait on a ring-buffer access — only the *next* tick's preparation does, and that preparation has a full bit period of slack (as little as ~8.68 µs at 115200 baud) rather than needing to complete before the pin write.

### 3.2 Bit-Level State Machine

```
TX_IDLE --(byte available)--> TX_START_BIT --> TX_DATA_BITS --> TX_STOP_BIT
   ^                                                                  |
   |                                                                  v
   +----------(queue empty, freshly re-checked)----- TX_NEXT_BYTE <---+
                                                            |
                                                            +--(another byte queued)--> TX_START_BIT
```

Frame format (fixed in Phase 1): **8N1, idle high, LSB-first.**

| Segment | Level | Duration |
|---|---|---|
| Idle | High | (between frames) |
| Start bit | Low | 1 bit period |
| Data bits 0–7 | LSB first | 8 bit periods |
| Stop bit | High | 1 bit period |

`TX_STOP_BIT`'s `advance()` call *prefetches* the next dequeue one tick early — since dequeuing from an empty queue is a harmless no-op, doing so speculatively costs nothing if it finds nothing. But the actual decision to stop the GPT timer is **never** taken from that prefetch: only `TX_NEXT_BYTE`'s `advance()` — one tick later, performing a fully fresh dequeue — is allowed to declare the queue empty and stop the timer. This means a `Write()` call landing in the narrow window between the prefetch and the fresh re-check is always picked up correctly; nothing queued during that window is ever stranded until the next transmission is manually kicked off.

**One accepted, deliberate launch-latency cost:** the very first bit period after an idle-to-transmitting transition always outputs idle-high (a "wasted" bit period) before the first byte's start bit begins on the following tick — about one bit period of extra latency, once per transition from idle to transmitting. This is a one-time launch-latency cost, not per-bit jitter, and has no effect on a receiver, which does not care when a transmission begins, only that its bit timing is accurate once it does.

---

## 4. Module File Responsibilities

| Module | Responsibility |
|---|---|
| `rs_bb_uart_tx.c` | Public function implementations; internal lifecycle state machine; owns the single static control block `g_bb_uart_tx_ctrl`; RAM-budget `_Static_assert` against `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` |
| `rs_bb_uart_tx_ring_buffer.c` | Static, fixed-capacity SPSC ring buffer. All-or-nothing enqueue (producer, main-loop/task context); single-byte dequeue (consumer, GPT ISR context). Lock-free — see [Section 5](#5-ring-buffer-concurrency-model) |
| `rs_bb_uart_tx_engine.c` | Hardware-agnostic TX bit-level state machine. Never includes FSP headers directly — calls back into the HAL shim to actually drive the pin, so it is unit-testable on the host |
| `rs_bb_uart_tx_hal.c` | GPT timer adapter (open/start/stop/close, period calculation via `infoGet()`), GPIO adapter (direct BSP pin control), and the GPT ISR-context callback that ties the engine and ring buffer together on every tick. The only file that includes GPT-specific FSP headers |
| `rs_bb_uart_tx_data_fpb_<board>.c/.h` | Board preset GPT instance; also hosts `rs_bb_uart_tx_ConfigGen()`, `rs_bb_uart_tx_ConfigCheck()`, `rs_bb_uart_tx_ConfigGetSize()` — see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#2-configuration-bridge) |

---

## 5. Ring Buffer Concurrency Model

The ring buffer is a classic **single-producer/single-consumer (SPSC)** design:

- `head` is written **only** by the producer (`rs_bb_uart_tx_ring_buffer_enqueue()`, called from main-loop/task context via `Write()`/`WriteString()`).
- `tail` is written **only** by the consumer (`rs_bb_uart_tx_ring_buffer_dequeue()`, called exclusively from GPT ISR context via the TX engine).

Because each side only ever *writes* the index it owns and only *reads* the index it doesn't, and because a naturally-aligned 32-bit store to `head` or `tail` is indivisible with respect to interrupts on this MCU family, correctness does **not** require disabling interrupts anywhere in the enqueue/dequeue path. Both fields are `volatile` so each side always observes the other's latest published value rather than a compiler-cached stale one. Occupancy is derived on demand from `head - tail` rather than tracked in a separate shared `count` field, which removes the need for a critical section on either operation.

Because `head == tail` must remain an unambiguous "empty" signal with no separate `count` field, the design reserves one slot: usable capacity is `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1` bytes, one fewer than the raw storage size — the standard, well-known trade-off for a head/tail-only circular buffer. Index wraparound uses a bitmask (`& (SIZE - 1)`), not `%`, which is why `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` must be a power of two (`64`/`128`/`256`/`512`/`1024` are the documented reference sizes; enforced by a `_Static_assert` in `rs_bb_uart_tx_ring_buffer.h`).

See [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md) for RAM budget details and [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) for why the default buffer size was raised from the design doc's nominal 256 bytes to 512 bytes.

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
