#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <openssl/md5.h>

#include "trace.h"
#include "topology.h"
#include "energy.h"

struct ts_plugin_results {
	const char *path;
	const char *md5sum;
	double duration;
	double energy;
};

struct ts_results {
	int nr_results;
	double duration;
	double energy;
	struct ts_plugin_results *tspr;
};

static char *md5sum_to_str(const unsigned char *md5sum, char *buffer)
{
	int i;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++)
		snprintf(&(buffer[i*2]), MD5_DIGEST_LENGTH*2, "%02x", (unsigned int)md5sum[i]);

	return buffer;
}

static const char *md5sum(const char *path)
{
	MD5_CTX c;
	char *buffer;
	unsigned char md5sum[MD5_DIGEST_LENGTH];
	size_t bytes = 1;
	FILE *f;

	buffer = malloc(512);
	if (!buffer)
		FATAL("Failed to allocate md5sum buffer\n");

	if (!MD5_Init(&c))
		FATAL("Failed to initialize md5 context\n");

	f = fopen(path, "r");
	if (!f) {
		ERROR("Failed to open '%s' for reading\n", path);
		return NULL;
	}

	while (bytes > 0) {
		bytes = read(fileno(f), buffer, sizeof(buffer));
		if (bytes > 0 && !MD5_Update(&c, buffer, bytes)) {
			ERROR("Failed to update md5 context\n");
			return NULL;
		}
	}

	if (!MD5_Final(md5sum, &c)) {
		ERROR("Failed to finalize md5\n");
		return NULL;
	}

	fclose(f);

	buffer = md5sum_to_str(md5sum, buffer);

	DEBUG("md5sum '%s' => '%s'\n", path, buffer);

	return buffer;
}

int results_update(struct ts_results *tsr, const char *path, double duration, double energy)
{
	struct ts_plugin_results *tspr = tsr->tspr;

	tspr = realloc(tspr, (tsr->nr_results + 1)*sizeof(*tspr));
	if (!tspr)
		FATAL("Failed to allocate memory for results\n");

	tspr[tsr->nr_results].path = strdup(path);
	tspr[tsr->nr_results].md5sum = md5sum(path);
	tspr[tsr->nr_results].energy = energy;
	tspr[tsr->nr_results].duration = duration;
	tsr->tspr = tspr;
	tsr->nr_results++;
	tsr->energy += energy;
	tsr->duration += duration;
	
	return 0;
}

static struct ts_plugin_results *results_find(const char *name, struct ts_results *tsr)
{
	int i;
	struct ts_plugin_results *tspr = tsr->tspr;

	for (i = 0; i < tsr->nr_results; i++)
		if (!strcmp(name, tspr[i].path))
			return &tspr[i];

	return NULL;
}

#define ratio(v1, v2) ((((v2) - (v1)) / (v1)) * 100)

int results_compare(struct ts_results *tsr1, struct ts_results *tsr2)
{
	int i;
	struct ts_plugin_results *tspr1, *tspr2;

	if (!tsr1 || !tsr2) {
		CRITICAL("Null energy result list\n");
		return -1;
	}

	if (tsr1->nr_results != tsr2->nr_results)
		WARNING("There is a different number of results\n");

	tspr1 = tsr1->tspr;
	tspr2 = tsr2->tspr;

	if (!tspr1 || !tspr2) {
		CRITICAL("No plugin results !\n");
		return -1;
	}

	for (i = 0; i < tsr1->nr_results; i++) {

		struct ts_plugin_results *tspr;
		char *name = basename(tsr1->tspr[i].path);

		tspr = results_find(tspr1[i].path, tsr2);
		if (!tspr) {
			WARNING("Failed to find plugin '%s' result to compare\n", name);
			continue;
		}

		DEBUG("'%s': %.0lf / %.0lf usecs\n",
		      name, tspr1[i].duration, tspr->duration);
		DEBUG("'%s': %lf / %lf uJ\n", name, tspr1[i].energy, tspr->energy);

		NOTICE("'%s': %+.2lf%% usecs / %+.2lf%% uJ\n",
		       name, ratio(tspr1[i].duration, tspr->duration),
		       ratio(tspr1[i].energy, tspr->energy));
	}

	DEBUG("Overall time: %.0lf / %.0lf usecs\n",
	      tsr1->duration, tsr2->duration);
	DEBUG("Overall energy: %lf / %lf uJ\n", tsr1->energy, tsr2->energy);

	NOTICE("Overall: %+.2lf%% usecs / %+.2lf%% uJ\n",
	       ratio(tsr1->duration, tsr2->duration),
	       ratio(tsr1->energy, tsr2->energy));

	return 0;
}

int results_publish(struct ts_results *tsr)
{
	struct ts_plugin_results *tspr;
	int i;

	if (!tsr)
		return -1;

	tspr = tsr->tspr;
	if (!tspr)
		FATAL("Something is wrong, no plugins result\n");

	for (i = 0; i < tsr->nr_results; i++) {
		NOTICE("%s: %.0lf usecs / %lf uJoules\n", tspr[i].path,
		       tspr[i].duration, tspr[i].energy);
	}

	NOTICE("Overall: %.0lf usecs, %lf uJoules\n", tsr->duration, tsr->energy);

	return 0;
}

struct ts_results *results_alloc(void)
{
	struct ts_results *tsr;

	tsr = malloc(sizeof(*tsr));
	if (tsr)
		memset(tsr, 0, sizeof(*tsr));

	return tsr;
}

void results_free(struct ts_results *tsr)
{
	free(tsr);
}

struct ts_results *results_load(const char *path)
{
	char name[4096];
	double energy, duration;
	char md5sum[512];
	int nr_results;
	struct ts_results *tsr;
	FILE *f;
	int i;

	tsr = results_alloc();
	if (!tsr) {
		FATAL("Failed to allocate memory for results");
		return NULL;
	}

	f = fopen(path, "r");
	if (!f) {
		ERROR("Failed to open file '%s'\n", path);
		return NULL;
	}

	if (fread(&nr_results, sizeof(nr_results), 1, f) != 1) {
		ERROR("Failed to result the number of results data\n");
		return NULL;
	}

	for (i = 0; i < nr_results; i++) {

		size_t len;

		if (fread(&len, sizeof(len), 1, f) < 1) {
			ERROR("Failed to read plugin name length\n");
			return NULL;
		}

		if (fread(name, len + 1, 1, f) < 1) {
			ERROR("Failed to read plugin name\n");
			return NULL;
		}

		if (fread(&len, sizeof(len), 1, f) < 1) {
			ERROR("Failed to read plugin name length\n");
			return NULL;
		}

		if (fread(md5sum, len + 1, 1, f) < 1) {
			ERROR("Failed to read plugin md5sum\n");
			return NULL;
		}

		if (fread(&duration, sizeof(duration), 1, f) < 1) {
			ERROR("Failed to read plugin duration results\n");
			return NULL;
		}

		if (fread(&energy, sizeof(energy), 1, f) < 1) {
			ERROR("Failed to read plugin energy results\n");
			return NULL;
		}

		if (results_update(tsr, name, duration, energy)) {
			ERROR("Failed to update results\n");
			return NULL;
		}

		if (tsr->tspr[i].md5sum && strcmp(tsr->tspr[i].md5sum, md5sum))
			WARNING("md5sum differs on '%s', was it modified ?\n", name);
	}

	fclose(f);

	return tsr;
}

int results_save(const char *path, struct ts_results *tsr)
{
	FILE *f;
	int i;

	if (!tsr) {
		ERROR("Wrong results passed as parameter\n");
		return -1;
	}

	if (!tsr->tspr) {
		ERROR("No plugin results\n");
		return -1;
	}

	f = fopen(path, "w");
	if (!f) {
		ERROR("Failed to open file '%s'\n", path);
		return -1;
	}

	if (fwrite(&tsr->nr_results, sizeof(tsr->nr_results), 1, f) != 1) {
		ERROR("Failed to write results data\n");
		return -1;
	}

	for (i = 0; i < tsr->nr_results; i++) {

		struct ts_plugin_results *tspr = &tsr->tspr[i];
		size_t len;

		len = strlen(tspr->path);
		if (fwrite(&len, sizeof(len), 1, f) < 1) {
			ERROR("Failed to write plugin results\n");
			return -1;
		}

		if (fwrite(tspr->path, len + 1, 1, f) < 1) {
			ERROR("Failed to write plugin results\n");
			return -1;
		}

		len = strlen(tspr->md5sum);
		if (fwrite(&len, sizeof(len), 1, f) < 1) {
			ERROR("Failed to write plugin results\n");
			return -1;
		}

		if (fwrite(tspr->md5sum, len + 1, 1, f) < 1) {
			ERROR("Failed to write plugin results\n");
			return -1;
		}

		if (fwrite(&tspr->duration, sizeof(tspr->duration), 1, f) < 1) {
			ERROR("Failed to write plugin results\n");
			return -1;
		}

		if (fwrite(&tspr->energy, sizeof(tspr->energy), 1, f) < 1) {
			ERROR("Failed to write plugin results\n");
			return -1;
		}
	}

	fclose(f);

	return 0;
}
