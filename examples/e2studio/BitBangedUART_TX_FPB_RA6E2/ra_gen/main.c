/* generated main source file - do not edit
 *
 * Matches the exact pattern of a real RASC-generated FPB-RA6E2 bare-metal
 * main.c — no g_hal_init() call needed before hal_entry() for a bare-metal
 * (no RTOS) project with no Configurator-managed auto-init instances;
 * g_common_init()/g_hal_init() (see ra_gen/hal_data.c) remain available
 * and are still a no-op here. */
#include "hal_data.h"
int main(void) {
    hal_entry();
    return 0;
}
