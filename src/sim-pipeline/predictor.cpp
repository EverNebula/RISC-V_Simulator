#include "predictor.hpp"
#include "utils.hpp"

char *pred_str[30] =
{
	"always not taken", "always taken", "1-bit predictor", "2-bit predictor",
	"2-bit predictor alternative"
};

Predictor::Predictor(PRED_TYPE mode) :
mode(mode)
{
	pred_state = new uint64_t[PredCacheSize];
	Init();
}

Predictor::~Predictor()
{
	delete [] pred_state;
}

void
Predictor::Init()
{
	for (int i = 0; i < PredCacheSize; ++i)
	{
		switch (mode)
		{
			case BIT1_PRED:
				pred_state[i] = 1;	// TAKEN
				break;
			case BIT2_PRED:
			case BIT2_PRED_ALT:
				pred_state[i] = 2; // WEAK_TAKEN
				break;
		}
	}
}

bool
Predictor::Predict(uint64_t adr)
{
	uint64_t state = pred_state[(adr >> 2) % PredCacheSize];
	switch (mode)
	{
		case ALWAYS_TAKEN:
			return true;
		case ALWAYS_NTAKEN:
			return false;
		case BIT1_PRED:
			return state;
		case BIT2_PRED:
		case BIT2_PRED_ALT:
			return (state>>1);
	}
	return false;
}

void
Predictor::Update(uint64_t adr, bool real)
{
	uint64_t &state = pred_state[(adr >> 2) % PredCacheSize];
	switch (mode)
	{
		case BIT1_PRED:
			if ((bool)state != real)
				state ^= 1;
			break;
		case BIT2_PRED:
			if (!(state == 0 && !real)
				&& !(state == 3 && real))
				state += real? 1:-1;
			break;
		case BIT2_PRED_ALT:
			switch (state)
			{
				case 0: // STRONG NOT TAKEN
					if (real)
						state = 1;
					break;
				case 1: // WEAK NOT TAKEN
					if (real)
						state = 3;
					else
						state = 0;
					break;
				case 2: // WEAK TAKEN
					if (real)
						state = 3;
					else
						state = 0;
					break;
				case 3:
					if (!real)
						state = 2;
					break;
			}
			break;
	}
}

