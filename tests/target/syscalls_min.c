/***********************************************************************************************************************
 * @file syscalls_min.c
 * @brief Minimal newlib/picolibc syscall stubs for the firmware unit-test build.
 *
 * Provides _write() so that any printf()/puts() reaches the Unity output port.
 * All other syscalls return sensible no-op values to satisfy the linker
 * without pulling in a semihosting implementation.
 *
 * Included in the firmware_ut_elf source list in firmware/meson.build.
 **********************************************************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

extern void unity_putc(int c);   /* tests/target/unity_port_polled_gpio.c */

int _write(int fd, char * ptr, int len)
{
    (void) fd;
    if ((NULL == ptr) || (len <= 0))
    {
        return 0;
    }
    for (int i = 0; i < len; i++)
    {
        unity_putc(ptr[i]);
    }
    return len;
}

int _read(int fd, char * ptr, int len)   { (void) fd; (void) ptr; (void) len; return 0; }
int _close(int fd)                        { (void) fd; return -1; }
int _fstat(int fd, struct stat * st)      { (void) fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                       { (void) fd; return 1; }
int _lseek(int fd, int ptr, int dir)      { (void) fd; (void) ptr; (void) dir; return 0; }
void _exit(int status)                    { (void) status; while (1) { __asm volatile ("nop"); } }
int _kill(int pid, int sig)               { (void) pid; (void) sig; return -1; }
int _getpid(void)                         { return 1; }
