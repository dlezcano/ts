#ifndef __TS_STATS_H
#define __TS_STATS_H

/*
 * Average on stream :
 * AVG = AVG + (VALUE - AVG) / NRVALUES
 */
#define avg(a, b, i) ((a) + (((b) - (a)) / (i)))

/*
 * Standard deviation:
 * SQRT = SUM_X2 / NRVALUES - (SUM_X * SUM_X)
 */
#define stddev(a, b, i) srqt((b) / (i) - ((a) * (a)))

#endif
