/*
 * crt0_stub.c - Weak _start stub for the firmware builds.
 *
 * Satisfies picolibc libc.a's reference to crt0.o.  The FSP uses
 * Reset_Handler as the true entry point (see -Wl,-eReset_Handler in
 * firmware/meson.build); _start is never called.  Without this stub, lld
 * reports "cannot open crt0.o".
 */

__attribute__((weak, noreturn)) void _start(void)
{
    /* Should never be called — FSP Reset_Handler is the entry point */
    while (1) { ; }
}
