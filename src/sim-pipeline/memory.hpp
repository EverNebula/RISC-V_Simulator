#ifndef MEMORY_HEADER
#define MEMORY_HEADER

#include <stdint.h>

class PageTableEntry
{
public:
	uint64_t ppn;
	uint64_t vpn;

	bool valid;
};

#endif
