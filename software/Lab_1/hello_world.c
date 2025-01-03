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
#define MAX_PULSE_WIDTH 2500

int background();
static void egm_pulse_received(void * context, alt_u32 id);
void run_interrupt_test();
void run_poll_test();
int back_task_counter = 0;
int char_cycle();
int pulse_widths[4] = {250, 375, 750, 1250}; // for the performance table

void run_poll_test(){
	// disable interrupt
	IOWR(STIMULUS_IN_BASE, 2, 0);

	for (int pulse_width = 1; pulse_width <= MAX_PULSE_WIDTH; ++ pulse_width) {

	//for the performance table
	//for (int i = 0; i < 4; ++ i) {
		//int pulse_width = pulse_widths[i];

		//give a narrow pulse of led to signal start of test
		IOWR(LED_PIO_BASE, 0, 2);
		IOWR(LED_PIO_BASE, 0, 0);
		//disable egm
		IOWR(EGM_BASE, 0, 0);
		//set pulse width and period
		int period = pulse_width * 2;
		IOWR(EGM_BASE, 2, pulse_width * 2);
		IOWR(EGM_BASE, 3, pulse_width);
		//enable egm
		IOWR(EGM_BASE, 0, 1);
		int poll_counter = char_cycle();

		while (1) {
			if (IORD(EGM_BASE, 1) == 0) {
				break;
			}

			// run background tasks
			for (int i = 0; i <= poll_counter; i++) {
				// check if egm enabled
				if (IORD(EGM_BASE, 1) == 1) {
					background();
					back_task_counter ++;
				} else {
					break;
				}
			}

			// poll for stimulus
			while (1) {
				// check if egm is enabled
				if (IORD(EGM_BASE, 1) == 1) {
					if (IORD(STIMULUS_IN_BASE, 0) == 1) {
						IOWR(RESPONSE_OUT_BASE, 0, 1);
						IOWR(RESPONSE_OUT_BASE, 0, 0);
						break;
					}
				} else {
					break;
				}
			}
		}

		printf("%d, %d, %d, %d, %d, %d\n", period, pulse_width, back_task_counter, IORD(EGM_BASE, 4),
						IORD(EGM_BASE, 5), IORD(EGM_BASE, 6));
		back_task_counter = 0;
	}
}

int char_cycle() {
	int poll_counter = -1;
	while (1) {
		// check if egm is enabled
		if (IORD(EGM_BASE, 1) == 0) {
			break;
		}

		//check for stimulus
		if (IORD(STIMULUS_IN_BASE, 0) == 1) {
			//send response
			IOWR(RESPONSE_OUT_BASE, 0, 1);
			IOWR(RESPONSE_OUT_BASE, 0, 0);
			int prev_stimulus = 1; // check for rising edge

			while (1) {
				//check if egm is enabled
				if (IORD(EGM_BASE, 1) == 0) {
					return 0;
				}
				//run as many background tasks as possible
				background();
				poll_counter ++ ;
				back_task_counter ++;
				int cur_stimulus = IORD(STIMULUS_IN_BASE, 0);

				if ((cur_stimulus == 1) && (prev_stimulus == 0)) {
					IOWR(RESPONSE_OUT_BASE, 0, 1);
					IOWR(RESPONSE_OUT_BASE, 0, 0);
					return poll_counter;
				}
				prev_stimulus = cur_stimulus;
			}

		}

	}
}

void run_interrupt_test() {

	// enable interrupt
	IOWR(STIMULUS_IN_BASE, 2, 1);

	for (int pulse_width = 1; pulse_width <= MAX_PULSE_WIDTH; ++ pulse_width) {

	//for the performance table
	//	for (int i = 0; i < 4; ++ i) {
	//		int pulse_width = pulse_widths[i];

		//disable egm
		IOWR(EGM_BASE, 0, 0);
		//set pulse width and period
		int period = pulse_width * 2;
		IOWR(EGM_BASE, 2, pulse_width * 2);
		IOWR(EGM_BASE, 3, pulse_width);
		//enable egm
		IOWR(EGM_BASE, 0, 1);

		while (1) {
			//check if test is over
			if (IORD(EGM_BASE, 1) == 0) {
				break;
			}
			background();
			back_task_counter++;
		}

		//print egm results
		printf("%d, %d, %d, %d, %d, %d\n", period, pulse_width, back_task_counter, IORD(EGM_BASE, 4),
				IORD(EGM_BASE, 5), IORD(EGM_BASE, 6));
		back_task_counter = 0;
	}

	//disable interrupt
	IOWR(STIMULUS_IN_BASE, 2, 0);
}

int main()
{
	//register ISR
	alt_irq_register(STIMULUS_IN_IRQ, (void*) 0, egm_pulse_received);
	//set response out to 0
	IOWR(RESPONSE_OUT_BASE, 0, 0);
	while (1) {
		unsigned char switches = IORD(SWITCH_PIO_BASE, 0) & 1;
		if (switches == 0) {
			printf("Interrupt method selected.\n");
			printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n\n Please, press PB0 to continue.\n\n");
		} else {
			printf("Tight polling method selected\n");
			printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n\n Please, press PB0 to continue.\n\n");
		}

		//wait for button or switch
		//if switch is flipped, enter if statement again
		while (1) {
			if ((IORD(SWITCH_PIO_BASE, 0) & 1) != switches) {
				break;
			} else if ((IORD(BUTTON_PIO_BASE, 0) | 14) != 15) {
				// Run the experiment
				printf("running\n");
				if (switches == 0){
					run_interrupt_test();
				} else {
					run_poll_test();
				}
				break;
			}
		}
	}
  return 0;
}

static void egm_pulse_received(void * context, alt_u32 id){
	//send output pulse
	IOWR(RESPONSE_OUT_BASE, 0, 1);
	IOWR(RESPONSE_OUT_BASE, 0, 0);
	//return from isr
	IOWR(STIMULUS_IN_BASE, 3, 0x0);
}

int background()
{
int j;
int x = 0;
int grainsize = 4;
int g_taskProcessed = 0;

for(j = 0; j < grainsize; j++)
	{
		g_taskProcessed++;
	}
	return x;
}
