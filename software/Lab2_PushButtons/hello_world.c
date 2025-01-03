/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

static void button_press(void * context, alt_u32 id);
static void timer_end(void * context, alt_u32 id);

int main()
{
	//register ISRs
	alt_irq_register(TIMER_0_IRQ, (void*) 0, timer_end);
	alt_irq_register(BUTTON_PIO_IRQ, (void*) 0, button_press);

	IOWR(BUTTON_PIO_BASE, 2, 0xF);

	//enable all button interrupts
	IOWR(BUTTON_PIO_BASE, 2, 0xF);
	while (1) {

	}
	return 0;
}

static void timer_end(void * context, alt_u32 id){
	//read the button status
	int status = IORD(BUTTON_PIO_BASE, 0);

	//turns on corresponding LED
	IOWR(LED_PIO_BASE, 0, status ^ 0xF);
	//clear status of the timer
	IOWR(TIMER_0_BASE, 0, 0);
	//clear interrupt request
	IOWR(BUTTON_PIO_BASE, 3, 0);
	//enable button interrupts
	IOWR(BUTTON_PIO_BASE, 2, 0xF);
}

static void button_press(void * context, alt_u32 id){
	//disable interrupts from button
	IOWR(BUTTON_PIO_BASE, 2, 0);

	//debounce button press, start timer
	IOWR(TIMER_0_BASE, 2, 0x4240);
	IOWR(TIMER_0_BASE, 3, 0xF);

	IOWR(TIMER_0_BASE, 1, 0x5); // 0x5 since we want START bit and ITO bit to be 1, 0b0101 = 0x5
}
