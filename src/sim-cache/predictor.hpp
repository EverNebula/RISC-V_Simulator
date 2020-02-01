#ifndef PREDICTOR_HEADER
#define PREDICTOR_HEADER

#define PredCacheSize		1024
#define PredTypeNum			5

#include <stdint.h>

enum PRED_TYPE
{
	ALWAYS_NTAKEN, ALWAYS_TAKEN, BIT1_PRED, BIT2_PRED,
	BIT2_PRED_ALT
};

extern char *pred_str[30];

class Predictor
{
public:
	PRED_TYPE mode;
	uint64_t *pred_state;

	Predictor(PRED_TYPE mode);
	~Predictor();

	void Init();
	bool Predict(uint64_t adr);
	void Update(uint64_t adr, bool real);
	char* Name(){ return pred_str[mode]; }
};

#endif
