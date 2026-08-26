# Bit-Banged UART TX

Software-driven, transmit-only UART implemented using a GPIO output and a
General PWM Timer (GPT) for bit-period control.

The Bit-Banged UART TX Product Enabling Software (PES) provides UART-style
serial output when a hardware SCI/UART channel is unavailable, reserved, or
not desired. The reusable PES implementation is maintained under `src/` and
is shared by every example project.

## Features

- Transmit-only UART
- 8 data bits, no parity, 1 stop bit (8N1)
- Least-significant bit first
- Configurable baud rate
- Non-blocking operation
- Software-managed transmit ring buffer
- GPT-based bit-period timing
- No dynamic memory allocation
- Bare-metal example projects
- Common source shared by all supported boards and development environments

## Supported Boards

- FPB-RA2E3
- FPB-RA4E2
- FPB-RA6E2

Board-specific GPIO, VCOM, clock, GPT, device, and debugger configuration is
kept with each example project and is intentionally not duplicated here.

## Supported Development Environments

- Renesas e2 studio
- Visual Studio Code

## Repository Layout

```text
.
|-- README.md
|-- meson.build
|-- meson_options.txt
|-- metadata.r365.xml
|-- docs/
|-- examples/
|   |-- e2studio/
|   |   |-- fpb-ra2e3/
|   |   |-- fpb-ra4e2/
|   |   `-- fpb-ra6e2/
|   `-- vscode/
|       |-- fpb-ra2e3/
|       |-- fpb-ra4e2/
|       `-- fpb-ra6e2/
|-- firmware/
|-- meson/
|-- scripts/
|-- src/
|-- subprojects/
|-- tests/
`-- toolchains/
```

### Directory Responsibilities

- `src/`: Generic reusable PES source. Example projects consume this common
  implementation and must not maintain independent copies.
- `examples/e2studio/`: Board-specific e2 studio example projects.
- `examples/vscode/`: Board-specific Visual Studio Code example projects.
- `meson/`: Shared Meson helper files and build infrastructure.
- `subprojects/`: Meson subproject definitions and dependency wraps.
- `tests/`: Host and target test sources, mocks, fixtures, and test build files.
- `toolchains/`: Cross-compilation and host toolchain definitions.
- `docs/`: Product, build, API, architecture, porting, and release documentation.
- `scripts/`: Build, test, packaging, and continuous-integration utilities.
- `firmware/`: Generated or packaged firmware artifacts when applicable.

## Reusable PES Source

The generic implementation resides under:

```text
src/rs_bb_uart_tx/
```

The source organization may include:

```text
src/rs_bb_uart_tx/
|-- rs_bb_uart_tx.c
|-- rs_bb_uart_tx.h
|-- rs_bb_uart_tx_types.h
|-- rs_bb_uart_tx_cfg.h
|-- rs_bb_uart_tx_ctrl.h
|-- rs_bb_uart_tx_engine.c
|-- rs_bb_uart_tx_engine.h
|-- rs_bb_uart_tx_ring_buffer.c
|-- rs_bb_uart_tx_ring_buffer.h
|-- rs_bb_uart_tx_hal.c
|-- rs_bb_uart_tx_hal.h
`-- config/
```

The `src/` directory contains reusable PES code only. IDE metadata, generated
board projects, launch files, and demo application code belong under
`examples/`.

## Example Projects

Examples are grouped first by development environment and then by board:

```text
examples/<development-environment>/<board>/
```

Each board example README contains the details required for that particular
combination, including:

- Prerequisites and tool versions
- Build and debug steps
- Target device selection
- GPIO TX selection
- On-board VCOM or optional external TTL-to-USB routing
- GPT instance and interrupt configuration
- Board clock and timer-divider configuration
- Expected demonstration output
- Board-specific limitations and validation notes

Start with one of the following directories:

```text
examples/e2studio/
examples/vscode/
```

## Serial Configuration

The default demonstration uses the following frame format:

| Setting | Value |
|---|---|
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Bit order | LSB first |
| Flow control | None |

The selected example defines the default baud rate, TX GPIO, timer instance,
and observation path.

## Public API

Include the public PES header and the configuration data selected by the
example project:

```c
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>

rs_bb_uart_tx_cfg_t    config;
rs_bb_uart_tx_status_t status;

status = rs_bb_uart_tx_ConfigGen(&config, timer_config, tx_pin);

if (RS_BB_UART_TX_STATUS_SUCCESS == status)
{
    status = rs_bb_uart_tx_ConfigCheck(&config);
}

if (RS_BB_UART_TX_STATUS_SUCCESS == status)
{
    status = rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\\r\\n");
}
```

The public interface provides configuration, write, and status operations.
Refer to the API header and API reference for the authoritative function
signatures, parameter requirements, calling context, and return values.

## Configuration

The generic PES derives UART bit timing from the GPT configuration supplied by
the selected example. Board-specific files provide the applicable timer, pin,
clock, interrupt, and peripheral-conflict settings.

Before running an example, verify that:

- The selected TX pin is configured as a GPIO output and idles high.
- No peripheral function claims the selected TX pin.
- The GPT overflow interrupt is enabled.
- The GPT callback is connected to the PES HAL callback.
- The selected timer configuration covers the required baud-rate range.
- The terminal configuration matches the example configuration.

## Observing Output

The preferred observation path is the on-board debug probe VCOM connection
when that path is supported by the selected example. An external TTL-to-USB
adapter is an optional secondary path.

For an external adapter, connect the configured GPIO TX output to the adapter
RXD input and connect board ground to adapter ground. Do not connect a target
GPIO directly to a true RS-232-level interface.

## Expected Demo Output

```text
================================
Renesas Bit-Banged UART Demo
================================
Project      : Bare-Metal Example
UART Mode    : Software TX Only
Frame Format : 8N1
System Ready
[000001] Heartbeat OK
[000002] Heartbeat OK
...
```

A board example may add the selected board, baud rate, and transport to the
banner.

## Testing

The top-level `tests/` directory contains tests for the generic implementation.
Tests should exercise reusable behavior independently of IDE-specific example
projects wherever practical. Target integration tests may use board-specific
fixtures while continuing to compile the common source from `src/`.

## Known Limitations

- TX only
- No receive path
- No full-duplex operation
- No interactive command-line input
- Write operations are not intended for interrupt-handler context
- Runtime reconfiguration is not supported while transmission is active
- A write that does not fit in the available ring-buffer space returns an
  error rather than partially enqueueing data
- Sustained input faster than the configured serial output rate can exhaust
  available transmit-buffer capacity

See each example README for board-specific limitations and validation status.

## Documentation

Additional documentation is maintained under `docs/`. Use the documentation
and the selected example README for build instructions, architecture details,
porting guidance, API behavior, acceptance criteria, and release information.
