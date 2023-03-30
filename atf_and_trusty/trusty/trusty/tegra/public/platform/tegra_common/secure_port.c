/*
 * Copyright (c) 2008 Travis Geiselbrecht
 * Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
 
// DEBUG_TX2: WHOLE FILE IS MODIFIED
#include <stdarg.h>
#include <reg.h>
#include <debug.h>
#include <printf.h>
#include <kernel/thread.h>
#include <platform/debug.h>
#include <arch/ops.h>
#include <platform/memmap.h>
#include <platform/platform_p.h>
#include <target/debugconfig.h>
#include <platform/combined_uart.h>
#include <platform/secure_port.h>

static unsigned int disable_secure_port = 1; /* initially disabled */

#define TEGRA_UART_NONE	0x0
#define TEGRA_COMBUART_ID 0xfe

static vaddr_t uart_base[] = {
	TEGRA_UART_NONE,
	TEGRA_UARTA_BASE,
	TEGRA_UARTB_BASE,
	TEGRA_UARTC_BASE,
	TEGRA_UARTD_BASE,
	TEGRA_UARTE_BASE
};


static unsigned int secure_port = 3;

#define UART_RHR	0
#define UART_THR	0
#define UART_LSR	5

/* 500 ms UART timeout */
#define UART_TIMEOUT_US	(500L * 1000L) /* in microseconds */

void platform_disable_secure_intf(void)
{
	disable_secure_port = 1;
}

void platform_enable_secure_intf(void)
{
	disable_secure_port = 0;
}

static inline void secure_write_uart_reg(int port, uint reg, unsigned char data)
{
	*REG8(uart_base[port] + (reg << 2)) = data;
}

static inline unsigned char secure_read_uart_reg(int port, uint reg)
{
	return *REG8(uart_base[port] + (reg << 2));
}

static int secure_uart_putc(int port, char c )
{
	static bool timed_out = false;
	lk_bigtime_t start = current_time_hires();

	while (!(secure_read_uart_reg(port, UART_LSR) & (1<<5))) {
		if (timed_out)
			return -1;
		if (current_time_hires() - start >= UART_TIMEOUT_US) {
			timed_out = true;
			return -1;
		}
	}

	timed_out = false;
	secure_write_uart_reg(port, UART_THR, c);
	return 0;
}

static int secure_uart_getc(int port, bool wait)
{
	static bool timed_out = false;
	lk_bigtime_t start = current_time_hires();

	if (wait) {
		while (!(secure_read_uart_reg(port, UART_LSR) & (1<<0))) {
			if (timed_out)
				return -1;
			if (current_time_hires() - start >= UART_TIMEOUT_US) {
				timed_out = true;
				return -1;
			}
		}
	} else {
		if (!(secure_read_uart_reg(port, UART_LSR) & (1<<0)))
			return -1;
	}

	timed_out = false;

	return secure_read_uart_reg(port, UART_RHR);
}

void secure_platform_dputc(char c)
{
	if (disable_secure_port || (secure_port == TEGRA_UART_NONE))
		return;

	if (secure_port == TEGRA_COMBUART_ID) {
		platform_tegra_comb_uart_putc(c);
		return;
	}

	if (c == '\n') {
		secure_uart_putc(secure_port, '\r');
	} else if (c == '\0') {
		return;
	}
	secure_uart_putc(secure_port, c);
}

int secure_platform_dgetc(char *c, bool wait)
{
	int _c;

	if (disable_secure_port || (secure_port == TEGRA_UART_NONE))
		return -1;

	if (secure_port == TEGRA_COMBUART_ID) {
		_c = platform_tegra_comb_uart_getc(wait);
	} else {
		_c = secure_uart_getc(secure_port, wait);
	}

	if (_c < 0)
		return -1;

	*c = _c;
	return 0;
}

void platform_init_secure_port(unsigned int dbg_port)
{
	
	secure_port = dbg_port;
	platform_enable_secure_intf();
	dprintf(0, "DEBUG PORT ENABLED with %d\n", secure_port);
	secure_platform_dputc('f');
}
