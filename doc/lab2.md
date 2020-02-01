# Lab2 RISC-V Simulator

## 概述
RISC-V Simulator实现了对于RISC-V架构的用户程序的简单功能模拟与性能模拟，并实现了单周期与五级流水线两种形式的模拟器。对于流水线模拟器，实现了多种分支跳转预测以及处理器运行周期的配置，并给之后模拟器的扩展提供了接口。

## 编译运行
模拟器的开发基于`RISC-V toolchain`，同时在代码中使用了`boost`库。进入项目文件夹后可以使用`make`来构建相应的目标：

- `pipeline` : 流水线模拟器
- `single` : 单周期模拟器
- `libecall` : 模拟器专用系统调用库（测试程序依赖）
- `testprog` : 所有的测试程序
- `all` : 将会自动编译流水线模拟器以及测试程序

之后运行`./sim`运行模拟器，参数如下：

- `-f <filename>` : __必要参数__，指定RISC-V用户程序路径
- `-s` : 单步调试
- `-v` : 输出指令相关信息
- `-d` : 输出全部debug信息
- `-p <type>` : 分支预测策略，目前支持五种，`type`为`0-4`，具体在后文解释，也可使用`-h`查看
- `-c <filename>` : 指令执行周期配置文件，格式参照`cfg/default.cfg`
- `-h` : 帮助信息

单步调试指令：

- `c` : 执行下一个周期/指令
- `r` : 输出所有寄存器
- `d` : 输出当前模拟器的所有信息到文件`status_dump.txt`
- `p` : 输出当前页表信息
- `m <address/hex> <size/dec>`: 查看`address`处`size`大小的数据，需要数据对齐
- `q` : 退出模拟器

## 结构介绍
### main
`main`是整个模拟器的入口，它会负责解析输入的命令行参数，初始化整个机器`Machine`类，加载相应的可执行文件至机器物理内存，同时开辟所需的栈空间。在处理完所有的初始化步骤后，将会调用`Machine`类的成员函数`Run`开始模拟。

### Machine
`Machine`类是整个模拟器中最重要的类，包含的内容大体可以分为机器运行(`machine.cpp`)、寄存器操作(`machine.cpp`)、内存操作(`memory.cpp`)、指令模拟(`riscsim.cpp`)、分支预测(`predictor.cpp`)、外部配置(`config.cpp`)几个模块。

```
class Machine 
{
public:
    Machine(PRED_TYPE pred_mode);
    ~Machine();

    // register operation
    int64_t ReadReg(int rid);
    void WriteReg(int rid, int64_t val);
    void PrintReg(FILE *fout = NULL);	

    // memory operation
    bool AllocatePage(uint64_t vpn);
    bool ReadMem(uint64_t addr, int size, void *value);
    bool WriteMem(uint64_t addr, int size, uint64_t value);
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
    void Run();
    void Status(FILE *fout = NULL);
    void SingleStepDebug();

    int64_t reg[RegNum];

    uint8_t *mem;
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

class PipelineRegister
{
public:
    Instruction inst;
    bool bubble, stall, pred_j;
    int64_t val_e, val_c;

    PipelineRegister()
    {
        bubble = true;
        stall  = false;
    }
};
```

`machine.cpp`中主要包含以下的函数：

- `void Run()` : 启动整个机器，开始模拟
- `void SingleStepDebug()` : 进行单步调试的命令解析
- `int64_t ReadReg() / void WriteReg()` : 寄存器的读取与写入
- `void Status()` : 输出整个机器的所有信息
- `void PrintReg()` : 输出所有寄存器信息

### Memory
内存部分包含虚拟地址到物理地址的翻译以及页表的处理。

__[页表]__
```
class PageTableEntry
{
public:
	uint64_t ppn;
	uint64_t vpn;

	bool valid;
};
```

目前页表项`PageTableEntry(memory.hpp)`只包含了虚拟页号、物理页号以及有效位三个信息。以后如果实现了Cache、虚拟内存可以增加脏位等信息。

而页表这里简单地用`Map`实现虚拟页到物理页的翻译，用`Set`保存所有空闲的物理页。

- `bool AllocatePage()` : 对一个虚拟页分配物理页，页分配必须由模拟器进行，用户程序不能自行增加使用页，如需要也必须通过系统调用由模拟器实现
- `bool Translate()` : 给定虚拟地址，翻译成物理地址
- `bool ReadMem()` : 从指定虚拟地址读取数据
- `bool WriteMem()` : 向指定虚拟地址写数据
- `void PrintPageTable()` : 打印页表信息
- `void PrintMem()` : 打印内存数据信息

### Riscsim
指令模拟包含了流水线五个阶段处理以及流水线相关的函数。五个阶段分别为：

- 取指 : 根据预测PC从内存/I-Cache中取出指令
- 译码 : 将32位指令进行解析，确定是哪一条指令并取出相应参数，对于立即数做64位的符号拓展，将寄存器的值取出（或Forwarding）
- 执行 : 执行指令的计算部分，如果是分支指令则判断预测正误并更新预测PC
- 访存 : 对于需要访存的s与l指令进行访存操作
- 回写 : 需要回写寄存器的指令进行回写操作

对于后面指令需要用到前面寄存器值的情况下，设置了Forwarding的旁路从W/M/E三个阶段向D阶段回传。

在这个流水线结构下，主要划分了以下几个Stall的情况：

- `Load-Use Hazard` : load指令后马上使用该寄存器，将F/D Stall一个周期，E阶段插Bubble
- `Control Hazard` : 分支跳转预测错误，之前的两个指令无效，D/E设Bubble，马上更新正确的预测PC
- `JALR Stall` : JALR指令需要根据寄存器的值跳转，但是F阶段无法获得寄存器的值，因此需要等到其到D阶段结束获得寄存器值时才能继续流水线
- `ECALL Stall` : 由于模拟器的特殊性，并没有一个完整的操作系统，而ECALL本应陷入内核执行一系列内核指令，所以这里简单地将流水线排空，然后再执行ECALL指令，来近似陷入内核的现象

需要注意的是当F阶段Stall时，预测PC除非是分支预测错误的强制更新，否则 __不能更新__。

相关函数:

- `int Fetch()` : 取指，返回所需的CPU Cycle(下同)
- `int Decode()` : 译码并得到寄存器的正确值
- `int Execute()` : 执行指令的计算部分
- `int MemoryAccess()` : 访存，限s和l指令
- `int WriteBack()` : 回写，特别的，对于ECALL抛出异常并执行
- `void UpdatePipeline()` : 将下一时刻流水线寄存器的值进行更新，Stall则保持不变
- `void PrintInst()` : 输出指令

### Predictor
分支预测包含了五种对于b系列分支跳转指令的预测方案，分别为：

- 0: Always not taken，总不跳转
- 1: Always taken，总跳转
- 2: 1-bit predictor，一位预测器，始终采取上一次的跳转策略
- 3: 2-bit predictor，两位预测器，跳转与不跳转均分为WEAK与STRONG两种状态
- 4: 2-bit predictor alternative，两位预测器(2)，当WEAK状态预测错误时进入反对的STRONG状态

由于无法对所有地址都保存一个预测状态，采取组相连Cache的方式来实现。

```
class Predictor
{
public:
	PRED_TYPE mode;
	uint64_t *pred_state;

	Predictor(PRED_TYPE mode);
	~Predictor();

	void Init();
	bool Predict(uint64_t adr);
	void Update(uint64_t adr, bool real);
	char* Name(){ return pred_str[mode]; }
};
```

在模拟器运行时可以用`-p <0-4>`来指定使用的分支预测策略。

相关函数:

- `void Init()` : 初始化预测器
- `bool Predict()` : 给定地址，预测是否跳转
- `void Update()` : 给定实际跳转情况，按状态机更新状态

### Config
外部配置包含了对模拟器可变参数的配置，目前仅包含不同指令处理器运行周期的配置。配置文件的格式为`str : value`的字符串对，支持使用`//`进行注释，模板见`cfg/default.cfg`。

```
class Config
{
public:
	unsigned u32_cfg[ConfigU32Num];

	Config();
	~Config();
	void LoadConfig(const char *file);
	void Print(FILE *fout = NULL);
};
```

在模拟器运行时可以用`-c <filename>`来指定使用的配置文件，默认为`cfg/default.cfg`。

目前包含的配置项目为：

- `SFT_CYC` : 整数，移位指令周期数
- `ADD32_CYC` : 整数，32位加法指令周期数
- `ADD64_CYC` : 整数，64位加法指令周期数
- `MUL32_CYC` : 整数，32位乘法指令周期数
- `MUL64_CYC` : 整数，64位乘法指令周期数
- `DIV32_CYC` : 整数，32位除法指令周期数
- `DIV64_CYC` : 整数，64位乘法指令周期数
- `CACHE_CYC` : 整数，访问Cache周期数
- `MEM_CYC` : 整数，访问内存周期数

处理Config相关函数：

- `void LoadConfig()` : 从文件读取配置
- `void Print()` : 打印当前配置信息

### Syscall
由于Linux的Syscall过多，模拟器定义了一套自己的Syscall。要想使用Syscall，必须在用户程序中`#include <syscall.h>`，并在链接时加上flag `-lecall`。想编译`libecall`库可以使用`make libecall`指令。

模拟器自定义Syscall支持了简单的读写以及exit，使用C内嵌汇编写成，具体见`syscall.c`。

### Testprog
除了给定的五个测试程序外，新增了lab1中的`ackermann`以及`matmul`。

编译测试程序的方法是运行`make testprog`指令。

## 测试程序评测
注：为了确认程序的运行正确性，在给定的测序程序后都增加了将结果输出的相关代码，因此可能比原运行周期/指令数多。

### 执行指令数与周期数
`Pipeline CpI`指流水线的周期/指令数，而`CPU CpI`指的是CPU周期/指令数。不同指令在执行阶段、访存阶段等所消耗的CPU周期可能不相同（通过config改变），但流水线都只计为1周期。

采用的分支跳转策略都是 __2-bit__ __predictor__。

对于CPU周期消耗设定如下：
```
---------------- Config ----------------
U32 CONFIG:
- SFT_CYC: 	        1
- ADD32_CYC: 	    1
- ADD64_CYC: 	    2
- MUL32_CYC: 	    3
- MUL64_CYC: 	    6
- DIV32_CYC: 	    5
- DIV64_CYC: 	    10
- CACHE_CYC: 	    3
- MEM_CYC: 	        20
----------------------------------------
```

__1)n!__:（更名为`nl`）
```
BASIC STATUS:
- Cycle Count:       1396
- Inst. Count:       1156
- Run Time:          0.0005
- Pipeline Cyc CpI:  1.21
- CPU Cyc CpI:       4.72
```

__2)add__:
```
BASIC STATUS:
- Cycle Count:       1643
- Inst. Count:       1322
- Run Time:          0.0005
- Pipeline Cyc CpI:  1.24
- CPU Cyc CpI:       5.93
```

__3)simple-fuction__:
```
BASIC STATUS:
- Cycle Count:       1650
- Inst. Count:       1329
- Run Time:          0.0007
- Pipeline Cyc CpI:  1.24
- CPU Cyc CpI:       5.94
```

__4)qsort__:
```
BASIC STATUS:
- Cycle Count:       27808
- Inst. Count:       22686
- Run Time:          0.0099
- Pipeline Cyc CpI:  1.23
- CPU Cyc CpI:       8.78
```

__5)mul-div__:
```
BASIC STATUS:
- Cycle Count:       1668
- Inst. Count:       1347
- Run Time:          0.0006
- Pipeline Cyc CpI:  1.24
- CPU Cyc CpI:       5.85
```
这里会发现`mul-div`反而比`add`的CPU CpI更低。经过指令输出发现，`mul-div`中并没有`mul`与`div`指令，这是因为编译器对其进行了优化。比如除2在实际运算中变成了移位指令，在上面的配置文件中移位指令是比`add64`运算还快的，这就导致了这个现象。但是可以发现，两者由于程序体相近，因此Pipline CpI一样，这是符合预期的。

### 停顿统计
分支预测策略的改变将会显著地影响到`Control Hazard`的数目，因此这里也是固定策略为 __2-bit__ __predictor__。

__1)n!__:
```
- Pred Strategy:     2-bit predictor
- Branch Pred Acc:   76.55%	(173 / 226)
- Load-use Hazard:   35
- Ctrl. Hazard:      53 * 2	(cycles)
- ECALL Stall:       3 * 3	(cycles)
- JALR Stall:        45 * 2	(cycles)
```

__2)add__:
```
- Pred Strategy:     2-bit predictor
- Branch Pred Acc:   88.52%	(162 / 183)
- Load-use Hazard:   99
- Ctrl. Hazard:      21 * 2	(cycles)
- ECALL Stall:       22 * 3	(cycles)
- JALR Stall:        57 * 2	(cycles)
```

__3)simple-fuction__:
```
- Pred Strategy:     2-bit predictor
- Branch Pred Acc:   88.52%	(162 / 183)
- Load-use Hazard:   99
- Ctrl. Hazard:      21 * 2	(cycles)
- ECALL Stall:       22 * 3	(cycles)
- JALR Stall:        57 * 2	(cycles)
```
2)与3)的结果是比较好比较的，它们的区别仅仅是是否将计算过程封装为函数，函数用JALR进行跳转，所以理应多几个JALR Stall，但是两者却相同。经过反汇编后发现两者调用syscall时有时使用JAL有时则是JALR，因此我将两者的输出过程去掉（即syscall部分），再进行比较。

此时，对于`add`，`JALR Stall`为25个，而`simple-fuction`为26个，这是个相对正常的结果。

__4)qsort__:
```
- Pred Strategy:     2-bit predictor
- Branch Pred Acc:   83.33%	(2099 / 2519)
- Load-use Hazard:   3335
- Ctrl. Hazard:      420 * 2	(cycles)
- ECALL Stall:       85 * 3	(cycles)
- JALR Stall:        346 * 2	(cycles)
```
`qsort`的指令数较多，因此停顿也较多，不过跟上面对比可以发现，`Control Hazard`的比例明显升高，这是因为排序中经常要进行比较。取数操作较多，因此`Load-Use Hazard` 也较多。

__5)mul-div__:
```
- Pred Strategy:     2-bit predictor
- Branch Pred Acc:   88.52%	(162 / 183)
- Load-use Hazard:   99
- Ctrl. Hazard:      21 * 2	(cycles)
- ECALL Stall:       22 * 3	(cycles)
- JALR Stall:        57 * 2	(cycles)
```
此程序与`add`相近，因此参数也一模一样。

### 预测策略统计
对于五种预测策略，下面分别给出预测准确率：

program|ALWAYS_NOT_TAKEN|ALWAYS_TAKEN|1BIT_PRED|2BIT_PRED|2BIT_PRED_ALT
-|-|-|-|-|-
n!|0.4602|0.5398|__0.7655__|__0.7655__|0.7566
add|0.4426|0.5574|__0.8852__|__0.8852__|__0.8852__
simple-fuction|0.4426|0.5574|__0.8852__|__0.8852__|__0.8852__
qsort|0.4954|0.5046|0.8333|0.8333|__0.8646__
mul-div|0.4426|0.5574|__0.8852__|__0.8852__|__0.8852__
ackermann(3,5)|0.6664|0.3336|__0.9922__|__0.9922__|0.9885
add|0.3419|__0.6581__|0.5913|0.5913|0.5837

可以发现`add`、`simple-fuction`、`mul-div`基本逻辑差不多，因此分支预测完全一样。但是这七个程序都不存在一条语句中的跳转与否变化比较激烈的，因此`1-bit predictor`与`2-bit predictor`基本一样。

各个策略基本都有适用的程序，但是总得来说`1-bit/2-bit predictor`平均表现较好。