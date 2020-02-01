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
	"L1C_HIT_CYC",
	"L2C_HIT_CYC",
	"L3C_HIT_CYC",
	"L1C_BUS_CYC",
	"L2C_BUS_CYC",
	"L3C_BUS_CYC",
	"MEM_CYC",
	"L1C_SIZE",
	"L1C_ASSOC",
	"L1C_BSIZE",
	"L1C_WT",
	"L1C_WA",
	"L1C_METHOD",
	"L1C_PREFETCH",
	"L2C_SIZE",
	"L2C_ASSOC",
	"L2C_BSIZE",
	"L2C_WT",
	"L2C_WA",
	"L2C_METHOD",
	"L2C_PREFETCH",
	"L3C_SIZE",
	"L3C_ASSOC",
	"L3C_BSIZE",
	"L3C_WT",
	"L3C_WA",
	"L3C_METHOD",
	"L3C_PREFETCH",
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
	{
		u32_cfg[i] = 1;
	}
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

unsigned
Config::GetConfig(char *name)
{
	int id = -1;
	for (int i = 0; i < ConfigU32Num; ++i)
	{
		if (strcmp(name, valid_cfg_u32[i]) == 0)
		{
			id = i;
			break;
		}
	}
	if (id == -1)
	{
		printf("[Warning] config: Cannot find config '%s'.\n", name);
		return -1;
	}

	return u32_cfg[id];
}
