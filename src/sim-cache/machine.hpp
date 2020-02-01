#ifndef MACHINE_HEADER
#define MACHINE_HEADER

#include "memory.hpp"
#include "cache.hpp"
#include "riscsim.hpp"
#include "predictor.hpp"
#include "config.hpp"
#include <map>
#include <queue>
#include <string>

#define ZeroReg             0
#define SPReg               2
#define A0Reg               10
#define A1Reg               11
#define A7Reg               17
#define PCReg               32
#define P_PCReg             33  // predict pc

#define RegNum              36
#define PageSize            4096
#define PhysicalPageNum     20000
#define PhysicalMemSize     PhysicalPageNum * PageSize

#define StackTopPtr         0x80000000
#define StackPageNum        4

class PipelineRegister
{
public:
    Instruction inst;
    bool bubble, stall, pred_j;
    int64_t val_e, val_c; // in decode stage - val_e=reg_a  val_c=reg_b

    PipelineRegister()
    {
        bubble = true;
        stall  = false;
    }
};

class Machine 
{
public:
    Machine(PRED_TYPE pred_mode);
    ~Machine();

    // register operation
    int64_t ReadReg(int rid)            { return reg[rid]; }
    void WriteReg(int rid, int64_t val) { reg[rid] = val; }
    void PrintReg(FILE *fout = NULL);	

    // memory operation
    bool AllocatePage(uint64_t vpn);
    int ReadMem(uint64_t addr, int size, void *value);
    int WriteMem(uint64_t addr, int size, uint64_t value, bool MemDirect = false);
    bool Translate(uint64_t v_addr, uint64_t *p_addr, int size);
    void PrintMem(FILE *fout = NULL, bool no_data = false);
    void PrintPageTable(FILE *fout = NULL);

    // simulator
    int Fetch();
    int Decode();
    int Execute();
    int MemoryAccess();
    int WriteBack();
    void UpdatePipeline();

    // machine
    void StorageInit(int cacheLevel);
    void Run();
    void Status(FILE *fout = NULL);
    void SingleStepDebug();

    int64_t reg[RegNum];

    Memory *mainMem;
    Cache *l1cache, *l2cache, *l3cache;
    Storage *topStorage;

    std::map<uint64_t, PageTableEntry*> pageTable;
    PageTableEntry *pte;
    std::priority_queue<uint64_t> freePage;
    PipelineRegister F_reg, D_reg, E_reg, M_reg, W_reg;
    PipelineRegister f_reg, d_reg, e_reg, m_reg;

    Predictor *predictor;

    Config cfg;

    bool singleStep;
    int cycCount;
    int cpuCount;
    int instCount;
    double runTime;

    int loadHzdCount;
    int ctrlHzdCount;
    int totalBranch;
    int ecallStlCount;
    int jalrStlCount;
};

#endif
