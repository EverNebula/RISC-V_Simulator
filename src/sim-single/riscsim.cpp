#include "machine.hpp"
#include "utils.hpp"
#include <stdio.h>

const char *op_str[60] =
{
	"add", "mul", "sub", "sll", "mulh",
	"slt", "xor", "div", "srl", "sra",
	"or", "rem", "and", "lb", "lh",
	"lw", "ld", "addi", "slli", "slti",
	"xori", "srli", "srai", "ori", "andi",
	"addiw", "jalr", "ecall", "sb", "sh",
	"sw", "sd", "beq", "bne", "blt",
	"bge", "auipc", "lui", "jal", "bltu",
	"bgeu", "lbu", "lhu", "ldu", "sltiu",
	"sltu", "slliw", "srliw", "sraiw", "addw",
	"subw", "sllw", "srlw", "sraw"
};

const char *reg_str[33] = {
    "zero", "ra", "sp", "gp", "tp",   
    "t0", "t1", "t2", "s0", "s1",   
    "a0", "a1", "a2", "a3", "a4",  
    "a5", "a6", "a7", "s2", "s3",  
    "s4", "s5", "s6", "s7", "s8",  
    "s9", "s10", "s11", "t3", "t4",  
    "t5", "t6", "pc"    
};

inline uint32_t
maskvalue(int len, uint32_t &value)
{
	uint32_t res = value & ((1 << len) - 1);
	value >>= len;
	return res;
}

inline int64_t
signext64(int len_src, uint64_t value)
{
	uint64_t sign = (1ll << (len_src - 1));
	if (sign & value) // neg
		return value | ((-1ll) ^ ((1ll << len_src) - 1));
	else
		return value & ((1ll << len_src) - 1);
}

bool
Machine::Fetch()
{
	dprintf("\n**** Fetch Stage [%d] ****\n", instCount);

	Instruction *inst = &test_inst;	
	uint64_t inst_adr = ReadReg(PCReg);

	dprintf("fetching operation from 0x%016llx.\n", inst_adr);
	inst->adr = inst_adr;
	if (!ReadMem(inst_adr, 4, (void*)&(inst->value)))
	{
		vprintf("--Can not fetch operation. [Fetch]\n");
		return false;
	}

	dprintf("op_value: 0x%08x\n", inst->value);
	return true;
}

bool
Machine::Decode()
{
	dprintf("\n**** Decode Stage [%d] ****\n", instCount);

	Instruction *inst = &test_inst;	
	uint32_t value = inst->value;
	uint32_t opcode = maskvalue(7, value);
	inst->opcode = opcode;

	// instruction type
	switch (opcode)
	{
		case 0x33:
		case 0x3b:
			inst->type = Rtype;
			inst->rd = maskvalue(5, value);
			inst->func3 = maskvalue(3, value);
			inst->rs1 = maskvalue(5, value);
			inst->rs2 = maskvalue(5, value);
			inst->func7 = maskvalue(7, value);
			break;
		case 0x03:
		case 0x13:
		case 0x1b:
		case 0x67:
		case 0x73:
			inst->type = Itype;
			inst->rd = maskvalue(5, value);
			inst->func3 = maskvalue(3, value);
			inst->rs1 = maskvalue(5, value);
			inst->imm = maskvalue(12, value);
			inst->func7 = (((inst->value) >> 26) & 0x3f);
			inst->imm = signext64(12, inst->imm);
			break;
		case 0x23:
			inst->type = Stype;
			inst->imm = 0;
			inst->imm |= maskvalue(5, value);
			inst->func3 = maskvalue(3, value);
			inst->rs1 = maskvalue(5, value);
			inst->rs2 = maskvalue(5, value);
			inst->imm |= (maskvalue(7, value) << 5);
			inst->imm = signext64(12, inst->imm);
			break;
		case 0x63:
			inst->type = SBtype;
			inst->imm = 0;
			inst->imm |= (maskvalue(1, value) << 11);
			inst->imm |= (maskvalue(4, value) << 1);
			inst->func3 = maskvalue(3, value);
			inst->rs1 = maskvalue(5, value);
			inst->rs2 = maskvalue(5, value);
			inst->imm |= (maskvalue(6, value) << 5);
			inst->imm |= (maskvalue(1, value) << 12);
			inst->imm = signext64(13, inst->imm);
			break;
		case 0x17:
		case 0x37:
			inst->type = Utype;
			inst->rd = maskvalue(5, value);
			inst->imm = (maskvalue(20, value) << 12);
			inst->imm = signext64(32, inst->imm);
			break;
		case 0x6f:
			inst->type = UJtype;
			inst->rd = maskvalue(5, value);
			inst->imm = 0;
			inst->imm |= (maskvalue(8, value) << 12);
			inst->imm |= (maskvalue(1, value) << 11);
			inst->imm |= (maskvalue(10, value) << 1);
			inst->imm |= (maskvalue(1, value) << 20);
			inst->imm = signext64(21, inst->imm);
			break;
		default:
			vprintf("[Error] Unknown opcode 0x%02x. [Decode]\n", opcode);
			return false;
	}

	// optype 
	switch (inst->type)
	{
		case Rtype:
			switch (inst->opcode)
			{
				case 0x33:
					switch (inst->func3)
					{
						case 0x00:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_add;
									break;
								case 0x01:
									inst->optype = Op_mul;
									break;
								case 0x20:
									inst->optype = Op_sub;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;
						case 0x01:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_sll;
									break;
								case 0x01:
									inst->optype = Op_mulh;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;
						case 0x02:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_slt;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;	
						case 0x03:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_sltu;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;	
						case 0x04:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_xor;
									break;
								case 0x01:
									inst->optype = Op_div;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;	
						case 0x05:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_srl;
									break;
								case 0x20:
									inst->optype = Op_sra;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;
						case 0x06:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_or;
									break;
								case 0x01:
									inst->optype = Op_rem;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;	
						case 0x07:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_and;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;	
						default:
							goto R_UNKNOWN;		
					}
					break;
				case 0x3b:
					switch (inst->func3)
					{
						case 0x00:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_addw;
									break;
								case 0x20:
									inst->optype = Op_subw;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;
						case 0x01:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_sllw;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;
						case 0x05:
							switch (inst->func7)
							{
								case 0x00:
									inst->optype = Op_srlw;
									break;
								case 0x20:
									inst->optype = Op_sraw;
									break;
								default:
									goto R_UNKNOWN;	
							}
							break;
					}
					break;
				default:
					R_UNKNOWN:
					vprintf("[Error] Unknown Rtype inst(opcode=0x%02x, func3=0x%02x, func7=0x%02x). [Decode]\n",
							 inst->opcode, inst->func3, inst->func7);
					return false;
			}
			break;
		case Itype:
			switch (inst->opcode)
			{
				case 0x03:
					switch (inst->func3)
					{
						case 0x00:
							inst->optype = Op_lb;
							break;
						case 0x01:
							inst->optype = Op_lh;
							break;
						case 0x02:
							inst->optype = Op_lw;
							break;
						case 0x03:
							inst->optype = Op_ld;
							break;
						case 0x04:
							inst->optype = Op_lbu;
							break;
						case 0x05:
							inst->optype = Op_lhu;
							break;
						case 0x06:
							inst->optype = Op_lwu;
							break;
						default:
							goto I_UNKNOWN;	
					}
					break;
				case 0x13:
					switch (inst->func3)
					{
						case 0x00:
							inst->optype = Op_addi;
							break;
						case 0x01:
							if (inst->func7 == 0x00)
								inst->optype = Op_slli;
							else
								goto I_UNKNOWN;
							break;
						case 0x02:
							inst->optype = Op_slti;
							break;
						case 0x03:
							inst->optype = Op_sltiu;
							break;
						case 0x04:
							inst->optype = Op_xori;
							break;
						case 0x05:
							if (inst->func7 == 0x00)
								inst->optype = Op_srli;
							else if (inst->func7 == 0x10)
								inst->optype = Op_srai;
							else
								goto I_UNKNOWN;
							break;
						case 0x06:
							inst->optype = Op_ori;
							break;
						case 0x07:
							inst->optype = Op_andi;
							break;
						default:
							goto I_UNKNOWN;	
					}
					break;
				case 0x1b:
					switch (inst->func3)
					{
						case 0x00:
							inst->optype = Op_addiw;
							break;
						case 0x01:
							if (inst->func7 == 0x00)
								inst->optype = Op_slliw;
							else
								goto I_UNKNOWN;
							break;
						case 0x05:
							if (inst->func7 == 0x00)
								inst->optype = Op_srliw;
							else if (inst->func7 == 0x10)
								inst->optype = Op_sraiw;
							else
								goto I_UNKNOWN;
							break;
						default:
							goto I_UNKNOWN;	
					}
					break;
				case 0x67:
					switch (inst->func3)
					{
						case 0x00:
							inst->optype = Op_jalr;
							break;
						default:
							goto I_UNKNOWN;	
					}
					break;
				case 0x73:
					switch (inst->func3)
					{
						case 0x00:
							if (inst->func7 == 0x00)
								inst->optype = Op_ecall;
							else
								goto I_UNKNOWN;
							break;
						default:
							goto I_UNKNOWN;	
					}
					break;
				default:
					I_UNKNOWN:
					vprintf("[Error] Unknown Itype inst(opcode=0x%02x, func3=0x%02x, func7=0x%02x). [Decode]\n",
							 inst->opcode, inst->func3, inst->func7);
					return false;	
			}
			break;
		case Stype:
			switch (inst->func3)
			{
				case 0x00:
					inst->optype = Op_sb;
					break;
				case 0x01:
					inst->optype = Op_sh;
					break;
				case 0x02:
					inst->optype = Op_sw;
					break;
				case 0x03:
					inst->optype = Op_sd;
					break;
				default:
					vprintf("[Error] Unknown func3 0x%02x for Stype inst. [Decode]\n",
							 inst->func3);
					return false;
			}
			break;
		case SBtype:
			switch (inst->func3)
			{
				case 0x00:
					inst->optype = Op_beq;
					break;
				case 0x01:
					inst->optype = Op_bne;
					break;
				case 0x04:
					inst->optype = Op_blt;
					break;
				case 0x05:
					inst->optype = Op_bge;
					break;
				case 0x06:
					inst->optype = Op_bltu;
					break;
				case 0x07:
					inst->optype = Op_bgeu;
					break;
				default:
					vprintf("[Error] Unknown func3 0x%02x for SBtype inst. [Decode]\n",
							 inst->func3);
					return false;
			}
			break;
		case Utype:
			switch (inst->opcode)
			{
				case 0x17:
					inst->optype = Op_auipc;
					break;
				case 0x37:
					inst->optype = Op_lui;
					break;
				default:
					vprintf("[Error] Unknown opcode 0x%02x for Utype inst. [Decode]\n",
							 inst->opcode);
					return false;
			}
			break;
		case UJtype:
			switch (inst->opcode)
			{
				case 0x6f:
					inst->optype = Op_jal;
					break;
				default:
					vprintf("[Error] Unknown opcode 0x%02x for UJtype inst. [Decode]\n",
							 inst->opcode);
					return false;	
			}
			break;
		default:
			ASSERT(false);
			// never reach
	}

	if (debug || verbose)
		inst->PrintInst();
	return true;
}

int64_t
Machine::Forward(int rid)
{
	int64_t res;
	res = ReadReg(rid);
	return res;
}

bool
Machine::Execute()
{
	dprintf("\n**** Execute Stage [%d] ****\n", instCount);

	Instruction *inst = &test_inst;
	int64_t val_a, val_b, val_e, val_c = 0;

	switch (inst->optype)
	{
		case Op_add:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a + val_b;
			break;
		case Op_mul:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a * val_b;
			break;
		case Op_sub:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a - val_b;
			break;
		case Op_sll:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a << val_b;
			break;		
		case Op_mulh:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (int64_t)(((__int128_t)val_a * (__int128_t)val_b) >> 64);
			break;
		case Op_slt:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a < val_b? 1 : 0;
			break;
		case Op_sltu:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (uint64_t)val_a < (uint64_t)val_b? 1 : 0;
			break;
		case Op_xor:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a ^ val_b;
			break;
		case Op_div:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a / val_b;
			break;
		case Op_srl:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (int64_t)((uint64_t)val_a >> val_b);
			break;
		case Op_sra:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a >> val_b;	
			break;
		case Op_or:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a | val_b;	
			break;
		case Op_rem:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a % val_b;
			break;
		case Op_and:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = val_a & val_b;
			break;
		case Op_addw:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (int64_t)((int32_t)(val_a + val_b));
			break;
		case Op_subw:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (int64_t)((int32_t)(val_a - val_b));
			break;
		case Op_sllw:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (int64_t)((int32_t)val_a << val_b);
			break;
		case Op_srlw:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (uint64_t)((uint32_t)val_a >> val_b);
			break;
		case Op_sraw:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (int64_t)((int32_t)val_a >> val_b);
			break;
		case Op_sb:
		case Op_sh:
		case Op_sw:
		case Op_sd:
			val_c = Forward(inst->rs2);
		case Op_lb:
		case Op_lh:
		case Op_lw:
		case Op_ld:
		case Op_lbu:
		case Op_lhu:
		case Op_lwu:
		case Op_addi:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = val_a + val_b;	
			break;
		case Op_slli:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = val_a << (val_b & 0x3f);
			break;	
		case Op_slti:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = val_a < val_b? 1 : 0;
			break;
		case Op_sltiu:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = (uint64_t)val_a < (uint64_t)val_b? 1 : 0;
			break;
		case Op_xori:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = val_a ^ val_b;
			break;
		case Op_srli:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = (int64_t)((uint64_t)val_a >> (val_b & 0x3f));
			break;
		case Op_srai:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = val_a >> (val_b & 0x3f);
			break;
		case Op_ori:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = val_a | val_b;
			break;
		case Op_andi:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = val_a & val_b;
			break;
		case Op_addiw:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = (int64_t)((int32_t)(val_a + val_b));
			break;
		case Op_slliw:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = (int64_t)((int32_t)val_a << (val_b & 0x1f));
			break;	
		case Op_srliw:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = (uint64_t)((uint32_t)val_a >> (val_b & 0x1f));
			break;	
		case Op_sraiw:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = (int64_t)((int32_t)val_a >> (val_b & 0x1f));
			break;	
		case Op_jalr:
			val_a = Forward(inst->rs1);
			val_b = inst->imm;
			val_e = Forward(PCReg) + 4;
			val_c = (val_a + val_b) & (-1ll ^ 0x1);
			break;
		case Op_ecall:
			break;
		case Op_beq:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (val_a == val_b);
			val_c = Forward(PCReg) + inst->imm;
			break;
		case Op_bne:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (val_a != val_b);
			val_c = Forward(PCReg) + inst->imm;
			break;
		case Op_blt:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (val_a < val_b);
			val_c = Forward(PCReg) + inst->imm;
			break;
		case Op_bltu:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = ((uint64_t)val_a < (uint64_t)val_b);
			val_c = Forward(PCReg) + inst->imm;
			break;
		case Op_bge:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = (val_a >= val_b);
			val_c = Forward(PCReg) + inst->imm;
			break;
		case Op_bgeu:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = ((uint64_t)val_a >= (uint64_t)val_b);
			val_c = Forward(PCReg) + inst->imm;
			break;
		case Op_auipc:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = Forward(PCReg) + inst->imm; 
			break;
		case Op_lui:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = inst->imm;
			break;
		case Op_jal:
			val_a = Forward(inst->rs1);
			val_b = Forward(inst->rs2);
			val_e = Forward(PCReg) + 4;
			val_c = Forward(PCReg) + inst->imm;
			break;

		default:
			vprintf("[Error] Unknown optype %d. [Execute]\n",
					 inst->optype);
			return false;
	}

	dprintf("val_e = 0x%016llx  val_c = 0x%016llx\n", val_e, val_c);
	WriteReg(E_ValEReg, val_e);
	WriteReg(E_ValCReg, val_c);

	return true;
}

bool
Machine::MemoryAccess()
{
	dprintf("\n**** Memory Stage [%d] ****\n", instCount);

	Instruction *inst = &test_inst;
	int64_t val_e = ReadReg(E_ValEReg), val_c = ReadReg(E_ValCReg);

	switch (inst->optype)
	{
		case Op_lb:
			if (!ReadMem(val_e, 1, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			val_e = (int64_t)((int8_t)val_c);
			break;
		case Op_lh:
			if (!ReadMem(val_e, 2, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			val_e = (int64_t)((int16_t)val_c);
			break;
		case Op_lw:
			if (!ReadMem(val_e, 4, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			val_e = (int64_t)((int32_t)val_c);
			break;
		case Op_ld:
			if (!ReadMem(val_e, 8, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			val_e = (int64_t)val_c;
			break;
		case Op_lbu:
			if (!ReadMem(val_e, 1, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			val_e = (uint64_t)((uint8_t)val_c);
			break;
		case Op_lhu:
			if (!ReadMem(val_e, 2, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			val_e = (uint64_t)((uint16_t)val_c);
			break;
		case Op_lwu:
			if (!ReadMem(val_e, 4, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			val_e = (uint64_t)((uint32_t)val_c);
			break;
		case Op_sb:
			if (!WriteMem(val_e, 1, (uint8_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			break;
		case Op_sh:
			if (!WriteMem(val_e, 2, (uint16_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			break;
		case Op_sw:
			if (!WriteMem(val_e, 4, (uint32_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			break;
		case Op_sd:
			if (!WriteMem(val_e, 8, (uint64_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return false;
			}
			break;

		default:
			dprintf("No memory access.\n");
	}

	dprintf("val_e = 0x%016llx  val_c = 0x%016llx\n", val_e, val_c);
	WriteReg(M_ValEReg, val_e);
	WriteReg(M_ValCReg, val_c);
	return true;
}

bool
Machine::WriteBack()
{
	dprintf("\n**** WriteBack Stage [%d] ****\n", instCount);

	// TODO Predict PC
	WriteReg(PCReg, ReadReg(PCReg) + 4);

	Instruction *inst = &test_inst;
	int64_t val_e = ReadReg(M_ValEReg), val_c = ReadReg(M_ValCReg);

	switch (inst->optype)
	{
		case Op_jalr:
		case Op_jal:
			WriteReg(PCReg, val_c);
		case Op_add:
		case Op_addw:
		case Op_mul:
		case Op_sub:
		case Op_subw:
		case Op_sll:
		case Op_sllw:		
		case Op_mulh:
		case Op_slt:
		case Op_sltu:
		case Op_xor:
		case Op_div:
		case Op_srl:
		case Op_sra:
		case Op_srlw:
		case Op_sraw:
		case Op_or:
		case Op_rem:
		case Op_and:
		case Op_lb:
		case Op_lh:
		case Op_lw:
		case Op_ld:
		case Op_lbu:
		case Op_lhu:
		case Op_lwu:
		case Op_addi:
		case Op_slli:
		case Op_slliw:	
		case Op_slti:	
		case Op_sltiu:
		case Op_xori:
		case Op_srli:
		case Op_srliw:
		case Op_srai:
		case Op_sraiw:
		case Op_ori:
		case Op_andi:
		case Op_addiw:
		case Op_auipc:
		case Op_lui:
			WriteReg(inst->rd, val_e);
			break;

		case Op_beq:
		case Op_bne:
		case Op_blt:
		case Op_bltu:
		case Op_bge:
		case Op_bgeu:
			if (val_e == 1)
				WriteReg(PCReg, val_c);
			break;

		case Op_ecall:
			val_e = ReadReg(A0Reg);
			val_c = ReadReg(A7Reg);
			switch(val_c)
			{
				case 0:
					printf("%d", val_e);
					break;
				case 1:
					printf("%c", val_e);
					break;
				case 2:
					char chr;
					int lim;
					lim = 0;
					while(ReadMem((uint64_t)val_e, 1, (void*)&chr) && chr != '\0' && (++lim) <= 100)
					{
						printf("%c", chr);
						val_e += 1;
					}
					if (lim >= MaxStrLen)
					{
						vprintf("[Warning] String cut due to length exceeding (> %d).\n", MaxStrLen);
					}
					break;
			  	case 3:
				    scanf("%lld", &val_e);
				    WriteReg(A0Reg, val_e);
				    break;
				case 4:
					scanf("%c", (char*)&val_e);
				    val_e = (int64_t)((char)val_e);
				    WriteReg(A0Reg, val_e);
				    break;
				case 93:
					printf("User program exited.\n");
					Status();
					exit(0);
					break;
				default:
					vprintf("[Error] Unknown syscall a0=0x%llx a7=0x%llx. [WriteBack]\n", val_e, val_c);
					return false;
			}
			break;
			
		default:
			dprintf("No writeback.\n");
	}

	if (debug)
	{
		PrintReg();
	}
	return true;
}

void
Instruction::PrintInst()
{
	if (optype < 0 || optype >= OpNum)
		goto OP_UNKNOWN;

	printf("[0x%016llx] %s ", adr, op_str[optype]);
	switch (type)
	{
		case Rtype:
			printf("%s, %s, %s\n", reg_str[rd], reg_str[rs1], reg_str[rs2]);
			break;
		case Itype:
			if (opcode == 0x03)
				printf("%s, %lld(%s)\n", reg_str[rd], imm, reg_str[rs1]);
			else if(opcode == 0x73)
				printf("\n");
			else
				printf("%s, %s, %lld\n", reg_str[rd], reg_str[rs1], imm);
			break;
		case Stype:
			printf("%s, %lld(%s)\n", reg_str[rs2], imm, reg_str[rs1]);
			break;
		case SBtype:
			printf("%s, %s, %lld\n", reg_str[rs1], reg_str[rs2], imm >> 1);
			break;
		case Utype:
			printf("%s, 0x%llx\n", reg_str[rd], (uint64_t)imm >> 12);
			break;
		case UJtype:
			printf("%s, %lld\n", reg_str[rd], imm >> 1);
			break;
		default:
			goto OP_UNKNOWN;
	}
	return;

	OP_UNKNOWN:
	printf("unknown instruction\n");
}
