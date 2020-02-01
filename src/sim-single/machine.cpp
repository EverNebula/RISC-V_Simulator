#include <stdio.h>
#include <string.h>
#include "machine.hpp"
#include "utils.hpp"

void SingleStepInfo()
{
	printf("Single Step Mode Help:\n");
	printf("c: continue\n");
	printf("r: print all registers\n");
	printf("d: dump machine status to file 'status_dump.txt'\n");
	printf("p: print page table\n");
	printf("m <address/hex> <size/dec>: get data from address\n");
	printf("q: quit\n\n");
}

void
Machine::SingleStepDebug()
{
	char buf[100], tbuf[100];
	while (true)
	{
		fgets(buf, 100, stdin);

		switch (buf[0])
		{
			case 'c':
				return;
			case 'r':
				PrintReg();
				break;
			case 'd':
				FILE *dump_out;
				dump_out = fopen("status_dump.txt", "w");
				Status(dump_out);
				fclose(dump_out);
				break;
			case 'p':
				PrintPageTable();
				break;
			case 'm':
				uint64_t addr, data;
				int size;
				sscanf(buf, "%s %llx %d", tbuf, &addr, &size);
				printf("%llx %d\n", addr, size);
				if (!ReadMem(addr, size, (void*)&data))
				{
					printf("cannot access that address.\n");
					continue;
				}
				printf("data in %08llx: ", addr);
				for (int i = size - 1; i >= 0 ; --i)
				{
					printf("%02llx ", (data >> (i * 8)) & 0xff);
				}
				printf("\n");
				break;
			case 'q':
				exit(0);
			default:
				printf("unknown command.\n");
				SingleStepInfo();
		}
	}
}

Machine::Machine(bool singleStep)
: singleStep(singleStep)
{
	// register initialization
	memset(reg, 0, sizeof reg);

	// main memory initialization
	mem = new uint8_t[PhysicalMemSize];
	for (int i = 0; i < PhysicalPageNum; ++i)
		freePage.push(i);
	pte = new PageTableEntry[PhysicalPageNum];

	instCount = 1;
	runTime = .0;

	if(singleStep)
		SingleStepInfo();
}

Machine::~Machine()
{
	delete mem;
	delete pte;
}

void
Machine::Run()
{
	Timer timer;

	runTime = 0;
    for ( ; ; )
    {
    	timer.StepTime();
		if (!Fetch())
		{
			panic("Fetch error!\n");
		}
		if (!Decode())
		{
			panic("Decode error!\n");
		}
		if (!Execute())
		{
			panic("Execute error!\n");
		}
		if (!MemoryAccess())
		{
			panic("Memory error!\n");
		}
		if (!WriteBack())
		{
			panic("WriteBack error!\n");
		}
    	WriteReg(0, 0);
		instCount++;
		runTime += timer.StepTime();

    	if (singleStep)
    		SingleStepDebug();
    }

    Status();
}

void
Machine::Status(FILE *fout)
{
	bool isnull = fout == NULL;
	if (isnull)
	{
		fout = stdout;
	}
	fprintf(fout, "\n---------------- Machine ---------------\n");
	fprintf(fout, "BASIC STATUS: \n");
	fprintf(fout, "- Inst. Count:    %d\n", instCount);
	fprintf(fout, "- Run Time:       %.4lf\n", runTime);
	fprintf(fout, "- IpS:            %.2lf\n", (double)instCount/runTime);
	fprintf(fout, "\nREGISTER FILE: \n");
	fprintf(fout, "- Reg. Num        %d\n", RegNum);
	PrintReg(fout);
	fprintf(fout,   "----------------------------------------\n");

	if (isnull)
	{
		fout = fopen("status_dump.txt", "w");
	}
	PrintMem(fout);
	if (isnull)
	{
		printf("memory status dumped to file 'status_dump.txt'.\n");
		fclose(fout);
	}
	else
	{
		printf("machine status dumped.\n");
	}
}

void
Machine::PrintReg(FILE *fout)
{
	if (fout == NULL)
		fout = stdout;
	for (int i = 0; i < RegNum; i += 4)
	{
		for (int j = 0; j < 4; j++)
			fprintf(fout, "[%4s]: 0x%08llx(%10lld)   ", i+j>32?"na":reg_str[i+j], reg[i+j], reg[i+j]);
		fprintf(fout, "\n");
	}
}
