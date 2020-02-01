#include "machine.hpp"
#include "utils.hpp"
#include <stdio.h>

bool
Machine::AllocatePage(uint64_t vpn)
{
	// virutal memory - TODO
	if (freePage.empty())
	{
		vprintf("[Error] Run out of memory. [AllocatePage]\n");
		return false;
	}

	uint64_t ppn = freePage.top();
	freePage.pop();

	pte[ppn].valid = true;
	pte[ppn].ppn = ppn;
	pte[ppn].vpn = vpn;
	pageTable[vpn] = (pte+ppn);

	dprintf("Allocated physical page 0x%08llx for vp 0x%08llx.\n", ppn, vpn);

	return true;
}

bool
Machine::ReadMem(uint64_t addr, int size, void *value)
{
	uint64_t p_addr;

	if (!Translate(addr, &p_addr, size))
	{
		vprintf("--Translate error. [ReadMem]\n");
		return false;
	}

	if (p_addr > PhysicalMemSize)
	{
		vprintf("[Error] Physical address out of bound. [ReadMem]\n");
		return false;
	}

	uint64_t tmp = 0;
	switch (size)
	{
		case 1:
			*(uint8_t*)value = *(uint8_t*)(mem + p_addr);
			tmp = *(uint8_t*)(mem + p_addr);
			break;
		case 2:
			*(uint16_t*)value = *(uint16_t*)(mem + p_addr);
			tmp = *(uint16_t*)(mem + p_addr);
			break;
		case 4:
			*(uint32_t*)value = *(uint32_t*)(mem + p_addr);
			tmp = *(uint32_t*)(mem + p_addr);
			break;
		case 8:
			*(uint64_t*)value = *(uint64_t*)(mem + p_addr);
			tmp = *(uint64_t*)(mem + p_addr);
			break;
		default:
			vprintf("[Error] Wrong size %d. [ReadMem]\n", size);
			return false;
	}

	printf("read 0x%llx %llu\n", addr, tmp);

	return true;
}

bool
Machine::WriteMem(uint64_t addr, int size, uint64_t value)
{
	uint64_t p_addr;

	if (!Translate(addr, &p_addr, size))
	{
		vprintf("--Translate error. [WriteMem]\n");
		return false;
	}

	if (p_addr > PhysicalMemSize)
	{
		vprintf("[Error] Physical address out of bound. [WriteMem]\n");
		return false;
	}

	switch (size)
	{
		case 1:
			*(uint8_t*)(mem + p_addr) = value;
			break;
		case 2:
			*(uint16_t*)(mem + p_addr) = value;
			break;
		case 4:
			*(uint32_t*)(mem + p_addr) = value;
			break;
		case 8:
			*(uint64_t*)(mem + p_addr) = value;
			break;
		default:
			vprintf("[Error] Wrong size %d. [WriteMem]\n", size);
			return false;
	}

	printf("write 0x%llx %llu\n", addr, value);

	return true;
}

bool
Machine::Translate(uint64_t v_addr, uint64_t *p_addr, int size)
{
	// check alignment
	if ((size == 4 && (v_addr & 0x3)) || (size == 2 && (v_addr & 0x1)))
	{
		vprintf("[Error] Alignment error, size = %d but addr = 0x%llx. [Translate]\n"
			    , size, v_addr);
		return false;
	}

	// translate
	uint64_t vpn = v_addr / PageSize, offset = v_addr % PageSize;

	if (pageTable.find(vpn) == pageTable.end())
	{
		vprintf("[Error] Page not found, vpn = 0x%llx. [Translate]\n", vpn);
		return false;
	}

	PageTableEntry *entry = pageTable[vpn];
	if (!entry->valid)
	{
		// TODO - load on use
		vprintf("Page Fault (page invalid), vpn = 0x%llx. [Translate]\n", vpn);
		return false;
	}

	*p_addr = entry->ppn * PageSize + offset;
	return true;
}

void
Machine::PrintPageTable(FILE *fout)
{
	if (fout == NULL)
		fout = stdout;
	fprintf(fout, "\nPAGE TABLE: \n");
	fprintf(fout, "vpn         ppn         valid\n");
	std::map<uint64_t, PageTableEntry*>::iterator it = pageTable.begin();
	while (it != pageTable.end())
	{
		PageTableEntry* nowpte = (*it).second;
		fprintf(fout, "0x%08llx  0x%08llx  %d\n", nowpte->vpn, nowpte->ppn, nowpte->valid);
		it++;
	}
}

void
Machine::PrintMem(FILE *fout, bool no_data)
{
	if (fout == NULL)
		fout = stdout;
	fprintf(fout, "\n---------------- Memory ----------------\n");
	fprintf(fout, "BASIC STATUS: \n");
	fprintf(fout, "- Page Size:       %d\n", PageSize);
	fprintf(fout, "- Phys. Mem Size:  %lld (%d * %d)\n", PhysicalMemSize, PhysicalPageNum, PageSize);
	int usePage = PhysicalPageNum - freePage.size();
	fprintf(fout, "- Phys. Mem Use:   %3.2lf%% (%d / %d)\n", (double)usePage/PhysicalPageNum, usePage, PhysicalPageNum);
	fprintf(fout, "- Stack Size:      %d\n", StackPageNum * PageSize);
	fprintf(fout, "- Stack Top Ptr.:  0x%08llx\n", StackTopPtr);

	if (no_data)
	{
		fprintf(fout,   "----------------------------------------\n");
		return;
	}

	fprintf(fout, "\nPAGE TABLE: \n");
	fprintf(fout, "vpn         ppn         valid\n");
	std::map<uint64_t, PageTableEntry*>::iterator it = pageTable.begin();
	while (it != pageTable.end())
	{
		PageTableEntry* nowpte = (*it).second;
		fprintf(fout, "0x%08llx  0x%08llx  %d\n", nowpte->vpn, nowpte->ppn, nowpte->valid);
		it++;
	}

	fprintf(fout, "\nDATA: \n");
	const int stepSize = 4;
	it = pageTable.begin();
	while (it != pageTable.end())
	{
		PageTableEntry* nowpte = (*it).second;
		if (nowpte->valid)
		{
			fprintf(fout, "- vpn 0x%08llx [in 0x%08llx]\n", nowpte->vpn, nowpte->ppn);
			uint64_t baseaddr = nowpte->vpn * PageSize, offset = 0;
			for (; offset < PageSize; offset += stepSize)
			{
				fprintf(fout, " 0x%08llx\t\t", baseaddr + offset);
				uint64_t data;
				ReadMem(baseaddr + offset, 4, (void*)&data);
				for (int i = stepSize - 1; i >= 0 ; --i)
				{
					fprintf(fout, "%02llx ", (data >> (i * 8)) & 0xff);
				}
				fprintf(fout, "\n");
			}
		}
		it++;
	}
	fprintf(fout,   "----------------------------------------\n");
}