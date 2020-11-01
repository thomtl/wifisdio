#include <nds.h>
#include <stdio.h>

int main(void) {
	consoleDemoInit();
	lcdSwap();

	while(1) {
		while(fifoCheckDatamsg(FIFO_USER_01)){
			size_t len = fifoCheckDatamsgLength(FIFO_USER_01);
			char* data = calloc(len + 1, 1);
			fifoGetDatamsg(FIFO_USER_01, len, (u8*)data);

			printf("%s", data);

			free(data);
		}

		//swiWaitForVBlank();
		scanKeys();
		if (keysDown() & KEY_START)
			break;
	}

	return 0;
}
