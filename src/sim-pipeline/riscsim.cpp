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
	"bgeu", "lbu", "lhu", "lwu", "sltiu",
	"sltu", "slliw", "srliw", "sraiw", "addw",
	"subw", "sllw", "srlw", "sraw"
};

const char *reg_str[34] = {
    "zero", "ra", "sp", "gp", "tp",   
    "t0", "t1", "t2", "s0", "s1",   
    "a0", "a1", "a2", "a3", "a4",  
    "a5", "a6", "a7", "s2", "s3",  
    "s4", "s5", "s6", "s7", "s8",  
    "s9", "s10", "s11", "t3", "t4",  
    "t5", "t6", "pc", "p_pc"  
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

bool predict_pc_updated;
uint64_t f_pred_pc;

int
Machine::Fetch()
{
	dprintf("\n**** Fetch Stage [%d] ****\n", cycCount);
	predict_pc_updated = false;

	if (F_reg.bubble) // output bubble
	{
		F_reg.bubble = false;
		F_reg.stall  = false;

		vprintf("[F] ---BUBBLE---\n");
		f_reg.bubble = true;
		f_reg.stall  = false;

		return 1;
	}

	Instruction *inst = &F_reg.inst;
	uint64_t inst_adr = ReadReg(P_PCReg);

	dprintf("fetching operation from 0x%016llx.\n", inst_adr);

	inst->adr = inst_adr;
	if (!ReadMem(inst_adr, 4, (void*)&(inst->value)))
	{
		vprintf("--Can not fetch operation. [Fetch]\n");
		return 0;
	}
	vprintf("[F] [0x%016llx] need decode 0x%08llx\n", inst_adr, inst->value);

	dprintf("op_value: 0x%08x\n", inst->value);
	f_reg = F_reg;
	f_reg.bubble = false;
	f_reg.stall  = false;

	if ((inst->value & 0x7f) == 0x63) // branch
	{
		f_reg.pred_j = predictor->Predict(inst_adr);
		if (f_reg.pred_j) // predict jump
		{
			dprintf("predictor: branch taken.\n");
			uint64_t jmp_imm = 0;
			uint32_t tmp_val = inst->value;
			maskvalue(7, tmp_val);
			jmp_imm = 0;
			jmp_imm |= (maskvalue(1, tmp_val) << 11);
			jmp_imm |= (maskvalue(4, tmp_val) << 1);
			maskvalue(13, tmp_val);
			jmp_imm |= (maskvalue(6, tmp_val) << 5);
			jmp_imm |= (maskvalue(1, tmp_val) << 12);
			jmp_imm = signext64(13, jmp_imm);

			f_pred_pc = inst->adr + jmp_imm;
		}
		else
		{		
			dprintf("predictor: branch not taken.\n");
			f_pred_pc = inst->adr + 4;
		}
	}
	else if ((inst->value & 0x7f) == 0x6f) // jal
	{
		uint64_t jmp_imm = 0;
		uint32_t tmp_val = inst->value;
		maskvalue(12, tmp_val);
		jmp_imm |= (maskvalue(8, tmp_val) << 12);
		jmp_imm |= (maskvalue(1, tmp_val) << 11);
		jmp_imm |= (maskvalue(10, tmp_val) << 1);
		jmp_imm |= (maskvalue(1, tmp_val) << 20);
		jmp_imm = signext64(21, jmp_imm);

		f_pred_pc = inst->adr + jmp_imm;
		dprintf("pipeline: JAL prefetch, jump to 0x%08llx.\n", inst->adr + jmp_imm);
	}
	else
	{
		f_pred_pc = inst->adr + 4;
		f_reg.pred_j = false;
	}

	return 1;
}

bool data_forwarded_rs1;
bool data_forwarded_rs2;

int
Machine::Decode()
{
	dprintf("\n**** Decode Stage [%d] ****\n", cycCount);

	if (D_reg.bubble) // output bubble
	{
		vprintf("[D] ---BUBBLE---\n");
		d_reg = D_reg;
		d_reg.bubble = true;
		d_reg.stall  = false;
		data_forwarded_rs2 = true;
		data_forwarded_rs1 = true;
		return 1;
	}

	Instruction *inst = &D_reg.inst;	
	uint32_t value = inst->value;
	uint32_t opcode = maskvalue(7, value);
	inst->opcode = opcode;

	inst->rd  = 0;
	inst->rs1 = 0;
	inst->rs2 = 0;
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
			return 0;
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
					return 0;
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
					return 0;	
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
					return 0;
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
					return 0;
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
					return 0;
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
					return 0;	
			}
			break;
		default:
			ASSERT(false);
			// never reach
	}

	if (debug || verbose)
	{
		printf("[D] ");
		inst->PrintInst();
	}

	d_reg = D_reg;
	d_reg.bubble = false;
	d_reg.stall  = false;
	d_reg.val_e = ReadReg(inst->rs1);
	d_reg.val_c = ReadReg(inst->rs2);
	data_forwarded_rs1 = false;
	data_forwarded_rs2 = false;

	// cannot get data in rs1
	if (inst->optype == Op_jalr)
	{
		F_reg.bubble = true;
		f_reg.bubble = true;
		dprintf("pipeline: JALR stall.\n");
	}

	if (inst->optype == Op_ecall)
	{
		F_reg.stall  = true;
		f_reg.bubble = true;
		dprintf("pipeline: ECALL stall.\n");
	}

	return 1;
}

int
Machine::Execute()
{
	int use_cyc = 1;
	dprintf("\n**** Execute Stage [%d] ****\n", cycCount);

	if (E_reg.bubble) // output bubble
	{
		vprintf("[E] ---BUBBLE---\n");
		e_reg = E_reg;
		e_reg.bubble = true;
		e_reg.stall  = false;
		return 1;
	}

	Instruction *inst = &E_reg.inst;
	if (debug || verbose)
	{
		printf("[E] ");
		inst->PrintInst();
	}
	int64_t val_a, val_b, val_e = 0, val_c = 0;
	val_a = E_reg.val_e;
	val_b = E_reg.val_c;

	switch (inst->optype)
	{
		case Op_add:
			val_e = val_a + val_b;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_mul:
			val_e = val_a * val_b;
			use_cyc = cfg.u32_cfg[MUL64_CYC];
			break;
		case Op_sub:
			val_e = val_a - val_b;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_sll:
			val_e = val_a << val_b;
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;		
		case Op_mulh:
			val_e = (int64_t)(((__int128_t)val_a * (__int128_t)val_b) >> 64);
			use_cyc = cfg.u32_cfg[MUL64_CYC];
			break;
		case Op_slt:
			val_e = val_a < val_b? 1 : 0;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_sltu:
			val_e = (uint64_t)val_a < (uint64_t)val_b? 1 : 0;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_xor:
			val_e = val_a ^ val_b;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_div:
			val_e = val_a / val_b;
			use_cyc = cfg.u32_cfg[DIV64_CYC];
			break;
		case Op_srl:
			val_e = (int64_t)((uint64_t)val_a >> val_b);
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;
		case Op_sra:
			val_e = val_a >> val_b;	
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;
		case Op_or:
			val_e = val_a | val_b;	
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_rem:
			val_e = val_a % val_b;
			use_cyc = cfg.u32_cfg[DIV64_CYC];
			break;
		case Op_and:
			val_e = val_a & val_b;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_addw:
			val_e = (int64_t)((int32_t)(val_a + val_b));
			use_cyc = cfg.u32_cfg[ADD32_CYC];
			break;
		case Op_subw:
			val_e = (int64_t)((int32_t)(val_a - val_b));
			use_cyc = cfg.u32_cfg[ADD32_CYC];
			break;
		case Op_sllw:
			val_e = (int64_t)((int32_t)val_a << val_b);
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;
		case Op_srlw:
			val_e = (uint64_t)((uint32_t)val_a >> val_b);
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;
		case Op_sraw:
			val_e = (int64_t)((int32_t)val_a >> val_b);
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;
		case Op_sb:
		case Op_sh:
		case Op_sw:
		case Op_sd:
			val_c = val_b;
		case Op_lb:
		case Op_lh:
		case Op_lw:
		case Op_ld:
		case Op_lbu:
		case Op_lhu:
		case Op_lwu:
		case Op_addi:
			val_b = inst->imm;
			val_e = val_a + val_b;	
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_slli:
			val_b = inst->imm;
			val_e = val_a << (val_b & 0x3f);
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;	
		case Op_slti:
			val_b = inst->imm;
			val_e = val_a < val_b? 1 : 0;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_sltiu:
			val_b = inst->imm;
			val_e = (uint64_t)val_a < (uint64_t)val_b? 1 : 0;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_xori:
			val_b = inst->imm;
			val_e = val_a ^ val_b;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_srli:
			val_b = inst->imm;
			val_e = (int64_t)((uint64_t)val_a >> (val_b & 0x3f));
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;
		case Op_srai:
			val_b = inst->imm;
			val_e = val_a >> (val_b & 0x3f);
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;
		case Op_ori:
			val_b = inst->imm;
			val_e = val_a | val_b;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_andi:
			val_b = inst->imm;
			val_e = val_a & val_b;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_addiw:
			val_b = inst->imm;
			val_e = (int64_t)((int32_t)(val_a + val_b));
			use_cyc = cfg.u32_cfg[ADD32_CYC];
			break;
		case Op_slliw:
			val_b = inst->imm;
			val_e = (int64_t)((int32_t)val_a << (val_b & 0x1f));
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;	
		case Op_srliw:
			val_b = inst->imm;
			val_e = (uint64_t)((uint32_t)val_a >> (val_b & 0x1f));
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;	
		case Op_sraiw:
			val_b = inst->imm;
			val_e = (int64_t)((int32_t)val_a >> (val_b & 0x1f));
			use_cyc = cfg.u32_cfg[SFT_CYC];
			break;	
		case Op_jalr:
			val_b = inst->imm;
			val_e = inst->adr + 4;
			val_c = (val_a + val_b) & (-1ll ^ 0x1);
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			WriteReg(P_PCReg, val_c);
			F_reg.bubble = false;
			F_reg.stall  = false;
			predict_pc_updated = true;
			jalrStlCount++;
			dprintf("pipeline: JALR Predict PC update 0x%08llx.\n", val_c);
			break;
		case Op_ecall:
			break;
		case Op_beq:
			val_e = (val_a == val_b);
			val_c = inst->adr + inst->imm;
			goto PRED_JUDGE;
		case Op_bne:
			val_e = (val_a != val_b);
			val_c = inst->adr + inst->imm;
			goto PRED_JUDGE;
		case Op_blt:
			val_e = (val_a < val_b);
			val_c = inst->adr + inst->imm;
			goto PRED_JUDGE;
		case Op_bltu:
			val_e = ((uint64_t)val_a < (uint64_t)val_b);
			val_c = inst->adr + inst->imm;
			goto PRED_JUDGE;
		case Op_bge:
			val_e = (val_a >= val_b);
			val_c = inst->adr + inst->imm;
			goto PRED_JUDGE;
		case Op_bgeu:
			val_e = ((uint64_t)val_a >= (uint64_t)val_b);
			val_c = inst->adr + inst->imm;

			PRED_JUDGE:
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			if (E_reg.pred_j != val_e) // predict incorrectly
			{
				predictor->Update(inst->adr, val_e);
				ctrlHzdCount++;
				predict_pc_updated = true;
				WriteReg(P_PCReg, val_e ? val_c : inst->adr+4);
				F_reg.bubble = false;
				F_reg.stall  = false;
				f_reg.bubble = true;
				d_reg.bubble = true;
				dprintf("pipeline: Control Hazard (should 0x%08llx, but 0x%08llx).\n", val_e ? val_c : inst->adr+4,
																						val_e ? inst->adr+4 : val_c);
			}

			break;
		case Op_auipc:
			val_e = inst->adr + inst->imm; 
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;
		case Op_lui:
			val_e = inst->imm;
			break;
		case Op_jal:
			val_e = inst->adr + 4;
			val_c = inst->adr + inst->imm;
			use_cyc = cfg.u32_cfg[ADD64_CYC];
			break;

		default:
			vprintf("[Error] Unknown optype %d. [Execute]\n",
					 inst->optype);
			return 0;
	}

	dprintf("val_e = 0x%016llx  val_c = 0x%016llx\n", val_e, val_c);

	e_reg = E_reg;
	e_reg.bubble = false;
	e_reg.stall  = false;
	e_reg.val_e = val_e;
	e_reg.val_c = val_c;

	// data forwarding
	if (inst->rd) // exists and not zero reg
	{
		if (inst->opcode == 0x03) // load
		{
			if (inst->rd == d_reg.inst.rs1
				|| inst->rd == d_reg.inst.rs2)
			{
				// load-use hazard
				F_reg.stall  = true;
				f_reg.stall  = true;
				d_reg.bubble = true;
				data_forwarded_rs1 = true;
				data_forwarded_rs2 = true;
				loadHzdCount++;
				dprintf("pipeline: Load-Use Hazard on %s.\n", reg_str[inst->rd]);
			}
		}

		else
		{
			if (inst->rd == d_reg.inst.rs1)
			{
				d_reg.val_e = val_e;
				data_forwarded_rs1 = true;
				dprintf("pipeline: Data forwarding %s = %lld.\n", reg_str[inst->rd], val_e);
			}
			if (inst->rd == d_reg.inst.rs2)
			{
				d_reg.val_c = val_e;
				data_forwarded_rs2 = true;
				dprintf("pipeline: Data forwarding %s = %lld.\n", reg_str[inst->rd], val_e);
			}
		}
	}

	if (inst->optype == Op_ecall)
	{
		F_reg.stall  = true;
		f_reg.bubble = true;
		d_reg.bubble = true;
		dprintf("pipeline: ECALL stall.\n");
	}

	return use_cyc;
}

int
Machine::MemoryAccess()
{
	int use_cyc = cfg.u32_cfg[MEM_CYC];
	dprintf("\n**** Memory Stage [%d] ****\n", cycCount);

	if (M_reg.bubble) // output bubble
	{
		vprintf("[M] ---BUBBLE---\n");
		m_reg = M_reg;
		m_reg.bubble = true;
		m_reg.stall  = false;
		return 1;
	}

	Instruction *inst = &M_reg.inst;
	if (debug || verbose)
	{
		printf("[M] ");
		inst->PrintInst();
	}
	int64_t val_e = M_reg.val_e, val_c = M_reg.val_c;

	switch (inst->optype)
	{
		case Op_lb:
			if (!ReadMem(val_e, 1, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			val_e = (int64_t)((int8_t)val_c);
			break;
		case Op_lh:
			if (!ReadMem(val_e, 2, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			val_e = (int64_t)((int16_t)val_c);
			break;
		case Op_lw:
			if (!ReadMem(val_e, 4, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			val_e = (int64_t)((int32_t)val_c);
			break;
		case Op_ld:
			if (!ReadMem(val_e, 8, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			val_e = (int64_t)val_c;
			break;
		case Op_lbu:
			if (!ReadMem(val_e, 1, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			val_e = (uint64_t)((uint8_t)val_c);
			break;
		case Op_lhu:
			if (!ReadMem(val_e, 2, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			val_e = (uint64_t)((uint16_t)val_c);
			break;
		case Op_lwu:
			if (!ReadMem(val_e, 4, &val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			val_e = (uint64_t)((uint32_t)val_c);
			break;
		case Op_sb:
			if (!WriteMem(val_e, 1, (uint8_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			break;
		case Op_sh:
			if (!WriteMem(val_e, 2, (uint16_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			break;
		case Op_sw:
			if (!WriteMem(val_e, 4, (uint32_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			break;
		case Op_sd:
			if (!WriteMem(val_e, 8, (uint64_t)val_c))
			{
				vprintf("--Memory access error. [MemoryAccess]\n");
				return 0;
			}
			break;

		default:
			use_cyc = 1;
			dprintf("No memory access.\n");
	}

	dprintf("val_e = 0x%016llx  val_c = 0x%016llx\n", val_e, val_c);
	
	m_reg = M_reg;
	m_reg.bubble = false;
	m_reg.stall  = false;
	m_reg.val_e  = val_e;
	m_reg.val_c  = val_c;

	// data forwarding
	if (inst->rd)
	{
		if (inst->rd == d_reg.inst.rs1 && !data_forwarded_rs1)
		{
			d_reg.val_e = val_e;
			data_forwarded_rs1 = true;
			dprintf("pipeline: Data forwarding %s = %lld.\n", reg_str[inst->rd], val_e);
		}
		if (inst->rd == d_reg.inst.rs2 && !data_forwarded_rs2)
		{
			d_reg.val_c = val_e;
			data_forwarded_rs2 = true;
			dprintf("pipeline: Data forwarding %s = %lld.\n", reg_str[inst->rd], val_e);
		}
	}

	if (inst->optype == Op_ecall)
	{
		F_reg.stall  = true;
		f_reg.bubble = true;
		d_reg.bubble = true;
		e_reg.bubble = true;
		dprintf("pipeline: ECALL stall.\n");
	}

	return use_cyc;
}

int
Machine::WriteBack()
{
	dprintf("\n**** WriteBack Stage [%d] ****\n", cycCount);

	if (W_reg.bubble) // output bubble
	{
		vprintf("[W] ---BUBBLE---\n");
		return 1;
	}

	instCount++;
	Instruction *inst = &W_reg.inst;
	if (debug || verbose)
	{
		printf("[W] ");
		inst->PrintInst();
	}
	int64_t val_e = W_reg.val_e, val_c = W_reg.val_c;
	WriteReg(PCReg, inst->adr);

	switch (inst->optype)
	{
		case Op_jalr:
		case Op_jal:
			// WriteReg(PCReg, val_c);
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
			WriteReg(ZeroReg, 0); // should not be modified
			break;

		case Op_beq:
		case Op_bne:
		case Op_blt:
		case Op_bltu:
		case Op_bge:
		case Op_bgeu:
			// if (val_e == 1)
			//	WriteReg(PCReg, val_c);
			totalBranch++;
			goto NO_WRITEBACK;
			break;

		case Op_ecall:
			ecallStlCount++;
			val_e = ReadReg(A0Reg);
			val_c = ReadReg(A7Reg);
			dprintf("Syscall: a0=0x%llx a7=0x%llx.\n", val_e, val_c);
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
					return 0;
			}
			break;
			
		default:
		NO_WRITEBACK:
			dprintf("No writeback.\n");
	}

	// data forwarding
	if (inst->rd)
	{
		if (inst->rd == d_reg.inst.rs1 && !data_forwarded_rs1)
		{
			d_reg.val_e = val_e;
			data_forwarded_rs1 = true;
			dprintf("pipeline: Data forwarding %s = %lld.\n", reg_str[inst->rd], val_e);
		}
		if (inst->rd == d_reg.inst.rs2 && !data_forwarded_rs2)
		{
			d_reg.val_c = val_e;
			data_forwarded_rs2 = true;
			dprintf("pipeline: Data forwarding %s = %lld.\n", reg_str[inst->rd], val_e);
		}
	}

	if (inst->optype == Op_ecall)
	{
		d_reg.bubble = true;
		e_reg.bubble = true;
		m_reg.bubble = true;
		dprintf("pipeline: ECALL stall.\n");
	}
	return 1;
}

void
Machine::UpdatePipeline()
{
	// update pipeline registers
	if (!F_reg.stall && !predict_pc_updated)
		WriteReg(P_PCReg, f_pred_pc);
	F_reg.stall = false;

	if (!f_reg.stall)
		D_reg = f_reg;
	if (!d_reg.stall)
		E_reg = d_reg;
	if (!e_reg.stall)
		M_reg = e_reg;
	W_reg = m_reg;
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
