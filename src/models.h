/* models.h */
#ifndef MODELS_H
#define MODELS_H

/* Include files */
#include "config.h"

#include <sys/types.h>
#include <assert.h>

#include "lc_common.h"

/* Standard definitions */
/* The structured model proper */
enum
{
	MODEL_BASIS,
	MODEL_2_3,
	MODEL_4_7,
	MODEL_8_15,
	MODEL_16_31,
	MODEL_32_63,
	MODEL_64_127,
	MODEL_128_255,

	MODEL_LAST
};

enum
{
	VAL_RUNA,
	VAL_RUNB,
	VAL_ONE,
	VAL_2_3,
	VAL_4_7,
	VAL_8_15,
	VAL_16_31,
	VAL_32_63,
	VAL_64_127,
	VAL_128_255,
	VAL_EOB
};

#define MAX_SYMBOLS		256

/* Type definitions */
/*
 * The counts for symbols [0..numSymbols[ are 
 * stored at freq[0] .. freq[numSymbols - 1].
 *
 * Presumably one should make sure that 
 * ((incVal + noExceed) / 2) < noExceed
 * so that scaling always produces sensible
 * results.
 *
 * We take incValue == 0 to indicate that the
 * counts shouldn't be incremented or scaled.
 */
struct Model
{
	unsigned const numSymbols, incValue, noExceed;
	unsigned totFreq, freq[MAX_SYMBOLS];
};

struct MTFVals_encode_st
{
	unsigned v;
	struct Model *m;
	unsigned n;
};

struct MTFVals_decode_st
{
	struct Model *m;
	unsigned n;
};

/* Function prototypes */
extern void initModels(void);
extern void initBogusModel(void);

static inline void scaleModel(struct Model *m);
static inline void updateModel(struct Model *m, unsigned symbol);

/* Global variables */
extern struct Model model_bogus, models[];
extern struct MTFVals_encode_st const MTFVals_encode[];
extern struct MTFVals_decode_st const MTFVals_decode[];

/* Function definitions */
void scaleModel(struct Model *m)
{
	unsigned i;

	m->totFreq = 0;
	for (i = 0; i < m->numSymbols; i++)
	{
		m->freq[i]++;
		m->freq[i] >>= 1;
		m->totFreq += m->freq[i];
	}
} /* scaleModel */

void updateModel(struct Model *m, unsigned symbol)
{
	assert(INRANGE(symbol, 0, m->numSymbols - 1));

	m->freq[symbol] += m->incValue;
	m->totFreq += m->incValue;
	if (m->totFreq > m->noExceed)
		scaleModel(m);
} /* updateModel */

#endif /* MODELS_H */
