/*=========================================================================*/
/*  Includes                                                               */
/*=========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <system.h>
#include <sys/alt_alarm.h>
#include <io.h>

#include "alt_types.h"
#include "altera_up_avalon_audio.h"

#include <altera_up_avalon_audio.h>
#include <altera_up_avalon_audio_and_video_config.h>
#include "diskio.h"
#include "fatfs.h"
#include "ff.h"
#include "monitor.h"
#include "uart.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "board_diag.h"


/*=========================================================================*/
/*  DEFINE: All Structures and Common Constants                            */
/*=========================================================================*/

/*=========================================================================*/
/*  DEFINE: Macros                                                         */
/*=========================================================================*/

#define PSTR(_a)  _a

/*=========================================================================*/
/*  DEFINE: Prototypes                                                     */
/*=========================================================================*/

/*=========================================================================*/
/*  DEFINE: Definition of all local Data                                   */
/*=========================================================================*/
//static alt_alarm alarm;
static unsigned long Systick = 0;
static volatile unsigned short Timer;   /* 1000Hz increment timer */

/*=========================================================================*/
/*  DEFINE: Definition of all local Procedures                             */
/*=========================================================================*/

/***************************************************************************/
/*  TimerFunction                                                          */
/*                                                                         */
/*  This timer function will provide a 10ms timer and                      */
/*  call ffs_DiskIOTimerproc.                                              */
/*                                                                         */
/*  In    : none                                                           */
/*  Out   : none                                                           */
/*  Return: none                                                           */
/***************************************************************************/
static alt_u32 TimerFunction (void *context)
{
   static unsigned short wTimer10ms = 0;

   (void)context;

   Systick++;
   wTimer10ms++;
   Timer++; /* Performance counter for this module */

   if (wTimer10ms == 10)
   {
      wTimer10ms = 0;
      ffs_DiskIOTimerproc();  /* Drive timer procedure of low level disk I/O module */
   }

   return(1);
} /* TimerFunction */

/***************************************************************************/
/*  IoInit                                                                 */
/*                                                                         */
/*  Init the hardware like GPIO, UART, and more...                         */
/*                                                                         */
/*  In    : none                                                           */
/*  Out   : none                                                           */
/*  Return: none                                                           */
/***************************************************************************/
static void IoInit(void)
{
   uart0_init(115200);

   /* Init diskio interface */
   ffs_DiskIOInit();

   //SetHighSpeed();

   /* Init timer system */
   //alt_alarm_start(&alarm, 1, &TimerFunction, NULL);

} /* IoInit */

/*=========================================================================*/
/*  DEFINE: All code exported                                              */
/*=========================================================================*/

uint32_t acc_size;                 /* Work register for fs command */
uint16_t acc_files, acc_dirs;
FILINFO Finfo;
#if _USE_LFN
char Lfname[512];
#endif

char Line[256];                 /* Console input buffer */

FATFS Fatfs[_VOLUMES];          /* File system object for each logical drive */
FIL File1, File2;               /* File objects */
DIR Dir;                        /* Directory object */
uint8_t Buff[2048] __attribute__ ((aligned(4)));  /* Working buffer */




static
FRESULT scan_files(char *path)
{
    DIR dirs;
    FRESULT res;
    uint8_t i;
    char *fn;


    if ((res = f_opendir(&dirs, path)) == FR_OK) {
        i = (uint8_t)strlen(path);
        while (((res = f_readdir(&dirs, &Finfo)) == FR_OK) && Finfo.fname[0]) {
            if (_FS_RPATH && Finfo.fname[0] == '.')
                continue;
#if _USE_LFN
            fn = *Finfo.lfname ? Finfo.lfname : Finfo.fname;
#else
            fn = Finfo.fname;
#endif
            if (Finfo.fattrib & AM_DIR) {
                acc_dirs++;
                *(path + i) = '/';
                strcpy(path + i + 1, fn);
                res = scan_files(path);
                *(path + i) = '\0';
                if (res != FR_OK)
                    break;
            } else {
                //      xprintf("%s/%s\n", path, fn);
                acc_files++;
                acc_size += Finfo.fsize;
            }
        }
    }

    return res;
}


//                put_rc(f_mount((uint8_t) p1, &Fatfs[p1]));

static

void put_rc(FRESULT rc)
{
    const char *str =
        "OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
        "INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
        "INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
        "LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0";
    FRESULT i;

    for (i = 0; i != rc && *str; i++) {
        while (*str++);
    }
    xprintf("rc=%u FR_%s\n", (uint32_t) rc, str);
}

int paused;
int stopped;
int previous;
int next;

int isWav(char * filename);

int isWav(char * filename) {
	char * suff = ".WAV";
	int str_len = strlen(filename);
	int suff_len = strlen(suff);

	if (suff_len > str_len) {
		return 0;
	}
	if (strcmp(filename + str_len - suff_len, suff) == 0) {
		return 1;
	}
	return 0;
}

static void displaySong(FILE * lcd, char * filename, int sd_index, int mode_i)
{
    fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
	char * mode_str[] = {"STOPPED", "PAUSED", "PBACK-NORM SPD",
			"PBACK�HALF SPD", "PBACK�DBL SPD", "PBACK�MONO"};

  if (lcd != NULL )
  {
    fprintf(lcd, "\n%s No.: %d\n", filename, sd_index);
    fprintf(lcd, "%s\n", mode_str[mode_i]);
  }
  return;
}

static void timer_end(void * context, alt_u32 id){
	//read the button status
	int status = IORD(BUTTON_PIO_BASE, 0);

	//check if any button is pressed
	int button = status ^ 0xF;
	if (button == 2) {
		paused = paused ^ 1;
	} else if(button == 4) {
		stopped = 1;
	} else if (button == 1) {
		next = 1;
	} else if (button == 8) {
		previous = 1;
	}

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
/***************************************************************************/
/*  main                                                                   */
/***************************************************************************/
int main(void)
{
	//register ISRs
	alt_irq_register(TIMER_0_IRQ, (void*) 0, timer_end);
	alt_irq_register(BUTTON_PIO_IRQ, (void*) 0, button_press);


	//enable all button interrupts
	IOWR(BUTTON_PIO_BASE, 2, 0xF);

	int fifospace;
    char *ptr, *ptr2;
    long p1, p2, p3;
    uint8_t res, b1, drv = 0;
    uint16_t w1;
    uint32_t s1, s2, cnt, blen = sizeof(Buff);
    static const uint8_t ft[] = { 0, 12, 16, 32 };
    uint32_t ofs = 0, sect = 0, blk[2];
    FATFS *fs;                  /* Pointer to file system object */

    alt_up_audio_dev * audio_dev;
    /* used for audio record/playback */
    unsigned int l_buf;
    unsigned int r_buf;
    // open the Audio port
    audio_dev = alt_up_audio_open_dev ("/dev/Audio");
    if ( audio_dev == NULL)
    alt_printf ("Error: could not open audio device \n");
    else
    alt_printf ("Opened audio device \n");

    IoInit();

#if _USE_LFN
    Finfo.lfname = Lfname;
    Finfo.lfsize = sizeof(Lfname);
#endif

    //START YOUR CODE
    char filelist[20][20];
    unsigned long filesizes[20];
    //di, fi, list
    disk_initialize((uint8_t) 0);
    f_mount(0, &Fatfs[0]);

    //get the files
    res = f_opendir(&Dir, "");
	if (res) // if res in non-zero there is an error; print the error.
	{
		put_rc(res);
		return -1;
	}
	s1 = 0;
	for (;;)
	{
		res = f_readdir(&Dir, &Finfo);
		if ((res != FR_OK) || !Finfo.fname[0])
			break;
		if (Finfo.fattrib & AM_DIR)
		{
			continue;
		}
		else
		{
			if (isWav(Finfo.fname) == 1){
				s1++;
				strcpy(filelist[s1 - 1], Finfo.fname);
				filesizes[s1 - 1] = Finfo.fsize;
			}
		}
	}
	FILE *lcd;
	lcd = fopen("/dev/lcd_display", "w");
	int track_num = 0;
	paused = 1;
	while (1) {
		displaySong(lcd, filelist + track_num, track_num, 0); // If paused, but shouldnt matter
		next = 0;
		previous = 0;
		stopped = 0;
		//its stopped. wait for the play button to be pressed
		while (paused) {
			//check for prev/next button to be pressed
			if (previous) {
				track_num --;
				if (track_num < 0) {
					track_num = s1 - 1;
				}
				break;
			}

			if (next) {
				track_num ++;
				if (track_num == s1) {
					track_num = 0;
				}
				break;
			}

		}

		if (previous || next) {
			continue;
		}

		//stops that are pressed at the start don't do anything
		stopped = 0;

		f_open(&File1, filelist[track_num], 1);
		int switch0 = IORD(SWITCH_PIO_BASE, 0) & 0x1;
		int switch1 = IORD(SWITCH_PIO_BASE, 0) & 0x2;
		int mode = switch0 + switch1;
		displaySong(lcd, filelist + track_num, track_num, mode + 2);

		p1 =  filesizes[track_num];

		while (p1)
		{
	/*
		<<<<<<<<<<<<<<<<<<<<<<<<< YOUR fp CODE GOES IN HERE >>>>>>>>>>>>>>>>>>>>>>
	*/

			if ((uint32_t) p1 >= blen)
			{
				cnt = blen;
				p1 -= blen;
			}
			else
			{
				cnt = p1;
				p1 = 0;
			}
			res = f_read(&File1, Buff, cnt, &s2);
			if (res != FR_OK)
			{
				put_rc(res); // output a read error if a read error occurs
				break;
			}
			if (cnt != s2) // We have run out of data in the file
				p1 = 0;
			// So go 32 bits at a time for a right and a left. s2 is the amount of bytes in the buffer
			uint32_t i = 0;
			int repeat_flag = 1;
			while (s2 >= i + 4) {
				//first, check if we have space in the fifo. There needs to be 32 bits of space available. Only must check 1?
				if (alt_up_audio_write_fifo_space(audio_dev, ALT_UP_AUDIO_RIGHT) == 0) {
					continue;
				}
				//read 16 bytes for the left channel and store it in l_buf
				//read 16 bytes for the right channel and store it in r_buf
				// write audio buffer from Buff
				l_buf = 0;
				r_buf = 0;
				memcpy(&l_buf, Buff + i, 2);
				i += 2;
				memcpy(&r_buf, Buff + i, 2);
				i += 2;

				if (mode == 3){
					//mono
					l_buf = r_buf;
				}

				if (paused) {
					displaySong(lcd, filelist + track_num, track_num, 1);
					while (paused) {
						if (stopped) {
							break;
						}
						if (previous) {
							track_num --;
							if (track_num < 0) {
								track_num = s1 - 1;
							}
							stopped = 1;
							//must turn to 0 so it doesnt instant play
							previous = 0;
						}
						if (next) {
							track_num ++;
							if (track_num == s1) {
								track_num = 0;
							}
							stopped = 1;
							next = 0;
						}
					}
					//do not display
					if (stopped) {
						break;
					}

					displaySong(lcd, filelist + track_num, track_num, mode + 2);
				}

				if (stopped) {
					break;
				}
				if (previous) {
					track_num --;
					if (track_num < 0) {
						track_num = s1 - 1;
					}
					break;
				}
				if (next) {
					track_num ++;
					if (track_num == s1) {
						track_num = 0;
					}
					break;
				}
				alt_up_audio_write_fifo (audio_dev, &(r_buf), 1, ALT_UP_AUDIO_RIGHT);
				alt_up_audio_write_fifo (audio_dev, &(l_buf), 1, ALT_UP_AUDIO_LEFT);

				if (mode == 1){
					//half speed, repeat a sample
					if (repeat_flag){
						repeat_flag = 0;
						i -= 4;
					} else {
						repeat_flag = 1;
					}
				} else if(mode == 2){
					//double speed, skip a sample
					i += 4;
				}
			}

			if (stopped) {
				break;
			}
			if (previous || next) {
				break;
			}
		}
		//read to the end, we need to go back to the start, pause it

		f_close(&File1);
		if (previous || next) {
			continue;
		}
		xprintf("done\n");
		paused = 1;
	}

	fclose(lcd);
    return (0);
}
