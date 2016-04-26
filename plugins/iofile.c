#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "common.h"

#define NROFFSET 256
#define PAGESIZE 4096

static char buffer[PAGESIZE];
static int fno = -1;

const char *plugin_name = "IOlatency1";
const char *plugin_desc = "IO latency test";

/*
 * Write the big file with size NROFFSET * PAGESIZE
 */
static int write_file(int fd)
{
	int i = 0;

	for (i = 0; i < NROFFSET; i++)
		if (write(fd, buffer, sizeof(buffer)) < 0) {
			ERROR("write");
			return -1;
		}

	fsync(fd);

	return 0;
}

void *plugin_prerun(void)
{
	char *name;

	if (asprintf(&name, "./iosimul-XXXXXX") < 0) {
		ERROR("Failed to allocate file name\n");
		return (void *)-1;
	}

	fno = mkostemp(name, O_SYNC | O_RDWR);
	if (fno < 0)
		return (void *)-1;

	unlink(name);
	free(name);

	if (write_file(fno))
		return (void *)-1;

	return &fno;
}

static int access_file(int fd)
{
	struct timeval t;
	off_t offset;
	int i, ret;

	/*
	 * Initialize the random seed with the number of
	 * current usec.  This random value will be used to
	 * access the file randomly, no sequential accesses
	 * which can be optimized by the hardware
	 */
	gettimeofday(&t, NULL);
	srandom(t.tv_usec);

	for (i = 0; i < 10000; i++) {

		/*
		 * Compute the offset address to read from
		 */
		offset = (random() % NROFFSET) * PAGESIZE;
		
		/*
		 * man posix_fadvise
		 */
		posix_fadvise(fd, offset, PAGESIZE, 
			      POSIX_FADV_DONTNEED);
		
		/*
		 * Measure the time to read a PAGESIZE buffer
		 */
		ret = random() % 2 ? pread(fd, buffer, PAGESIZE, offset) :
			pwrite(fd, buffer, PAGESIZE, offset);

		if (ret > 0)
			continue;

		ERROR("pread/pwrite: %m\n");
		return ret;
	}

	return 0;
}


int plugin_run(void *arg)
{
	int *fd = arg;
	return access_file(*fd);
}

void plugin_postrun(void *arg)
{
	int *fd = arg;
	close(*fd);
}
