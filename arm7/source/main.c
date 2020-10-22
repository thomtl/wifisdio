#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "wifisdio/wifisdio.h"

volatile bool exitflag = false;

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

	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);

	setPowerButtonCB(powerButtonCB);
}

void print(const char* c){
	fifoSendDatamsg(FIFO_USER_01, strlen(c), (uint8_t*)c);
}

int main() {
	init_arm7();

	sdio_atheros_init();

	while (!exitflag) {
		if ((REG_KEYINPUT & KEY_START) == 0)
			exitflag = true;
		
		swiWaitForVBlank();
	}

	return 0;
}
