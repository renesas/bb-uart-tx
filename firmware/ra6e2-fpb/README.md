# FPB-RA6E2 firmware configuration

`configuration.xml` is the FSP Smart Configurator project (r_gpt timer stack + GPIO) for the FPB-RA6E2
(Cortex-M33, R7FA6E2BB). Meson runs RASC headless on it (`scripts/run_rasc.py`) to produce `ra_gen`/`ra_cfg` and the
LLD linker scripts.

It is a copy of `examples/VSCode/BitBangedUART_TX_FPB_RA6E2/configuration.xml`; keep the two in sync.

```powershell
meson setup build-ra6e2 --cross-file toolchains\llvm-cortex-m33.ini -Dgen_board=ra6e2-fpb -Dboard=ra6e2_fpb -Dmcu=ra6e2 -Ddevice=RA6E2
meson compile -C build-ra6e2
```

`_RA_CORE` is chosen automatically (CM33).
