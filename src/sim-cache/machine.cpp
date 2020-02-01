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

	// page table initialization
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

    l1cache = NULL;
    l2cache = NULL;
    l3cache = NULL;
}

Machine::~Machine()
{
	delete mainMem;
	delete l1cache;

	delete pte;
	delete predictor;
}

void Machine::StorageInit(int cacheLevel)
{
	// storage initialization
    StorageLatency ll;
    StorageStats s;
    CacheConfig l1c;
    s.access_time = 0;

    mainMem = new Memory();
    mainMem->SetStats(s);
    ll.bus_latency = 0;
    ll.hit_latency = cfg.GetConfig("MEM_CYC");
    mainMem->SetLatency(ll);

	if (cacheLevel > 2)
	{
	    l3cache = new Cache("L3 cache", (CACHE_METHOD)cfg.GetConfig("L3C_METHOD"));
	    l3cache->SetStats(s);
	    ll.bus_latency = cfg.GetConfig("L3C_BUS_CYC");
	    ll.hit_latency = cfg.GetConfig("L3C_HIT_CYC");
	    l3cache->SetLatency(ll);

	    l1c.size = cfg.GetConfig("L3C_SIZE");
	    l1c.assoc = cfg.GetConfig("L3C_ASSOC");
	    l1c.line_size = cfg.GetConfig("L3C_BSIZE"); // Size of cache line
	    l1c.write_through = (bool)cfg.GetConfig("L3C_WT"); // 0|1 for back|through
	    l1c.write_allocate = (bool)cfg.GetConfig("L3C_WA"); // 0|1 for no-alc|alc
	    l3cache->SetConfig(l1c);
    	l3cache->SetPrefetch(cfg.GetConfig("L3C_PREFETCH"));
	    l3cache->Allocate();
    	l3cache->SetLower(mainMem);
	}
    if (cacheLevel > 1)
    {
	    l2cache = new Cache("L2 cache", (CACHE_METHOD)cfg.GetConfig("L2C_METHOD"));
	    l2cache->SetStats(s);
	    ll.bus_latency = cfg.GetConfig("L2C_BUS_CYC");
	    ll.hit_latency = cfg.GetConfig("L2C_HIT_CYC");
	    l2cache->SetLatency(ll);

	    l1c.size = cfg.GetConfig("L2C_SIZE");
	    l1c.assoc = cfg.GetConfig("L2C_ASSOC");
	    l1c.line_size = cfg.GetConfig("L2C_BSIZE"); // Size of cache line
	    l1c.write_through = (bool)cfg.GetConfig("L2C_WT"); // 0|1 for back|through
	    l1c.write_allocate = (bool)cfg.GetConfig("L2C_WA"); // 0|1 for no-alc|alc
	    l2cache->SetConfig(l1c);
    	l2cache->SetPrefetch(cfg.GetConfig("L2C_PREFETCH"));
	    l2cache->Allocate();
	    if (cacheLevel > 2)
    		l2cache->SetLower(l3cache);
    	else
    		l2cache->SetLower(mainMem);
    }
    if (cacheLevel > 0)
    {
    	l1cache = new Cache("L1 cache", (CACHE_METHOD)cfg.GetConfig("L1C_METHOD"));
    	l1cache->SetStats(s);
	    ll.bus_latency = cfg.GetConfig("L1C_BUS_CYC");
	    ll.hit_latency = cfg.GetConfig("L1C_HIT_CYC");
	    l1cache->SetLatency(ll);

	    l1c.size = cfg.GetConfig("L1C_SIZE");
	    l1c.assoc = cfg.GetConfig("L1C_ASSOC");
	    l1c.line_size = cfg.GetConfig("L1C_BSIZE"); // Size of cache line
	    l1c.write_through = (bool)cfg.GetConfig("L1C_WT"); // 0|1 for back|through
	    l1c.write_allocate = (bool)cfg.GetConfig("L1C_WA"); // 0|1 for no-alc|alc
	    l1cache->SetConfig(l1c);
    	l1cache->SetPrefetch(cfg.GetConfig("L1C_PREFETCH"));
	    l1cache->Allocate();
	    if (cacheLevel > 1)
    		l1cache->SetLower(l2cache);
    	else
    		l1cache->SetLower(mainMem);
    }

	if (cacheLevel != 0)
    	topStorage = l1cache;
    else
    	topStorage = mainMem;
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
			vprintf("- Fetch CPU Cyc: %d\n", useCyc);
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = Decode()) == 0)
		{
			panic("Decode error!\n");
		}
		else
		{
			vprintf("- Decode CPU Cyc: %d\n", useCyc);
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = Execute()) == 0)
		{
			panic("Execute error!\n");
		}
		else
		{
			vprintf("- Execute CPU Cyc: %d\n", useCyc);
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = MemoryAccess()) == 0)
		{
			panic("Memory error!\n");
		}
		else
		{
			vprintf("- Memory CPU Cyc: %d\n", useCyc);
			mxCyc = MAX(mxCyc, useCyc);
		}

		if ((useCyc = WriteBack()) == 0)
		{
			panic("WriteBack error!\n");
		}
		else
		{
			vprintf("- WriteBack CPU Cyc: %d\n", useCyc);
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

	// cfg.Print(fout);

	fprintf(fout, "\n----------------- Cache ----------------\n");
	fprintf(fout, "L1 Cache: \n");
	if (l1cache)
	{
		l1cache->Print(fout);
	}
	else
	{
		fprintf(fout, "  None\n");
	}
	fprintf(fout, "\nL2 Cache: \n");
	if (l2cache)
	{
		l2cache->Print(fout);
	}
	else
	{
		fprintf(fout, "  None\n");
	}
	fprintf(fout, "\nL3 Cache: \n");
	if (l3cache)
	{
		l3cache->Print(fout);
	}
	else
	{
		fprintf(fout, "  None\n");
	}
	fprintf(fout,   "----------------------------------------\n");

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
