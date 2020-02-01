#include "machine.hpp"
#include "utils.hpp"
#include <stdio.h>
#include <string.h>

char *cache_method_str[40] =
{
	"LRU",
	"TWO_QUEUE"
};

Cache::Cache(char *debug, CACHE_METHOD m)
{
	total = total_hit = 0;
	pf_num = 1;
	bypass = 0;
	method = m;
	strcpy(name, debug);
}

Cache::~Cache()
{

}

void
Cache::HandleRequest(uint64_t addr, int bytes, int read,
					uint8_t *content, int &hit, int &time,
					bool prefetching)
{
	// dprintf("%s: requested on 0x%llx, %d bytes, read: %d\n", name, addr, bytes, read);
	int vic_id = 0;
	int lower_hit, lower_time;

	if (!prefetching) total++;
	hit = 0;
	time = 0;
	// Bypass?
	if (!BypassDecision(addr))
	{
		// cache miss
		if ((vic_id = ReplaceDecision(addr)) == -1)
		{
			// dprintf("%s: miss.\n", name);
			if (!read && !config_.write_allocate) // no-write allocate
			{
				lower_->HandleRequest(addr, bytes, read, content,
									lower_hit, lower_time, prefetching);
				time += lower_time;
				return;
			}

			// Choose victim
			vic_id = ReplaceAlgorithm(addr);
			hit = 0;

			// recently evicted
			if (lines[vic_id].valid)
				r_evict[GET_CACHE_SET(addr)] = lines[vic_id].tag;

			// dirty writeback
			if (lines[vic_id].valid && lines[vic_id].dirty)
			{
				lower_->HandleRequest(GET_CACHE_ADDR(lines[vic_id].tag, vic_id/config_.assoc), config_.line_size, 0,
									lines[vic_id].data, lower_hit, lower_time, prefetching);
				time += lower_time;
			}
			lines[vic_id].valid = true;
			lines[vic_id].dirty = false;
			lines[vic_id].tag = GET_CACHE_TAG(addr);
		}

		// cache hit
		else 
		{
			// dprintf("%s: hit.\n", name);
			if (!prefetching) total_hit++;
			// return hit & time
			hit = 1;
			time += latency_.bus_latency + latency_.hit_latency;
			stats_.access_time += latency_.bus_latency + latency_.hit_latency;

			if (!read) // write
			{
				// copy contents
				if (bytes <= config_.line_size)
				{
					memcpy(lines[vic_id].data + GET_CACHE_OFFSET(addr), content,
							bytes);
				}
				else
				{
					printf("[Error] Write size is bigger than line size. (%d > %d)\n",
							bytes, config_.line_size);
					exit(0);
				}
				
				if (config_.write_through) // write through
				{
					lower_->HandleRequest(addr, bytes, read, content,
										lower_hit, lower_time, prefetching);
					time += lower_time;
				}	
				else
				{
					lines[vic_id].dirty = true;
				}
			}

			else // read
			{
				if (bytes <= config_.line_size)
				{
					memcpy(content, lines[vic_id].data + GET_CACHE_OFFSET(addr),
							bytes);
				}
				else
				{
					printf("[Error] Read size is bigger than line size. (%d > %d)\n",
							bytes, config_.line_size);
					exit(0);
				}
			}

			return;
		}
	}
	else // bypass
	{
		lower_->HandleRequest(addr, bytes, read, content,
							lower_hit, lower_time, prefetching);
		time += lower_time;
		return;
	}

	// Prefetch?
	if (!prefetching && PrefetchDecision())
	{
		PrefetchAlgorithm(GET_CACHE_ADDR(lines[vic_id].tag, vic_id/config_.assoc));
	}

	{

		if (!read) // write first
		{
			lower_->HandleRequest(addr, bytes, 0, content, 
								lower_hit, lower_time, prefetching);
			time += latency_.bus_latency + lower_time;
			stats_.access_time += latency_.bus_latency;
		}

		// Fetch from lower layer
		lower_->HandleRequest(GET_CACHE_ALIGN(addr), config_.line_size, 1, lines[vic_id].data,
							lower_hit, lower_time, prefetching);
		if (read)
		{
			time += latency_.bus_latency + lower_time;
			stats_.access_time += latency_.bus_latency;
		}

		// write allocate or read miss
		// dprintf("%s: vic_id %d offset 0x%llx\n", name, vic_id, GET_CACHE_OFFSET(addr));
		if (read)
		{
			if (bytes <= config_.line_size)
			{
				memcpy(content, lines[vic_id].data + GET_CACHE_OFFSET(addr),
						bytes);
			}
			else
			{
				printf("[Error] R/W size is bigger than line size. (%d > %d)\n",
						bytes, config_.line_size);
				exit(0);
			}
		}
	}
}

int
Cache::BypassDecision(uint64_t addr)
{
	if (!bypass) return false;
	uint64_t tag = GET_CACHE_TAG(addr);
	uint64_t set_id = GET_CACHE_SET(addr);

	CacheLine *d = lines + set_id*config_.assoc;
	for (int i = 0; i < config_.assoc; ++i)
		if (!d[i].valid || d[i].tag == tag) // cold miss
			return false;

	if (r_evict[set_id] == tag) // capacity miss
	{
		printf("here conflict miss 0x%x\n", tag);
		// r_evict[set_id] = tag;
		return true;
	}
	return false;
}

int
Cache::ReplaceDecision(uint64_t addr) 
{
	uint64_t set_id = GET_CACHE_SET(addr);
	uint64_t tag = GET_CACHE_TAG(addr);
	CacheLine *d = lines + set_id*config_.assoc;

	for (int i = 0; i < config_.assoc; ++i)
		if (d[i].valid && d[i].tag == tag)
		{
			if (method == TWO_QUEUE && !d[i].inlru) // push into lru queue
			{
				d[i].inlru = true;
			}

			// update
			d[i].last_vis = config_.assoc;
			for (int j = 0; j < config_.assoc; ++j)
				d[j].last_vis--;

			return i + set_id*config_.assoc;
		}

	return -1;
}

int
Cache::ReplaceAlgorithm(uint64_t addr)
{
	// now LRU algorithm

	uint64_t set_id = GET_CACHE_SET(addr);
	uint64_t tag = GET_CACHE_TAG(addr);
	CacheLine *d = lines + set_id*config_.assoc;

	if (method == LRU)
	{
		int vic_id = -1;
		for (int i = 0; i < config_.assoc; ++i)
			if (!d[i].valid)
			{
				vic_id = i;
				break;
			}

		if (vic_id == -1)
		{
			int min_vis = config_.assoc;
			for (int i = 0; i < config_.assoc; ++i)
				if (d[i].last_vis < min_vis)
				{
					vic_id = i;
					min_vis = d[i].last_vis;
				}
		}

		if (d[vic_id].valid)
		{
			// dprintf("%s: (%llx-%llx) was replaced.\n", name, d[vic_id].tag, set_id);
		}
		d[vic_id].last_vis = config_.assoc;
		for (int i = 0; i < config_.assoc; ++i)
			d[i].last_vis--;

		// dprintf("%s: 0x%llx(%llx-%llx) loaded in.\n", name, addr, tag, set_id);
		return vic_id + set_id*config_.assoc;
	}

	if (method == TWO_QUEUE)
	{
		int vic_id = -1;
		for (int i = 0; i < config_.assoc; ++i)
			if (!d[i].valid)
			{
				vic_id = i;
				break;
			}

		if (vic_id == -1)
		{
			int min_vis = config_.assoc;
			for (int i = 0; i < config_.assoc; ++i)
				if (!d[i].inlru && d[i].last_vis < min_vis) // in fifo queue
				{
					vic_id = i;
					min_vis = d[i].last_vis;
				}

			if (vic_id == -1) // find in lru queue
			{
				for (int i = 0; i < config_.assoc; ++i)
					if (d[i].last_vis < min_vis) // in lru queue
					{
						vic_id = i;
						min_vis = d[i].last_vis;
					}	
			}
		}

		if (d[vic_id].valid)
		{
			// dprintf("%s: (%llx-%llx) was replaced.\n", name, d[vic_id].tag, set_id);
		}
		d[vic_id].inlru = false;
		d[vic_id].last_vis = config_.assoc;
		for (int i = 0; i < config_.assoc; ++i)
			d[i].last_vis--;

		// dprintf("%s: 0x%llx(%llx-%llx) loaded in.\n", name, addr, tag, set_id);
		return vic_id + set_id*config_.assoc;
	}
}

int
Cache::PrefetchDecision() 
{
	return pf_num > 1;
}

void
Cache::PrefetchAlgorithm(uint64_t addr) 
{
	int hit, lower_time;
	uint8_t *buf;
	buf = new uint8_t[config_.line_size];
	for (int i = 1; i < pf_num; ++i)
	{
		addr += config_.line_size; // next block
		this->HandleRequest(addr, config_.line_size, 1, buf,
							hit, lower_time, true);
	}
	delete [] buf;
}

void
Cache::Allocate()
{
	int tot = config_.assoc * config_.set_num;
	lines = new CacheLine[tot];
	for (int i = 0; i < tot; ++i)
	{
		lines[i].last_vis = 0;
		lines[i].tag = 0;
		lines[i].valid = false;
		lines[i].dirty = false;
		lines[i].inlru = false;
		lines[i].data = new uint8_t[config_.line_size];
		memset(lines[i].data, 0, config_.line_size);
	}
	r_evict = new uint64_t[config_.set_num];
}

void
Cache::Print(FILE *fout)
{
	bool isnull = fout == NULL;
	if (isnull)
	{
		fout = stdout;
	}

	fprintf(fout, "- Cache Size:        %d\n", config_.size);
	fprintf(fout, "- Block Size:        %d\n", config_.line_size);
	fprintf(fout, "- Set Number:        %d\n", config_.set_num);
	fprintf(fout, "- Associativity:     %d\n", config_.assoc);
	fprintf(fout, "- Replace Method:    %s\n", cache_method_str[method]);
	fprintf(fout, "- Prefetch Num:      %d\n", pf_num);
	fprintf(fout, "- Access Times:      %d\n", total);
	fprintf(fout, "- Miss Times:        %d   (HIT:%d)\n", total - total_hit, total_hit);
	fprintf(fout, "- Miss Rate:         %.2lf %%\n", (double)(total - total_hit)/total*100);
	fprintf(fout, "  %s\t[%s]\n", config_.write_through? "[Write Through]":"[Write Back]   ",
										config_.write_allocate? "Write Alloc":"No-write Alloc");
}

