#ifndef CACHE_HEADER
#define CACHE_HEADER

#include "storage.hpp"
#include <stdint.h>
#include <stdio.h>

#define GET_CACHE_TAG(addr) \
	(addr / config_.line_size / config_.set_num)

#define GET_CACHE_SET(addr) \
	((addr / config_.line_size) % config_.set_num)

#define GET_CACHE_ADDR(tag, set_id) \
	((tag * config_.set_num + set_id) * config_.line_size)

#define GET_CACHE_ALIGN(addr) \
	((addr / config_.line_size) * config_.line_size)

#define GET_CACHE_OFFSET(addr) \
	(addr % config_.line_size)

typedef struct CacheConfig_
{
	int size;
	int assoc;
	int set_num; // Number of cache sets
	int write_through; // 0|1 for back|through
	int write_allocate; // 0|1 for no-alc|alc
	int line_size;
} CacheConfig;

typedef struct CacheLine_
{
	bool valid;
	bool dirty;
	bool inlru;
	int tag;
	int last_vis;
	uint8_t *data;
} CacheLine;

enum CACHE_METHOD
{
	LRU,
	TWO_QUEUE
};

extern char *cache_method_str[40];

class Cache: public Storage
{
public:
	Cache(char *debug, CACHE_METHOD m = LRU);
	~Cache();

	// Sets & Gets
	void SetConfig(CacheConfig cc)
	{
		config_ = cc;
		config_.set_num = cc.size / cc.assoc / cc.line_size;
	}
	void GetConfig(CacheConfig &cc) { cc = config_; }
	void SetLower(Storage *ll) { lower_ = ll; }
	void SetPrefetch(int p) { if (p > 0 && p <= config_.set_num) pf_num = p; }
	void Allocate();
	// Main access process
	void HandleRequest(uint64_t addr, int bytes, int read,
	                 	uint8_t *content, int &hit, int &time,
	                 	bool prefetching = false);

	void InfoClear() { total = total_hit = 0; }
	void Print(FILE *fout = NULL);
	double MissRate() { return (double)(total-total_hit)/total; }

private:
	// Bypassing
	int BypassDecision(uint64_t addr);
	// Replacement
	int ReplaceDecision(uint64_t addr);
	int ReplaceAlgorithm(uint64_t addr);
	// Prefetching
	int PrefetchDecision();
	void PrefetchAlgorithm(uint64_t addr);

	CacheConfig config_;
	Storage *lower_;
	CACHE_METHOD method;

	CacheLine *lines;
	uint64_t *r_evict;
	int total;
	int total_hit;
	int pf_num;
	bool bypass;
	char name[100];

	DISALLOW_COPY_AND_ASSIGN(Cache);
};

#endif
