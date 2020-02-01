#ifndef MEMORY_HEADER
#define MEMORY_HEADER

#include <stdint.h>
#include <stdio.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
	TypeName(const TypeName&); \
	void operator=(const TypeName&)

#define GET_CACHE_TAG(addr) \
	(addr / config_.size)

#define GET_CACHE_SET(addr) \
	((addr / config_.line_size) % config_.set_num)

#define GET_TAG_ST_ADDR(tag) \
	(tag * config_.size)

// Storage access stats
typedef struct StorageStats_
{
	int access_counter;
	int miss_num;
	int access_time; // In nanoseconds
	int replace_num; // Evict old lines
	int fetch_num; // Fetch lower layer
	int prefetch_num; // Prefetch
} StorageStats;

// Storage basic config
typedef struct StorageLatency_
{
	int hit_latency; // In nanoseconds
	int bus_latency; // Added to each request
} StorageLatency;

typedef struct CacheConfig_
{
	int size;
	int assoc;
	int set_num; // Number of cache sets
	int write_through; // 0|1 for back|through
	int write_allocate; // 0|1 for no-alc|alc
	int line_size;
} CacheConfig;

class Storage
{
public:
	Storage() {}
	~Storage() {}

	// Sets & Gets
	void SetStats(StorageStats ss) { stats_ = ss; }
	void GetStats(StorageStats &ss) { ss = stats_; }
	void SetLatency(StorageLatency sl) { latency_ = sl; }
	void GetLatency(StorageLatency &sl) { sl = latency_; }

	// Main access process
	// [in]  addr: access address
	// [in]  bytes: target number of bytes
	// [in]  read: 0|1 for write|read
	// [i|o] content: in|out data
	// [out] hit: 0|1 for miss|hit
	// [out] time: total access time
	virtual void HandleRequest(uint64_t addr, int bytes, int read,
														 char *content, int &hit, int &time) = 0;

protected:
	StorageStats stats_;
	StorageLatency latency_;
};

class Memory: public Storage
{
public:
	Memory() {}
	~Memory() {}

	// Main access process
	void HandleRequest(uint64_t addr, int bytes, int read,
	                 char *content, int &hit, int &time);

private:
	// Memory implement

	DISALLOW_COPY_AND_ASSIGN(Memory);
};

typedef struct CacheSet_
{
	bool valid;
	bool dirty;
	int tag;
	int last_vis;
} CacheSet;

class Cache: public Storage
{
public:
	Cache() { total = total_hit = 0; }
	~Cache() {}

	// Sets & Gets
	void SetConfig(CacheConfig cc)
	{
		config_ = cc;
		config_.line_size = cc.size / cc.assoc / cc.set_num;
	}
	void GetConfig(CacheConfig &cc) { cc = config_; }
	void SetLower(Storage *ll) { lower_ = ll; }
	void Allocate();
	// Main access process
	void HandleRequest(uint64_t addr, int bytes, int read,
	                 char *content, int &hit, int &time);

	double GetMissRate() { return (double)(total-total_hit)/total; }

private:
	// Bypassing
	int BypassDecision();
	// Partitioning
	void PartitionAlgorithm();
	// Replacement
	int ReplaceDecision(uint64_t addr);
	int ReplaceAlgorithm(uint64_t addr);
	// Prefetching
	int PrefetchDecision();
	void PrefetchAlgorithm();

	CacheConfig config_;
	Storage *lower_;
	DISALLOW_COPY_AND_ASSIGN(Cache);

	CacheSet *data;
	int total;
	int total_hit;
};

#endif