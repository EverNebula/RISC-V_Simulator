#include "machine.hpp"
#include "utils.hpp"

#include <elfio/elfio.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;

#include <boost/program_options.hpp>
using namespace boost::program_options;

Machine *machine;
bool singleStep = false;
string fileName;

void ParseArg(int argc, char *argv[])
{
    options_description opts("RISC-V Simulator Options");

    opts.add_options()
        (",s", "single step")
        ("verbose,v", "print more info")
        ("debug,d", "print debug info")
        ("filename,f", value<string>()->required(), "riscv elf file")
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
        printf("please use -f to specify the risc-v file.\n");
        exit(0);
    }

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
            machine->WriteMem(adr, 1, data[offset]);
        }

    }

    machine->WriteReg(PCReg, elf.get_entry());
    // machine->PrintReg();

    vprintf("initializing stack...\n");
    // stack initialization
    machine->WriteReg(SPReg, StackTopPtr);
    {
        uint64_t vpn = (StackTopPtr / PageSize);
        for (int i = 0; i < StackPageNum; i++, vpn--)
        {
            machine->AllocatePage(vpn);
        }
        machine->WriteMem(StackTopPtr, 4, 0xdeadbeef);
    }
    vprintf("***** loading elf end [%.2lf]\n\n", timer.Finish());
}

int main(int argc, char *argv[])
{
    // parse args
    ParseArg(argc, argv);

    // build machine
    machine = new Machine(singleStep);
    LoadELF();

    // machine run
    machine->Run();

    // 

    return 0;
}
