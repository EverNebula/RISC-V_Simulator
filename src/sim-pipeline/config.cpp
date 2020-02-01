#include "machine.hpp"
#include "utils.hpp"
#include <string.h>

char *valid_cfg_u32[40] =
{
	"SFT_CYC",
	"ADD32_CYC",
	"ADD64_CYC",
	"MUL32_CYC",
	"MUL64_CYC",
	"DIV32_CYC",
	"DIV64_CYC",
	"CACHE_CYC",
	"MEM_CYC"		
};

bool InConfigU32(char *idf, int &id)
{
	for (int i = 0; i < ConfigU32Num; ++i)
	{
		if (strcmp(idf, valid_cfg_u32[i]) == 0)
		{
			id = i;
			return true;
		}
	}
	id = -1;
	return false;
}

Config::Config()
{
	for (int i = 0; i < ConfigU32Num; ++i)
		u32_cfg[i] = 1;
}

Config::~Config()
{

}

void
Config::LoadConfig(const char *file)
{
	FILE *fi = fopen(file, "r");

	if (fi == NULL)
	{
		printf("config: Cannot open config file '%s'\n", file);
		return;
	}

	char buf[100];
	char idf[100];
	int id;
	while(fgets(buf, 100, fi))
	{
		if ((strlen(buf) < 3) ||
			(buf[0] == '\\' && buf[1] == '\\'))
			continue;
		sscanf(buf, "%[A-Za-z0-9\_]:", idf);
		if (InConfigU32(idf, id))
		{
			unsigned val;
			sscanf(buf, "%[A-Za-z0-9\_]:%u", idf, &val);
			u32_cfg[id] = val;
			dprintf("config: set u32 config '%s' to value %u\n", valid_cfg_u32[id], val);
		}
	}
}

void
Config::Print(FILE *fout)
{
	if (fout == NULL)
		fout = stdout;
	fprintf(fout, "\n---------------- Config ----------------\n");
	fprintf(fout, "U32 CONFIG:\n");
	for (int i = 0; i < ConfigU32Num; ++i)
	{
		fprintf(fout, "- %s: \t%10u\n", valid_cfg_u32[i], u32_cfg[i]);
	}
	fprintf(fout,   "----------------------------------------\n");
}
