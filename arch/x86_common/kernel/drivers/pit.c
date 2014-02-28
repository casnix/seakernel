/* pit.c: Copyright (c) 2010 Daniel Bittman
 * Functions for handling the PIT
 */
#include <kernel.h>
#include <isr.h>
#include <task.h>
#include <atomic.h>
void do_tick();

void install_timer(int hz)
{
	current_hz=hz;
	register_interrupt_handler(IRQ0, &tm_timer_handler, 0);
	u32int divisor = 1193180 / hz;
	outb(0x43, 0x36);
	u8int l = (u8int)(divisor & 0xFF);
	u8int h = (u8int)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);
}
