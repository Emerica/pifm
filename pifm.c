// To run:
// gcc -lm -std=c99 -g -xc pifm.c -o fm
// Access from ARM Running Linux

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
int  shutdown;
int  mem_fd;
char *gpio_mem, *gpio_map;
char *spi0_mem, *spi0_map;


// I/O access
volatile unsigned *gpio;
volatile unsigned *allof7e;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
#define GPIO_GET *(gpio+13)  // sets   bits which are 1 ignores bits which are 0

#define ACCESS(base) *(volatile int*)((int)allof7e+base-0x7e000000)
#define SETBIT(base, bit) ACCESS(base) |= 1<<bit
#define CLRBIT(base, bit) ACCESS(base) &= ~(1<<bit)

void setup_io();

#define CM_GP0CTL (0x7e101070)
#define GPFSEL0 (0x7E200000)
#define CM_GP0DIV (0x7e101074)
#define DMABASE (0x7E007000)

struct GPCTL {
    char SRC         : 4;
    char ENAB        : 1;
    char KILL        : 1;
    char             : 1;
    char BUSY        : 1;
    char FLIP        : 1;
    char MASH        : 2;
    unsigned int     : 13;
    char PASSWD      : 8;
};

void setup_fm(int state)
{
    allof7e = (unsigned *)mmap(
                  NULL,
                  0x01000000,  //len
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  mem_fd,
                  0x20000000  //base
              );

    if ((int)allof7e==-1) exit(-1);
    SETBIT(GPFSEL0 , 14);
    CLRBIT(GPFSEL0 , 13);
    CLRBIT(GPFSEL0 , 12);
    struct GPCTL setupword = {6/*SRC*/, state, 0, 0, 0, state,0x5a};
    ACCESS(CM_GP0CTL) = *((int*)&setupword);
}
void shutdown_fm()
{
    if(!shutdown){
        shutdown=1;
        printf("\nShutting Down\n");
        setup_fm(0);
        exit(0);
    }
}
void delay( int i)
{
    for(volatile unsigned int j = 1.25*(1<<i); j; j--);
}

void modulate(int m,int mod)
{
    ACCESS(CM_GP0DIV) = (0x5a << 24) + mod + m;
}
//Emerica - removed seeking in reverse for reading stdin
void playWav(char* filename,int mod,float bw)
{
    int fp;
    fp = open(filename, 'r');
    lseek(fp, 22, SEEK_SET); //Skip header??
    short* data = (short*)malloc(1024);
    unsigned int rnd=1;
    printf("Now broadcasting: %s\n",filename);
    while (read(fp, data, 1024)) {
	for (int j=0; j<1024/2; j++){
            float dval = (float)(data[j])/65536.0*bw;
            int intval = (int)(floor(dval));
            float frac = dval - (float)intval;
            unsigned int fracval = (unsigned int)(frac*((float)(1<<16))*((float)(1<<16)));
            for (int i=0; i<270; i++) {
                rnd = (rnd >> 1) ^ (-(rnd & 1u) & 0xD0000001u);
                modulate( intval + (fracval>rnd?1:0),mod);
            }
	}
    }
}

struct CB {
    unsigned int TI;
    unsigned int SOURCE_AD;
    unsigned int DEST_AD;
    unsigned int TXFR_LEN;
    unsigned int STRIDE;
    unsigned int NEXTCONBK;
};

struct DMAregs {
    unsigned int CS;
    unsigned int CONBLK_AD;
    unsigned int TI;
    unsigned int SOURCE_AD;
    unsigned int DEST_AD;
    unsigned int TXFR_LEN;
    unsigned int STRIDE;
    unsigned int NEXTCONBK;
    unsigned int DEBUG;
};


int main(int argc, char **argv)
{
    int g,rep;
    // Set up gpi pointer for direct register access
    signal(SIGTERM, &shutdown_fm);
    signal(SIGINT, &shutdown_fm);
    atexit(&shutdown_fm);
    setup_io();
    // Switch GPIO 7..11 to output mode
    /************************************************************************\
     * You are about to change the GPIO settings of your computer.          *
     * Mess this up and it will stop working!                               *
     * It might be a good idea to 'sync' before running this program        *
     * so at least you still have your code changes written to the SD-card! *
    \************************************************************************/
    // Set GPIO pins 7-11 to output
    for (g=7; g<=11; g++) {
        INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
        //OUT_GPIO(g);
    }
    setup_fm(1);
    float freq = atof(argv[2]);
    float bw;
    int mod = (500/freq)*4096;
    modulate(0,mod);
    if (argc==3){
      bw = 25.0;
      printf("Setting up modulation:%f Mhz / %d @ %f\n",freq,mod,bw);
      playWav(argv[1],mod,bw);
    }else if (argc==4){
      bw = atof(argv[3]);
      printf("Setting up modulation:%f Mhz / %d @ %f\n",freq,mod,bw);
      playWav(argv[1],mod,bw);
    }else{
      fprintf(stderr, "Usage:  %s wavfile.wav freq [power]\n\nWhere wavfile is 16 bit 22.050kHz Mono\nPower will default to 25 if not specified. It should only be lowered!",argv[0]);
    }
    return 0;
} // main


//
// Set up a memory regions to access GPIO
//
void setup_io()
{
    /* open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        printf("can't open /dev/mem \n");
        exit (-1);
    }
    /* mmap GPIO */
    // Allocate MAP block
    if ((gpio_mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
        printf("allocation error \n");
        exit (-1);
    }
    // Make sure pointer is on 4K boundary
    if ((unsigned long)gpio_mem % PAGE_SIZE)
        gpio_mem += PAGE_SIZE - ((unsigned long)gpio_mem % PAGE_SIZE);
    // Now map it
    gpio_map = (unsigned char *)mmap(
                   gpio_mem,
                   BLOCK_SIZE,
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED|MAP_FIXED,
                   mem_fd,
                   GPIO_BASE
               );
    if ((long)gpio_map < 0) {
        printf("mmap error %d\n", (int)gpio_map);
        exit (-1);
    }
    // Always use volatile pointer!
    gpio = (volatile unsigned *)gpio_map;
} // setup_io
