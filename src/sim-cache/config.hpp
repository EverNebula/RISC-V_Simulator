#ifndef CONFIG_HEADER
#define CONFIG_HEADER

#include <stdio.h>

extern char *valid_cfg_u32[40];

#define ConfigU32Num		35

enum CFG_U32
{
	SFT_CYC,			// shift
	ADD32_CYC,			// add 32bit
	ADD64_CYC,			// add 64bit
	MUL32_CYC,			// mul 32bit
	MUL64_CYC,			// mul 64bit
	DIV32_CYC,			// div 32bit / rem 32bit
	DIV64_CYC,			// div 64bit / rem 64bit
	L1C_HIT_CYC,
	L2C_HIT_CYC,
	L3C_HIT_CYC,
	L1C_BUS_CYC,
	L2C_BUS_CYC,
	L3C_BUS_CYC,
	MEM_CYC,			// memory access
	L1C_SIZE,
	L1C_ASSOC,
	L1C_BSIZE,
	L1C_WT,
	L1C_WA,
	L1C_METHOD,
	L1C_PREFETCH,
	L2C_SIZE,
	L2C_ASSOC,
	L2C_BSIZE,
	L2C_WT,
	L2C_WA,
	L2C_METHOD,
	L2C_PREFETCH,
	L3C_SIZE,
	L3C_ASSOC,
	L3C_BSIZE,
	L3C_WT,
	L3C_WA,
	L3C_METHOD,
	L3C_PREFETCH
};

class Config
{
public:
	unsigned u32_cfg[ConfigU32Num];

	Config();
	~Config();
	void LoadConfig(const char *file);
	void Print(FILE *fout = NULL);
	unsigned GetConfig(char *name);
};

#endif
