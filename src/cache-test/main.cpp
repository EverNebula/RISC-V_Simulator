#include "utils.hpp"
#include "memory.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;

#include <boost/program_options.hpp>
using namespace boost::program_options;

Memory *mainMem;
Cache *l1cache;

bool singleStep = false;
bool writeAllocate = false;
bool writeThrough = false;
string fileName;

void ParseArg(int argc, char *argv[])
{
    options_description opts("RISC-V Simulator Options");

    opts.add_options()
        (",s", "single step")
        ("verbose,v", "print more info")
        ("debug,d", "print debug info")
        (",a", "write allocate")
        (",t", "write through")
        ("filename,f", value<string>()->required(), "trace file")
        ("help,h", "print help info")
        ;
    variables_map vm;
    
    store(parse_command_line(argc, argv, opts), vm);

    if(vm.count("help"))
    {
        cout << opts << endl;
        return;
    }

    if(vm.count("-s"))
    {
        singleStep = true;
        verbose = true;
    }

    if(vm.count("-a"))
    {
        writeAllocate = true;
        verbose = true;
    }

    if(vm.count("-t"))
    {
        writeThrough = true;
        verbose = true;
    }

    if(vm.count("debug"))
    {
        debug = true;
    }

    if(vm.count("verbose"))
    {
        verbose = true;
    }    

    //--filename tmp.txt
    if (vm.count("filename"))
    {
        fileName = vm["filename"].as<string>();
    }
    else
    {
        printf("please use -f to specify the trace file.\n");
        exit(0);
    }

    printf("[verbose]  %s\n", verbose?"on":"off");
    printf("[debug]    %s\n", debug?"on":"off");
    printf("[single]   %s\n\n", singleStep?"on":"off");
}

int main(int argc, char *argv[])
{
    // parse args
    ParseArg(argc, argv);

    // run trace
    mainMem = new Memory();
    l1cache = new Cache();
    l1cache->SetLower(mainMem);

    StorageStats s;
    s.access_time = 0;
    mainMem->SetStats(s);
    l1cache->SetStats(s);

    StorageLatency ml;
    ml.bus_latency = 6;
    ml.hit_latency = 100;
    mainMem->SetLatency(ml);

    StorageLatency ll;
    ll.bus_latency = 3;
    ll.hit_latency = 10;
    l1cache->SetLatency(ll);

    CacheConfig l1c;
    l1c.size = 32 * 1024;
    l1c.assoc = 1;
    l1c.set_num = 512; // Number of cache sets
    l1c.write_through = writeThrough; // 0|1 for back|through
    l1c.write_allocate = writeAllocate; // 0|1 for no-alc|alc
    l1cache->SetConfig(l1c);
    l1cache->Allocate();


    // int hit, time;
    // char content[64];
    // l1cache->HandleRequest(0, 0, 1, content, hit, time);
    // printf("Request access time: %dns\n", time);
    // l1cache->HandleRequest(1024, 0, 1, content, hit, time);
    // printf("Request access time: %dns\n", time);

    // l1cache->GetStats(s);
    // printf("Total L1 access time: %dns\n", s.access_time);
    // mainMem->GetStats(s);
    // printf("Total Memory access time: %dns\n", s.access_time);
    // return 0;

    FILE *trace = fopen(fileName.c_str(), "r");
    if (trace == NULL)
    {
        printf("can not open file %s.\n", fileName.c_str());
        exit(0);
    }

    int hit, time, tot_time = 0;
    char content[64];
    
    char buf[100];
    int cnt = 0;
    while(fgets(buf, 100, trace))
    {
        char op[10];
        uint64_t addr;
        sscanf(buf, "%s %llx", op, &addr);
        // printf("%s %llu\n", op, addr);
        switch (op[0])
        {
            case 'r':
                l1cache->HandleRequest(addr, 0, 1, content, hit, time);
                break;
            case 'w':
                l1cache->HandleRequest(addr, 0, 0, content, hit, time);
                break;
            default:
                printf("unknown command.\n");
                exit(0);
        }
        tot_time += time;
        cnt++;
    }
    printf("[trace end] miss rate: %.2lf%%, total time: %dns", l1cache->GetMissRate(), tot_time);

    return 0;
}
