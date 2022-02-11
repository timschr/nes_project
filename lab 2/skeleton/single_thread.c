#include <stdio.h>
#include <stdlib.h>

#define RUNNING 0
#define DONE    1

int protothread() {
	static int  state = 0;
	switch(state) {
	case 0:
		printf("thread 1");
		state++;
	case 1:
		printf("thread 2");
		state++;
	case 2:
		printf("thread 3");
		state++;
	case 3:
		printf("Done!\n");
		return DONE;
	}
}

int main(int argc, char *argv[]) {

	int i = 0;
	int ret;

	do {
		i++;
		ret = protothread();

	} while (ret != DONE);

	printf("thread stopped after %d calls\n", i);

	return 0;
}
