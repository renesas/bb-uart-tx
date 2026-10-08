#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

/*
 * Route Unity character output to the board VCOM TXD pin.
 * unity_putc() is implemented in tests/target/unity_port_polled_gpio.c.
 */

void unity_putc(int c);

#define UNITY_OUTPUT_CHAR(a)    unity_putc(a)
#define UNITY_OUTPUT_FLUSH()
#define UNITY_OUTPUT_START()
#define UNITY_OUTPUT_COMPLETE()

#endif /* UNITY_CONFIG_H */
