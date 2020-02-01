#ifndef CONFIG_HEADER
#define CONFIG_HEADER

#include <stdio.h>

extern char *valid_cfg_u32[40];

#define ConfigU32Num		9

enum CFG_U32
{
	SFT_CYC,			// shift
	ADD32_CYC,			// add 32bit
	ADD64_CYC,			// add 64bit
	MUL32_CYC,			// mul 32bit
	MUL64_CYC,			// mul 64bit
	DIV32_CYC,			// div 32bit / rem 32bit
	DIV64_CYC,			// div 64bit / rem 64bit
	CACHE_CYC,			// cache access
	MEM_CYC				// memory access
};

class Config
{
public:
	unsigned u32_cfg[ConfigU32Num];

	Config();
	~Config();
	void LoadConfig(const char *file);
	void Print(FILE *fout = NULL);
};

#endif
