#ifndef __TS_ENERGY_H
#define __TS_ENERGY_H

#define ENERGY_CORE_SUPPORTED    0x1
#define ENERGY_NONCORE_SUPPORTED 0x2
#define ENERGY_PKG_SUPPORTED     0x4
#define ENERGY_DRAM_SUPPORTED    0x8
#define ENERGY_GPU_SUPPORTED     0x10

struct energy_pkg {
	double pkg;
	double core;
	double noncore;
};

struct energy_sys {
	double dram;
	double gpu;
};

struct energy {
	void *data;
	void *handle;
	int flags;
	struct energy_sys sys;
	struct energy_pkg *pkg;
	struct topology *topology;
};

struct topology;

extern struct energy *energy_init(struct topology *);

extern int  energy_read(struct energy *);

extern void energy_fini(struct energy *);

extern void energy_delta(struct energy *, struct energy *, struct energy *);

extern struct energy *energy_clone(struct energy *);

extern double energy_cost(struct energy *);

extern void energy_free(struct energy *);

#endif
