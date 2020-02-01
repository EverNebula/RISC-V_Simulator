#include <stdio.h>
#include <string.h>
#include "machine.hpp"
#include "utils.hpp"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

Machine::Machine(PRED_TYPE mode)
{
	// register initialization
	memset(reg, 0, sizeof reg);

	// main memory initialization
	mem = new uint8_t[PhysicalMemSize];
	for (int i = 0; i < PhysicalPageNum; ++i)
		freePage.push(i);
	pte = new PageTableEntry[PhysicalPageNum];

	predictor = new Predictor(mode);

	cycCount = 0;
	cpuCount = 0;
	instCount = 0;
	runTime = .0;

    loadHzdCount = 0;
    ctrlHzdCount = 0;
    ecallStlCount = 0;
    jalrStlCount = 0;
    totalBranch = 0;
}

Machine::~Machine()
{
	delete mem;
	delete pte;
	delete predictor;
}

void
Machine::Run()
{
	Timer timer;

	runTime = 0;
	F_reg.bubble = false;

	if(singleStep)
		SingleStepInfo();
    for ( ; ; )
    {
    	int mxCyc = 1, useCyc;
    	timer.StepTime();
		if ((useCyc = Fetch()) == 0)
		{
			panic("Fetch error!\n");
		}
		else
		{
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = Decode()) == 0)
		{
			panic("Decode error!\n");
		}
		else
		{
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = Execute()) == 0)
		{
			panic("Execute error!\n");
		}
		else
		{
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = MemoryAccess()) == 0)
		{
			panic("Memory error!\n");
		}
		else
		{
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = WriteBack()) == 0)
		{
			panic("WriteBack error!\n");
		}
		else
		{
			mxCyc = MAX(mxCyc, useCyc);
		}

		UpdatePipeline();

		cycCount++;
		cpuCount += mxCyc;
		runTime += timer.StepTime();

		if (debug)
		{
			PrintReg();
		}

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
	fprintf(fout, "- Cycle Count:       %d\n", cycCount);
	fprintf(fout, "- Inst. Count:       %d\n", instCount);
	fprintf(fout, "- Run Time:          %.4lf\n", runTime);
	fprintf(fout, "- Pipeline Cyc CpI:  %.2lf\n", (double)cycCount/instCount);
	fprintf(fout, "- CPU Cyc CpI:       %.2lf\n\n", (double)cpuCount/instCount);

	fprintf(fout, "- Pred Strategy:     %s\n", predictor->Name());
	fprintf(fout, "- Branch Pred Acc:   %.2lf%%\t(%d / %d)\n", (double)(totalBranch - ctrlHzdCount)/totalBranch*100,
															totalBranch - ctrlHzdCount, totalBranch);
	fprintf(fout, "- Load-use Hazard:   %d\n", loadHzdCount);
	fprintf(fout, "- Ctrl. Hazard:      %d * 2\t(cycles)\n", ctrlHzdCount);
	fprintf(fout, "- ECALL Stall:       %d * 3\t(cycles)\n", ecallStlCount);
	fprintf(fout, "- JALR Stall:        %d * 2\t(cycles)\n", jalrStlCount);
	
	fprintf(fout, "\nREGISTER FILE: \n");
	fprintf(fout, "- Reg. Num           %d\n", RegNum);
	PrintReg(fout);
	fprintf(fout,   "----------------------------------------\n");

	cfg.Print(fout);

	if (isnull)
	{
		PrintMem(NULL, true);
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
			fprintf(fout, "[%4s]: 0x%08llx(%10lld)   ", i+j>33?"na":reg_str[i+j], reg[i+j], reg[i+j]);
		fprintf(fout, "\n");
	}
}
