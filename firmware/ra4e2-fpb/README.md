# FPB-RA4E2 firmware configuration

`configuration.xml` is the FSP Smart Configurator project (r_gpt timer stack + GPIO) for the FPB-RA4E2
(Cortex-M33, R7FA4E2B9). Meson runs RASC headless on it (`scripts/run_rasc.py`) to produce `ra_gen`/`ra_cfg` and the
LLD linker scripts.

It is a copy of `examples/VSCode/BitBangedUART_TX_FPB_RA4E2/configuration.xml`; keep the two in sync.

```powershell
meson setup build-ra4e2 --cross-file toolchains\llvm-cortex-m33.ini -Dgen_board=ra4e2-fpb -Dboard=ra4e2_fpb -Dmcu=ra4e2 -Ddevice=RA4E2
meson compile -C build-ra4e2
```

`_RA_CORE` is chosen automatically (CM33).
