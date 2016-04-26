#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/utsname.h>

#include "../trace.h"
#include "../topology.h"
#include "../energy.h"

/* 
 * MSR_RAPL_POWER_UNIT provides the following information across all
 * RAPL domains:
 *
 * - Power Units (bits 3:0): Power related information (in Watts) is
 * based on the multiplier, 1/ 2^PU; where PU is an unsigned integer
 * represented by bits 3:0. Default value is 0011b, indicating power
 * unit is in 1/8 Watts increment
 *
 * - Energy Status Units (bits 12:8): Energy related information (in
 * Joules) is based on the multiplier, 1/2^ESU; where ESU is an
 * unsigned integer represented by bits 12:8. Default value is 10000b,
 * indicating energy status unit is in 15.3 micro-Joules increment
 *
 * - Time Units (bits 19:16): Time related information (in Seconds) is
 * based on the multiplier, 1/ 2^TU; where TU is an unsigned integer
 * represented by bits 19:16. Default value is 1010b, indicating time
 * unit is in 976 microseconds increment
 *
 */
#define MSR_RAPL_POWER_UNIT         0x606
#define MSR_RAPL_PU_MASK            0xf
#define MSR_RAPL_ESU_MASK           0x1f
#define MSR_RAPL_TU_MASK            0xf

/*
 * MSR_PKG_ENERGY_STATUS is a read-only MSR. It reports the actual
 * energy use for the package domain. This MSR is updated every ~1msec
 * (rapl time unit). It has a wraparound time of around 60 secs when
 * power consumption is high, and may be longer otherwise.
 * 
 * MSR_PP0_ENERGY_STATUS/MSR_PP1_ENERGY_STATUS is a read-only MSR. It
 * reports the actual energy use for the respective power plane
 * domain. This MSR is updated every ~1msec.
 *
 * MSR_DRAM_ENERGY_STATUS is a read-only MSR. It reports the actual
 * energy use for the DRAM domain. This MSR is updated every ~1msec.
 *
 * - Total Energy Consumed (bits 31:0): The unsigned integer value
 * represents the total amount of energy consumed since that last time
 * this register is cleared. The unit of this field is specified by the
 * 'Energy Status Units' field of MSR_RAPL_POWER_UNIT.
 *
 */
#define MSR_PKG_ENERGY_STATUS  0x611
#define MSR_DRAM_ENERGY_STATUS 0x619
#define MSR_PP0_ENERGY_STATUS  0x639
#define MSR_PP1_ENERGY_STATUS  0x641

struct rapl {
	int flags;
	double pu;  /* Power Unit         */
	double esu; /* Energy Status Unit */
	double tu;  /* Time Unit          */
};

static int msr_read(int cpu, unsigned long offset, uint64_t *value)
{
	FILE *file;
	char *msrpath;
	int ret;

	if (asprintf(&msrpath, "/dev/cpu/%d/msr", cpu) < 0) {
		CRITICAL("Failed to allocate msr's path\n");
		return -1;
	}

	file = fopen(msrpath, "r");
	if (!file)
		goto out_free;

	if (fseek(file, offset, SEEK_CUR))
		goto out_fclose;

	if (fread(value, sizeof(*value), 1, file) != 1)
		goto out_fclose;

	ret = 0;
out_fclose:
	fclose(file);
out_free:
	free(msrpath);

	return ret;
}	

static uint64_t rapl_read_units(int cpu)
{
	uint64_t value;

	if (msr_read(cpu, MSR_RAPL_POWER_UNIT, &value)) {
		ERROR("Failed to read power unit from msr\n");
		return -1;
	}

	return value;
}

static double rapl_read_energy(int cpu, struct rapl *rapl, int domain)
{
        uint64_t value;                                                                        

	if (msr_read(cpu, domain, &value)) {
		ERROR("Failed to read domain energy from msr\n");
		return -1;
	}

	return value * rapl->esu;
}

static double rapl_core_energy(int pkgid, struct topology *topology, struct rapl *rapl)
{
	int cpu = topology->package[pkgid].core[0].os_id;;
	return rapl_read_energy(cpu, rapl, MSR_PP0_ENERGY_STATUS);
}

static double rapl_noncore_energy(int pkgid, struct topology *topology, struct rapl *rapl)
{
	int cpu = topology->package[pkgid].core[0].os_id;;
	return rapl_read_energy(cpu, rapl, MSR_PP1_ENERGY_STATUS);
}

static double rapl_package_energy(int pkgid, struct topology *topology, struct rapl *rapl)
{
	int cpu = topology->package[pkgid].core[0].os_id;;
	return rapl_read_energy(cpu, rapl, MSR_PKG_ENERGY_STATUS);
}

static double rapl_dram_energy(int pkgid, struct topology *topology, struct rapl *rapl)
{
	int cpu = topology->package[pkgid].core[0].os_id;;
	return rapl_read_energy(cpu, rapl, MSR_DRAM_ENERGY_STATUS);
}

static double rapl_gpu_energy(int pkgid, struct topology *topology, struct rapl *rapl)
{
	return -1; /* I don't have yet the offset */
}

static inline double rapl_pu(uint64_t units)
{
	return 1.0 / (1 << (units & MSR_RAPL_PU_MASK));
}

static inline double rapl_esu(uint64_t units)
{
	return 1.0 / (1 << ((units >> 8) & MSR_RAPL_ESU_MASK));
}

static inline double rapl_tu(uint64_t units)
{
	return 1.0 / (1 << ((units >> 16) & MSR_RAPL_TU_MASK));
}

int sensor_probe(void)
{
	struct utsname utsname;
	const char *machine[] = {
		"i386", "i686", "x86_64",
	};
	int i, found = 0;
	uint64_t value;

	if (uname(&utsname)) {
		ERROR("Failed to utsname\n");
		return -1;
	}

	for (i = 0; i < sizeof(machine) / sizeof(machine[0]) && !found; i++) {
		if (!strcmp(machine[i], utsname.machine))
			found = 1;
	}

	if (!found)
		return -1;

	if (msr_read(0, 0, &value)) {
		ERROR("Failed to read msr (modprobe msr ? permission ?)\n");
		return -1;
	}

	return 0;
}

void sensor_fini(struct energy *energy)
{
	struct rapl *rapl = energy->data;
	free(rapl);
}

int sensor_read(struct energy *energy)
{
	int i;
	struct topology *topology = energy->topology;
	struct rapl *rapl = energy->data;

	for (i = 0; i < topology->nrpackages; i++) {

		if (energy->flags & ENERGY_CORE_SUPPORTED)
			energy->pkg[i].core = rapl_core_energy(i, topology, &rapl[i]);

		if (energy->flags & ENERGY_NONCORE_SUPPORTED)
			energy->pkg[i].noncore = rapl_noncore_energy(i, topology, &rapl[i]);

		if (energy->flags & ENERGY_PKG_SUPPORTED)
			energy->pkg[i].pkg= rapl_package_energy(i, topology, &rapl[i]);
	}

	if (energy->flags & ENERGY_DRAM_SUPPORTED)
		energy->sys.dram = rapl_dram_energy(0, topology, &rapl[i]);

	if (energy->flags & ENERGY_GPU_SUPPORTED)
		energy->sys.gpu = rapl_gpu_energy(0, topology, &rapl[i]);

	return 0;
}

int sensor_init(struct energy *energy)
{
	struct topology *topology = energy->topology;
	struct rapl *rapl;
	uint64_t value;
	int i;

	rapl = calloc(topology->nrpackages, sizeof(*rapl));
	if (!rapl)
		return -1;

	for (i = 0; i < topology->nrpackages; i++) {

		value = rapl_read_units(i);
		if (value < 0)
			goto out_free;

		rapl[i].pu  = rapl_pu(value);
		rapl[i].esu = rapl_esu(value);
		rapl[i].tu  = rapl_tu(value);

		if (rapl_core_energy(i, topology, &rapl[i]) >= 0)
			energy->flags |= ENERGY_CORE_SUPPORTED;

		if (rapl_noncore_energy(i, topology, &rapl[i]) >= 0)
			energy->flags |= ENERGY_NONCORE_SUPPORTED;

		if (rapl_package_energy(i, topology, &rapl[i]) >= 0)
			energy->flags |= ENERGY_PKG_SUPPORTED;

		if (rapl_dram_energy(i, topology, &rapl[i]) >= 0)
			energy->flags |= ENERGY_DRAM_SUPPORTED;

		if (rapl_gpu_energy(i, topology, &rapl[i]) >= 0)
			energy->flags |= ENERGY_GPU_SUPPORTED;

	}
	energy->data = rapl;

	return 0;
out_free:
	free(rapl);
	return -1;
}
