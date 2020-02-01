#ifndef RISCSIM_HEADER
#define RISCSIM_HEADER

#define OpNum		54
#define MaxStrLen	1024	

enum InstType 
{
    Rtype, Itype, Stype, SBtype, Utype, UJtype
};

enum OpType
{
	Op_add, Op_mul, Op_sub, Op_sll, Op_mulh,
	Op_slt, Op_xor, Op_div, Op_srl, Op_sra,
	Op_or, Op_rem, Op_and, Op_lb, Op_lh,
	Op_lw, Op_ld, Op_addi, Op_slli, Op_slti,
	Op_xori, Op_srli, Op_srai, Op_ori, Op_andi,
	Op_addiw, Op_jalr, Op_ecall, Op_sb, Op_sh,
	Op_sw, Op_sd, Op_beq, Op_bne, Op_blt,
	Op_bge, Op_auipc, Op_lui, Op_jal, Op_bltu,
	Op_bgeu, Op_lbu, Op_lhu, Op_lwu, Op_sltiu,
	Op_sltu, Op_slliw, Op_srliw, Op_sraiw, Op_addw,
	Op_subw, Op_sllw, Op_srlw, Op_sraw
};

extern const char *op_str[60];
extern const char *reg_str[33];

class Instruction
{
  public:
  	uint64_t adr;
    uint32_t value;

    uint32_t opcode;
    uint32_t func3, func7;
    uint32_t rs1, rs2, rd;
    int64_t imm;

    InstType type;
    OpType optype;

    void PrintInst();
};

#endif