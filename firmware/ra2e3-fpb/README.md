# FPB-RA2E3 firmware configuration

`configuration.xml` is the FSP Smart Configurator project (r_gpt timer stack + GPIO) for the FPB-RA2E3
(Cortex-M23, R7FA2E3073CFL). Meson runs RASC headless on it (`scripts/run_rasc.py`) to produce `ra_gen`/`ra_cfg` and the
LLD linker scripts.

It is a copy of `examples/VSCode/BitBangedUART_TX_FPB_RA2E3/configuration.xml`; keep the two in sync.

```powershell
meson setup build-ra2e3 --cross-file toolchains\llvm-cortex-m23.ini -Dgen_board=ra2e3-fpb -Dboard=ra2e3_fpb -Dmcu=ra2e3 -Ddevice=RA2E3
meson compile -C build-ra2e3
```

`_RA_CORE` is chosen automatically (CM23).
