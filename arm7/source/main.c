#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "wifisdio/wifisdio.h"
#include "wifisdio/wmi.h"
#include "wifisdio/net/udp.h"

volatile bool exitflag = false;
volatile uint32_t arm7_count_60hz = 0;

void VblankHandler() { arm7_count_60hz++; }
void VcountHandler() { inputGetAndSend(); }
void powerButtonCB() { exitflag = true; }

void init_arm7() {
	readUserSettings();
	ledBlink(0);

	irqInit();
	initClockIRQ(); // Start the RTC IRQ
	fifoInit();
	touchInit();

	SetYtrigger(80);

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);

	setPowerButtonCB(powerButtonCB);
}

void put(const char* s) {
	fifoSendDatamsg(FIFO_USER_01, strlen(s), (uint8_t*)s);
}


TWL_BSS char print_buf[100] = {0};

void print(const char* s, ...){
	va_list va;
	va_start(va, s);
	vsnprintf(print_buf, 100, s, va);
	va_end(va);

	put(print_buf);
}

void panic(const char* s, ...) {
	va_list va;
	va_start(va, s);
	vsnprintf(print_buf, 100, s, va);
	va_end(va);

	put(print_buf);

	while(1)
		;
}

int main() {
	init_arm7();

	sdio_init();
	print("SDIO: Init\n");

	while (!exitflag) {
		sdio_poll_mbox(0);
		if ((REG_KEYINPUT & KEY_START) == 0)
			exitflag = true;
		
		swiWaitForVBlank();
	}

	return 0;
}
