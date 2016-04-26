#ifndef __TS_TOPOLOGY_H
#define __TS_TOPOLOGY_H
struct thread {
	int os_id;
	int thread_id;
};

struct core {
	int os_id;
	int core_id;
	int nrthreads;
	struct thread *thread;
};

struct package {
	int nrcores;
	int package_id;
	struct core *core;
};

struct topology {
	int nrpackages;	
	struct package *package;
};

extern struct topology *topology_init(void);
extern void topology_fini(struct topology *topology);
#endif
