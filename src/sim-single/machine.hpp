#ifndef MACHINE_HEADER
#define MACHINE_HEADER

#include "memory.hpp"
#include "riscsim.hpp"
#include <map>
#include <queue>

#define SPReg               2
#define A0Reg               10
#define A1Reg               11
#define A7Reg               17
#define PCReg               32
#define E_ValEReg           33
#define E_ValCReg           34  
#define M_ValEReg           35 
#define M_ValCReg           36

#define RegNum              40
#define PageSize            4096
#define PhysicalPageNum     1000
#define PhysicalMemSize     PhysicalPageNum * PageSize

#define StackTopPtr         0x80000000
#define StackPageNum        5

class Machine 
{
public:
    Machine(bool singleStep);
    ~Machine();

    // register operation
    int64_t ReadReg(int rid)            { return reg[rid]; }
    void WriteReg(int rid, int64_t val) { reg[rid] = val; }
    void PrintReg(FILE *fout = NULL);	

    // memory operation
    bool AllocatePage(uint64_t vpn);
    bool ReadMem(uint64_t addr, int size, void *value);
    bool WriteMem(uint64_t addr, int size, uint64_t value);
    bool Translate(uint64_t v_addr, uint64_t *p_addr, int size);
    void PrintMem(FILE *fout = NULL);
    void PrintPageTable(FILE *fout = NULL);

    // simulator
    int64_t Forward(int rid);
    bool Fetch();
    bool Decode();
    bool Execute();
    bool MemoryAccess();
    bool WriteBack();

    void Run();
    void Status(FILE *fout = NULL);
    void SingleStepDebug();

    int64_t reg[RegNum];

    uint8_t *mem;
    std::map<uint64_t, PageTableEntry*> pageTable;
    PageTableEntry *pte;
    std::priority_queue<uint64_t> freePage;

    Instruction f_inst, d_inst, e_inst, m_inst, w_inst;
    Instruction test_inst;
    bool f_bubble, d_bubble, e_bubble, m_bubble, w_bubble;
    bool f_stall, d_stall, e_stall, m_stall, w_stall;

    bool singleStep;
    int instCount;
    double runTime;
};

#endif