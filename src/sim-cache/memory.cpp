#include "machine.hpp"
#include "utils.hpp"
#include <stdio.h>
#include <string.h>

Memory::Memory()
{
	data = new uint8_t[PhysicalMemSize];
	memset(data, 0, sizeof data);
}

Memory::~Memory()
{
	delete [] data;
}

void
Memory::HandleRequest(uint64_t addr, int bytes, int read,
					uint8_t *content, int &hit, int &time,
					bool prefetching)
{
	// dprintf("memory: requested on 0x%llx, %d bytes, read: %d\n", addr, bytes, read);
	hit = 1;
	time = latency_.hit_latency + latency_.bus_latency;
	stats_.access_time += time;

	if (read) // read
	{
		if (addr + bytes <= PhysicalMemSize)
		{
			memcpy(content, data+addr, bytes);
		}
		else
		{
			printf("[Error] Physical address out of bound. [Memory::HandleRequest]\n");
			exit(0);
		}
	}
	else // write
	{
		if (addr + bytes <= PhysicalMemSize)
		{
			memcpy(data+addr, content, bytes);
		}
		else
		{
			printf("[Error] Physical address out of bound. [Memory::HandleRequest]\n");
			exit(0);
		}
	}
}

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

int
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

	uint8_t buf[10];
	int hit = 0, time = 0;
	// dprintf("before: %llu\n", *(uint64_t*)buf);
	topStorage->HandleRequest(p_addr, size, 1, buf, hit, time);
	// dprintf("after:  %llu\n", *(uint64_t*)buf);
	// printf("%d\n",time);

	uint64_t tmp = 0;
	switch (size)
	{
		case 1:
			*(uint8_t*)value = *(uint8_t*)buf;
			tmp = *(uint8_t*)buf;
			break;
		case 2:
			*(uint16_t*)value = *(uint16_t*)buf;
			tmp = *(uint16_t*)buf;
			break;
		case 4:
			*(uint32_t*)value = *(uint32_t*)buf;
			tmp = *(uint32_t*)buf;
			break;
		case 8:
			*(uint64_t*)value = *(uint64_t*)buf;
			tmp = *(uint64_t*)buf;
			break;
		default:
			vprintf("[Error] Wrong size %d. [ReadMem]\n", size);
			return false;
	}

	// printf("read 0x%llx %llu\n", p_addr, tmp);

	return time;
}

int
Machine::WriteMem(uint64_t addr, int size, uint64_t value, bool memDirect)
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

	uint8_t buf[10];

	switch (size)
	{
		case 1:
			*(uint8_t*)buf = value;
			break;
		case 2:
			*(uint16_t*)buf = value;
			break;
		case 4:
			*(uint32_t*)buf = value;
			break;
		case 8:
			*(uint64_t*)buf = value;
			break;
		default:
			vprintf("[Error] Wrong size %d. [WriteMem]\n", size);
			return false;
	}


	// printf("write 0x%llx %llu\n", p_addr, value);
	int hit = 0, time = 0;
	if (memDirect)
		mainMem->HandleRequest(p_addr, size, 0, buf, hit, time);
	else
	{
		topStorage->HandleRequest(p_addr, size, 0, buf, hit, time);
		// printf("%d\n",time);
	}

	return time;
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
