#include "utils.hpp"
#include "memory.hpp"

void
Memory::HandleRequest(uint64_t addr, int bytes, int read,
					char *content, int &hit, int &time)
{
	hit = 1;
	time = latency_.hit_latency + latency_.bus_latency;
	stats_.access_time += time;
}

void
Cache::HandleRequest(uint64_t addr, int bytes, int read,
					char *content, int &hit, int &time)
{
	int vic_id = 0;
	int lower_hit, lower_time;

	hit = 0;
	time = 0;
	// Bypass?
	if (!BypassDecision())
	{
		total++;
		PartitionAlgorithm();
		// Miss?
		if ((vic_id = ReplaceDecision(addr)) == -1)
		{
			if (!read && !config_.write_allocate) // no-write allocate
			{
				lower_->HandleRequest(addr, bytes, read, content,
									lower_hit, lower_time);
				time += lower_time;
				return;
			}

			// Choose victim
			vic_id = ReplaceAlgorithm(addr);
			hit = 0;

			// dirty writeback
			if (data[vic_id].valid && data[vic_id].dirty)
			{
				char *buf = NULL;
				lower_->HandleRequest(GET_TAG_ST_ADDR(data[vic_id].tag), config_.line_size, 0, buf,
									lower_hit, lower_time);
				time += lower_time;
			}
			data[vic_id].valid = true;
			data[vic_id].dirty = false;
		}
		else
		{
			total_hit++;
			// return hit & time
			hit = 1;
			time += latency_.bus_latency + latency_.hit_latency;
			stats_.access_time += latency_.bus_latency + latency_.hit_latency;

			if (!read && config_.write_through) // write through
			{
				lower_->HandleRequest(addr, bytes, read, content,
									lower_hit, lower_time);
				time += lower_time;
			}
			else
			{
				data[vic_id].dirty = true;
			}

			return;
		}
	}

	// Prefetch?
	if (PrefetchDecision())
	{
		PrefetchAlgorithm();
	} 
	else
	{
		// Fetch from lower layer
		lower_->HandleRequest(addr, bytes, read, content,
							lower_hit, lower_time);
		time += latency_.bus_latency + lower_time;
		stats_.access_time += latency_.bus_latency;

		if (!read) // write allocate
		{
			;
		}
	}
}

int
Cache::BypassDecision()
{
	return false;
}

void
Cache::PartitionAlgorithm() 
{
}

int
Cache::ReplaceDecision(uint64_t addr) 
{
	uint64_t set_id = GET_CACHE_SET(addr);
	uint64_t tag = GET_CACHE_TAG(addr);
	CacheSet *d = data + set_id*config_.set_num;

	for (int i = 0; i < config_.assoc; ++i)
		if (d[i].valid && d[i].tag == tag)
			return i + set_id*config_.set_num;

	return -1;
}

int
Cache::ReplaceAlgorithm(uint64_t addr)
{
	// now LRU algorithm

	uint64_t set_id = GET_CACHE_SET(addr);
	uint64_t tag = GET_CACHE_TAG(addr);
	CacheSet *d = data + set_id*config_.set_num;

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
		dprintf("cache: (%llx-%llx) was replaced.\n", d[vic_id].tag, set_id);
	}
	d[vic_id].last_vis = config_.assoc;
	d[vic_id].tag = tag;
	for (int i = 0; i < config_.assoc; ++i)
		d[i].last_vis--;

	dprintf("cache: 0x%llx(%llx-%llx) loaded in.\n", addr, tag, set_id);
	return vic_id + set_id*config_.set_num;
}

int
Cache::PrefetchDecision() 
{
	return false;
}

void
Cache::PrefetchAlgorithm() 
{
}

void
Cache::Allocate()
{
	int tot = config_.assoc * config_.set_num;
	data = new CacheSet[tot];
	for (int i = 0; i < tot; ++i)
	{
		data[i].last_vis = 0;
		data[i].tag = 0;
		data[i].valid = false;
		data[i].dirty = false;
	}
}
