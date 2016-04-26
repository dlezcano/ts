#include "common.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

const char *plugin_name = "Load_200";
const char *plugin_desc = "200 ms load and idle time";
static int buffer[16 * 1024 * 1024];
volatile sig_atomic_t intr = 0;

static void sighandler(int sig)
{
	intr = 1;
}

int plugin_run(void *arg)
{
	int i;
	unsigned long itv = 200000;

	struct itimerval itval = {
		.it_value = { 
			.tv_sec = 0,
			.tv_usec = itv, 
		},
		.it_interval = {
			.tv_sec = 0,
			.tv_usec = itv,
		},
	};

	struct sigaction sa = { .sa_handler = sighandler, };

	sigaction (SIGVTALRM, &sa, NULL);

	for (i = 0; i < 20; i++) {

		if (setitimer(ITIMER_VIRTUAL, &itval, NULL)) {
			ERROR("Failed to set timer\n: %m");
			return -1;
		}

		while(!intr) {
			int k;
			for (k = 0; k < (sizeof(buffer) / sizeof(buffer[0])); k++)
				buffer[k] = 0xDEADBEEF;
		}
		
		intr = 0;

		usleep(itv);
	}

	return 0;
}
