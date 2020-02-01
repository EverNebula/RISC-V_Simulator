#include "machine.hpp"
#include "utils.hpp"

#include <elfio/elfio.hpp>

#include <string.h>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;

#include <boost/program_options.hpp>
using namespace boost::program_options;

Machine *machine;
bool singleStep = false;
string fileName, cfgName;
PRED_TYPE predType;
bool runTrace = false;
int cacheLevel = 3;

void ParseArg(int argc, char *argv[])
{
    options_description opts("RISC-V Simulator Options");

    opts.add_options()
        (",s", "single step")
        (",t", "running a trace")
        ("verbose,v", "print more info")
        ("debug,d", "print debug info")
        ("config,c", value<string>()->required(), "set config file (format as 'config/default.cfg')")
        ("filename,f", value<string>()->required(), "riscv elf file")
        ("level,l", value<int>()->required(), "cache level (can be 0-3)")
        ("pred,p", value<int>()->required(),
         "branch predict strategy (0-4)\n 0: always not taken\n 1: always taken\n 2: 1-bit predictor\n \
3: 2-bit predictor\n 4: 2-bit predictor alternative")
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

    if(vm.count("-t"))
    {
        runTrace = true;
    }

    if(vm.count("debug"))
    {
        debug = true;
    }

    if(vm.count("config"))
    {
        cfgName = vm["config"].as<string>();
    }
    else
    {
        cfgName = string("cfg/default.cfg");
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
        printf("please use -f to specify the risc-v file.\n");
        exit(0);
    }

    if (vm.count("level"))
    {
        cacheLevel = vm["level"].as<int>();
        if (cacheLevel < 0 || cacheLevel > 3)
        {
            printf("wrong cache level, must be an integer between 0 and 3.\n");
            exit(0);
        }
    }
    else
    {
        cacheLevel = 3;
    }

    if (vm.count("pred"))
    {
        int type = vm["pred"].as<int>();
        if (type >= 0 && type < PredTypeNum)
            predType = (PRED_TYPE)type;
        else
        {
            printf("predict strategy not found (0-4), use ALWAYS_NTAKEN\n");
            predType = ALWAYS_NTAKEN;
        }
    }
    else
        predType = ALWAYS_NTAKEN;

    printf("[verbose]  %s\n", verbose?"on":"off");
    printf("[debug]    %s\n", debug?"on":"off");
    printf("[single]   %s\n\n", singleStep?"on":"off");
}

void LoadELF()
{
    ELFIO::elfio elf;
    Timer timer;
    vprintf("***** loading elf file\n");
    elf.load(fileName.c_str());

    if (elf.get_machine() != EM_RISCV)
    {
        printf("not a risc-v prog.\n");
        return;
    }

    vprintf("loading data to memory...\n");
    ELFIO::Elf_Half n_seg = elf.segments.size();

    for (int i = 0; i < n_seg; ++i)
    {
        const ELFIO::segment *pseg = elf.segments[i];

        uint64_t adr = pseg->get_virtual_address();
        uint64_t m_end = adr + pseg->get_memory_size();
        uint64_t f_end = adr + pseg->get_file_size();
        // add page table entry
        for (uint64_t vpn = (adr/PageSize); vpn*PageSize < f_end; vpn++)
        {
            machine->AllocatePage(vpn);            
        }

        const uint8_t* data = (uint8_t*)pseg->get_data();
        for (uint64_t offset = 0; adr < m_end; adr++, offset++)
        {
            machine->WriteMem(adr, 1, data[offset], true);
        }

    }

    // pipeline - predict pc
    machine->WriteReg(P_PCReg, elf.get_entry());
    // machine->PrintReg();

    vprintf("initializing stack...\n");
    // stack initialization
    machine->WriteReg(SPReg, StackTopPtr - 8);
    {
        uint64_t vpn = (StackTopPtr / PageSize) - 1;
        for (int i = 0; i < StackPageNum; i++, vpn--)
        {
            machine->AllocatePage(vpn);
        }
        machine->WriteMem(StackTopPtr - 8, 8, 0xdeadbeefdeadbeefll, true);
    }
    vprintf("***** loading elf end [%.2lf]\n\n", timer.Finish());
}

void RunTrace()
{
    FILE *trace = fopen(fileName.c_str(), "r");
    if (trace == NULL)
    {
        printf("can not open file %s.\n", fileName.c_str());
        exit(0);
    }

    int hit, time;
    int64_t tot_time = 0;
    uint8_t content[64];
    content[0] = 1;
    
    char buf[100];
    int cnt = 0;
    while(fgets(buf, 100, trace))
    {
        char op[10];
        uint64_t addr;
        sscanf(buf, "%s %llx", op, &addr);
        // addr %= PhysicalMemSize;
        vprintf("%s 0x%llx\n", op, addr);
        switch (op[0])
        {
            case 'r':
                machine->topStorage->HandleRequest(addr, 1, 1, content, hit, time);
                break;
            case 'w':
                machine->topStorage->HandleRequest(addr, 1, 0, content, hit, time);
                break;
            default:
                printf("unknown command.\n");
                exit(0);
        }
        vprintf("%s\n", hit?"hit":"miss");
        tot_time += time;
        cnt++;
    }

    // printf("%lld ", tot_time);

    // double l1m = machine->l1cache->MissRate();
    // double l2m = machine->l2cache->MissRate();
    // printf("L1 Cache: %lf\n", l1m);
    // printf("L2 Cache: %lf\n", l2m);
    // printf("AMAT: %lf\n", (1-l1m) + l1m*(1-l2m)*8 + l1m*l2m*100);
    if(cacheLevel > 0)
    {
        printf("L1 Cache:\n");
        machine->l1cache->Print();
    }
    if(cacheLevel > 1)
    {
        printf("L2 Cache:\n");
        machine->l2cache->Print();
    }
    if(cacheLevel > 2)
    {
        printf("L3 Cache:\n");
        machine->l3cache->Print();
    }
    fclose(trace);
}

void Test()
{
    Memory *mainMem;
    Cache *l1cache, *l2cache, *l3cache;

    mainMem = new Memory();
    l1cache = new Cache("L1 cache");
    l2cache = new Cache("L2 cache");
    l3cache = new Cache("L3 cache");

    l1cache->SetLower(l2cache);
    l2cache->SetLower(l3cache);
    l3cache->SetLower(mainMem);

    StorageStats s;
    s.access_time = 0;
    mainMem->SetStats(s);
    l1cache->SetStats(s);
    l2cache->SetStats(s);
    l3cache->SetStats(s);

    StorageLatency ml;
    ml.bus_latency = 0;
    ml.hit_latency = 100;
    mainMem->SetLatency(ml);

    StorageLatency ll;
    ll.bus_latency = 0;
    ll.hit_latency = 1;
    l1cache->SetLatency(ll);

    ll.bus_latency = 0;
    ll.hit_latency = 8;
    l2cache->SetLatency(ll);

    ll.bus_latency = 0;
    ll.hit_latency = 20;
    l3cache->SetLatency(ll);

    CacheConfig l1c;
    l1c.size = 1024;
    l1c.assoc = 1;
    l1c.line_size = 16; // Size of cache line
    l1c.write_through = 1; // 0|1 for back|through
    l1c.write_allocate = 0; // 0|1 for no-alc|alc
    l1cache->SetConfig(l1c);
    l1cache->Allocate();

    l1c.size = 4 * 1024;
    l1c.assoc = 1;
    l1c.line_size = 16; // Size of cache line
    l1c.write_through = 1; // 0|1 for back|through
    l1c.write_allocate = 1; // 0|1 for no-alc|alc
    l2cache->SetConfig(l1c);
    l2cache->Allocate();

    l1c.size = 16 * 1024;
    l1c.assoc = 1;
    l1c.line_size = 16; // Size of cache line
    l1c.write_through = 0; // 0|1 for back|through
    l1c.write_allocate = 1; // 0|1 for no-alc|alc
    l3cache->SetConfig(l1c);
    l3cache->Allocate();

    int hit, time, tot_time = 0;
    uint8_t content[64];
    content[0] = 0x01;
    l1cache->HandleRequest(0x4e1f0b0, 1, 0, content, hit, time);
    content[0] = 0x23;
    l1cache->HandleRequest(0x4e1f0b1, 1, 0, content, hit, time);
    content[0] = 0x45;
    l1cache->HandleRequest(0x4e1f0b2, 1, 0, content, hit, time);
    content[0] = 0x67;
    l1cache->HandleRequest(0x4e1f0b3, 1, 0, content, hit, time);
    content[0] = 0x67;
    l1cache->HandleRequest(0x3e1f0b0, 1, 0, content, hit, time);
    l1cache->HandleRequest(0x4e1f0b0, 4, 1, content, hit, time);

    for(int i = 0; i < 4; ++i)
    {
        printf("%llx ", content[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    // parse args
    ParseArg(argc, argv);

    // build machine
    machine = new Machine(predType);
    machine->singleStep = singleStep;
    machine->cfg.LoadConfig(cfgName.c_str());
    machine->StorageInit(cacheLevel);
    // for (int i = 0; i <= 10; i += 2)
    // {  
    //     for (int j = 0; j < 10; j++)
    //         {
    //             machine = new Machine(predType);
    //             machine->singleStep = singleStep;
    //             machine->cfg.LoadConfig(cfgName.c_str());
    //             machine->cfg.u32_cfg[L1C_SIZE] = 1 << (15 + i);
    //             machine->cfg.u32_cfg[L1C_BSIZE] = 8 << j;
    //             machine->StorageInit(cacheLevel);

    //             RunTrace();

    //             delete machine;
    //         }
    //     printf("\n");
    // }
    // for (int i = 0; i <= 10; i += 2)
    // {  
    //     for (int j = 0; j < 2; j++)
    //         for (int k = 1; k >= 0; k--)
    //         {
    //             machine = new Machine(predType);
    //             machine->singleStep = singleStep;
    //             machine->cfg.LoadConfig(cfgName.c_str());
    //             machine->cfg.u32_cfg[L1C_SIZE] = 1 << (15 + i);
    //             machine->cfg.u32_cfg[L1C_WT] = j;
    //             machine->cfg.u32_cfg[L1C_WA] = k;
    //             machine->StorageInit(cacheLevel);

    //             RunTrace();

    //             delete machine;
    //         }
    //     printf("\n");
    // }
    // return 0;

    // Cache Test
    if (runTrace)
    {
        RunTrace();
        return 0;
    }

    LoadELF();

    // machine run
    machine->Run();

    return 0;
}
