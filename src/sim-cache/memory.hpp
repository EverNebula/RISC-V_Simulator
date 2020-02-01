#ifndef MEMORY_HEADER
#define MEMORY_HEADER

#include "storage.hpp"
#include <stdint.h>

class PageTableEntry
{
public:
	uint64_t ppn;
	uint64_t vpn;

	bool valid;
};

class Memory: public Storage
{
public:
	Memory();
	~Memory();

	// Main access process
	void HandleRequest(uint64_t addr, int bytes, int read,
	                 	uint8_t *content, int &hit, int &time,
	                 	bool prefetching = false);

private:
	// Memory implement
	uint8_t *data;

	DISALLOW_COPY_AND_ASSIGN(Memory);
};

#endif
