#include "rdShader.h"

#ifdef RENDER_DROID2

#include "Win95/std.h"
#include "General/stdMath.h"
#include "General/stdString.h"
#include "General/stdFileUtil.h"
#include "General/stdFnames.h"
#include "General/stdHashTable.h"
#include "Engine/rdroid.h"

#include "Modules/std/std3D.h"

#define RD_SHADER_OPCODE(name, value) \
    { #name, value },

typedef struct rdShader_opcodeNames
{
	const char* name;
	uint8_t     token;
} rdShader_OpCode;

static const rdShader_OpCode rdShader_opcodeNames[] =
{
#include "rdShader_OpCodes.h"
};

#undef RD_SHADER_OPCODE

typedef struct
{
	uint8_t idx;
	uint8_t type;
	uint8_t fmt;
	uint8_t address;
	uint8_t swizzle;
	uint8_t mask;
	uint8_t abs;
	uint8_t negate_or_invert;
	float   immediate;
} rdShader_Register;

typedef struct
{
	uint8_t           mult;
	uint8_t           reduction;
	rdShader_Register reg;
} rdShader_SrcOperand;

typedef struct
{
	uint8_t           multiplier;
	rdShader_Register reg;
} rdShader_DestOperand;

typedef struct
{
	uint8_t              opcode;
	uint8_t              precise;
	uint8_t              clamp;
	uint8_t              srcCount;
	rdShader_DestOperand dest;
	rdShader_SrcOperand  src[3];
} rdShader_Instruction;

typedef struct rdShader_Alias
{
	char                   name[16];
	char                   reg[16];
	struct rdShader_Alias* next;
} rdShader_Alias;

typedef struct
{
	stdHashTable*   aliasHash;
	rdShader_Alias* firstAlias;
	rdShader*       shader;
} rdShader_Assembler;

rdShader_Assembler* rdShader_pCurrentAssembler = NULL;

static uint8_t rdShader_ParseOpCode(const char* name)
{
	char* swizzle = strchr(name, '.');
	if (swizzle)
		*swizzle = '\0';

	for (uint8_t i = 0; i < (RD_SHADER_MAX_OPS & 0x1F); ++i)
	{
		if (stricmp(name, rdShader_opcodeNames[i].name) == 0)
		{
			if (swizzle)
				*swizzle = '.';
			return rdShader_opcodeNames[i].token;
		}
	}
	if (swizzle)
		*swizzle = '.';
	return RD_SHADER_OP_NOP;
}

static uint8_t rdShader_OperandCount(uint8_t opcode)
{
	return (opcode >> 5) & 0x3;
}

static uint8_t rdShader_ParseSrcRegisterType(char c)
{
	switch (c)
	{
	case 'r': return RD_SHADER_GPR;
	case 't': return RD_SHADER_TEX;
	case 'v': return RD_SHADER_CLR;
	case 'c': return RD_SHADER_CON;
	case 's': return RD_SHADER_SYS;
	default:  return 0;
	}
}

static uint8_t rdShader_ParseMultiplier(const char* token)
{
	if (strnicmp(token, "mul:2", 5) == 0) return RD_SHADER_X2;
	if (strnicmp(token, "mul:4", 5) == 0) return RD_SHADER_X4;
	if (strnicmp(token, "div:2", 5) == 0) return RD_SHADER_D2;
	if (strnicmp(token, "div:4", 5) == 0) return RD_SHADER_D4;

	if (strncmp(token, "bias", 4) == 0) return RD_SHADER_BIAS;
	if (strncmp(token, "expand", 6) == 0) return RD_SHADER_EXPAND;

	return 0;
}

static uint8_t rdShader_ParseFormat(const char* token)
{
	if (strnicmp(token, "fmt:ubyte4", 10) == 0) return RD_SHADER_U8;
	if (strnicmp(token, "fmt:sbyte4", 10) == 0) return RD_SHADER_S8;
	if (strnicmp(token, "fmt:half2", 9) == 0) return RD_SHADER_F16;
	if (strnicmp(token, "fmt:float", 9) == 0) return RD_SHADER_F32;
	return RD_SHADER_U8;
}

static int8_t rdShader_ParseSysRegister(const char* name)
{
	static const char* keywords[] =
	{
		"sv:sec", "sv:xy", "sv:z", "sv:pos", "sv:uv", "sv:aspect", "mat:fill", "mat:albedo", "mat:glow", "mat::f0", "mat::roughness", "mat:displacement"
	};

	for (uint8_t i = 0; i < RD_SHADER_SYS_MAX; ++i)
	{
		if (stricmp(name, keywords[i]) == 0)
			return i;
	}
	return -1;
}

static uint8_t rdShader_ParseReduction(const char* name)
{
	static const char* keywords[RD_SHADER_SRC_RED_COUNT] =
	{
		"", "lum", "sum", "avg", "min", "max", "mag",
	};

	for (uint8_t i = 0; i < RD_SHADER_SRC_RED_COUNT; ++i)
	{
		if (strnicmp(name, keywords[i], 3) == 0)
			return i;
	}

	return 0;
}

static uint8_t rdShader_ParseDstRegisterType(char c)
{
	if (c == 'r')
		return RD_SHADER_GPR;
	return 0; // todo: error
}

static uint8_t rdShader_ParseSwizzle(const char* swizzle)
{
	const int length = strlen(swizzle);
	if (length == 0 || length > 4) return 0xE4; // Default "xyzw"

	uint8_t mask = 0;
	char last = 'x'; // Default to 'x' if missing

	for (int i = 0; i < 4; i++)
	{
		char c = (i < length) ? swizzle[i] : last; // Extend last valid character
		switch (c)
		{
		case 'x': case 'r': last = 'x'; mask |= (0 << (i * 2)); break;
		case 'y': case 'g': last = 'y'; mask |= (1 << (i * 2)); break;
		case 'z': case 'b': last = 'z'; mask |= (2 << (i * 2)); break;
		case 'w': case 'a': last = 'w'; mask |= (3 << (i * 2)); break;
		default: return 0xE4; // Invalid input, return default
		}
	}
	return mask;
}

static int rdShader_ExtractSwizzle(const char* expression, char* swizzle_out)
{
	int mask = 0;

	// Lookup table for swizzle characters to write masks
	const int swizzle_map[] = { WRITE_RED, WRITE_GREEN, WRITE_BLUE, WRITE_ALPHA };
	const char swizzle_chars[] = { 'x', 'y', 'z', 'w', 'r', 'g', 'b', 'a', '\0' };

	int i;
	for (i = 1; i < 5; i++)
	{
		// max 4 swizzle chars (xyzw or rgba)
		char c = expression[i];
		if (c != 'x' && c != 'y' && c != 'z' && c != 'w'
			&& c != 'r' && c != 'g' && c != 'b' && c != 'a')
		{
			break; // stop at first non-swizzle character
		}
		swizzle_out[i - 1] = c;

		int idx = strchr(swizzle_chars, c) - swizzle_chars;
		mask |= swizzle_map[idx & 3]; // set the appropriate mask
	}

	swizzle_out[i - 1] = '\0'; // null-terminate

	return mask;
}

static char* rdShader_ParseRegister(char* token, char* swizzle, rdShader_Register* reg)
{
	// extract register index or immediate value
	if (reg->type == RD_SHADER_IMM16 || reg->type == RD_SHADER_IMM8)
	{
		reg->immediate = atof(token);
		reg->swizzle = SWIZZLE_XYZW;
		reg->mask = WRITE_RGBA;

		union
		{
			int8_t s;
			uint8_t u;
		} pack;
		pack.s = (int8_t)roundf(stdMath_Clamp(reg->immediate, -1.0f, 1.0f) * 127.0f);
		reg->address = pack.u;
	}
	else
	{
		if (reg->type != RD_SHADER_SYS && reg->address == 0xFF)
			reg->address = atoi(token);

		// handle the swizzle mask
		if (swizzle)
		{
			char mask[5] = { 'x', 'y', 'z', 'w', '\0' };
			reg->mask = rdShader_ExtractSwizzle(swizzle, mask);
			reg->swizzle = rdShader_ParseSwizzle(mask);
		}
		else
		{
			reg->swizzle = SWIZZLE_XYZW;
			reg->mask = WRITE_RGBA;
		}
	}

	return token;
}

static void rdShader_ParseDestinationOperand(char* token, rdShader_DestOperand* op)
{
	op->reg.address = 0xFF;

	// find the swizzle
	char* swizzle = strchr(token, '.');

	// if this isn't an immediate value, clear the swizzle
	if (isalpha(token[0]) && swizzle)
		*swizzle = '\0';
		
	char* alias = (char*)stdHashTable_GetKeyVal(rdShader_pCurrentAssembler->aliasHash, token);
	if (alias)
	{
		op->reg.type = rdShader_ParseDstRegisterType(alias[0]);
		op->reg.address = atoi(alias + 1);
	}
	else
	{
		char typeChar = token[0];
		if ((typeChar != 'r') && (typeChar != 'R'))
			return; // todo: error
		++token;
	}

	if (swizzle)
		*swizzle = '.';

	rdShader_ParseRegister(token, swizzle, &op->reg);
}

static char* rdShader_ParseSourceRegister(char* token, rdShader_Register* reg)
{
	// find the swizzle
	char* swizzle = strchr(token, '.');

	if (isdigit(token[0])) // immediate value
	{
		if (reg->idx >= 3)
			reg->type = RD_SHADER_IMM8;
		else
			reg->type = RD_SHADER_IMM16;
	}
	else
	{
		if (swizzle)
			*swizzle = '\0'; // set to null so we can parse the name

		// check for an alias
		char* alias = (char*)stdHashTable_GetKeyVal(rdShader_pCurrentAssembler->aliasHash, token);

		// check for a system name
		int8_t addr = rdShader_ParseSysRegister(alias ? alias : token);
		if (addr >= 0)
		{
			reg->type = RD_SHADER_SYS;
			reg->address = addr;
		}
		else
		{
			if (alias)
			{
				reg->type = rdShader_ParseSrcRegisterType(alias[0]);
				reg->address = atoi(alias + 1);
			}
			else
			{
				reg->type = rdShader_ParseSrcRegisterType(token[0]);
				++token;
			}
		}

		if (swizzle)
			*swizzle = '.';
	}

	token = rdShader_ParseRegister(token, swizzle, reg);

	return token;
}

static void rdShader_ParseSourceOperandExpression(char* token, rdShader_SrcOperand* op)
{
	token = rdShader_ParseSourceRegister(token, &op->reg);
	while (isspace(*token)) // skip whitespace
		token++;

	char* modifiers = strchr(token, '[');
	if (modifiers)
	{
		char tmp[128];
		char* arg = stdString_GetEnclosedStringContents(modifiers, tmp, 128, '[', ']');
		if (!arg)
			return;

		char* modifier = tmp;
		while (*modifier)
		{
			char* next_comma = strchr(modifier, ' ');
			if (next_comma)
				*next_comma = '\0';

			// trim whitespace from the current modifier
			while (isspace(*modifier))
				modifier++;

			// try to parse multipliers first
			uint8_t mult = rdShader_ParseMultiplier(modifier);
			if (mult)
				op->mult = mult;
			else if (strnicmp(modifier, "negate", 6) == 0)
				op->reg.negate_or_invert = RD_SHADER_NEGATE;
			else if (strnicmp(modifier, "invert", 6) == 0)
				op->reg.negate_or_invert = RD_SHADER_INVERT;
			else
				op->reg.fmt = rdShader_ParseFormat(modifier);

			if (next_comma)
				modifier = next_comma + 1;
			else
				break;
		}
	}
}

// source operand layout
//  |31         30|29          27|26       25|24       16|15    8|7   6|5   4|  3  |2    0|
//  |  reduction  |  scale/bias  |  neg/inv  |  swizzle  |  addr | idx | fmt | abs | type |
static uint32_t rdShader_AssembleSrc(
	uint8_t idx,
	uint8_t type,
	uint8_t fmt,
	uint8_t addr,
	uint8_t abs,
	uint8_t swizzle,
	uint8_t negate_or_invert,
	uint8_t scale_bias,
	uint8_t reduction
)
{
	uint32_t result;
	result = type & 0x7;
	result |= (abs & 0x1) << 3;
	result |= (fmt & 0x3) << 4;
	result |= (idx & 0x3) << 6;
	result |= (addr & 0xFF) << 8;
	result |= (swizzle & 0xFF) << 16;
	result |= (negate_or_invert & 0x3) << 24;
	result |= (scale_bias & 0x7) << 26;
	result |= (reduction & 0x7) << 29;

	return result;
}

// operation + destination layout
// |31         29|  28 |27          25|24       17|16     10|    9    |8      7|    6    |5       0|
// | write mask  | abs |  multiplier  |  swizzle  |  index  |  negate | format | precise | op code |
static uint32_t rdShader_AssembleOpAndDst(
	uint8_t opcode,
	uint8_t fmt,
	uint8_t addr,
	uint8_t swizzle,
	uint8_t multiplier,
	uint8_t write_mask,
	uint8_t precise,
	uint8_t abs,
	uint8_t neg,
	uint8_t clamp
)
{
	uint32_t result;
	result  = (opcode & 0x1F);
	result |= (precise & 0x1) << 5;
	result |= (fmt & 0x3) << 6;
	result |= (neg & 0x1) << 8;
	result |= (clamp & 0x1) << 9;
	result |= (addr & 0x3F) << 10;
	result |= (swizzle & 0xFF) << 16;
	result |= (multiplier & 0x7) << 24;
	result |= (abs & 0x1) << 27;
	result |= (write_mask & 0xF) << 28;
	return result;
}

static int rdShader_AssembleInstruction(rdShaderInstr* result, rdShader_Instruction* inst)
{
	result->op_dst = rdShader_AssembleOpAndDst(inst->opcode,
											   inst->dest.reg.fmt,
											   inst->dest.reg.address,
											   inst->dest.reg.swizzle,
											   inst->dest.multiplier,
											   inst->dest.reg.mask,
											   inst->precise,
											   inst->dest.reg.abs,
											   inst->dest.reg.negate_or_invert,
											   inst->clamp);


	// if we have 3 operands, then immediate values must be 8 bit
	if (inst->srcCount >= 3)
	{
		if (inst->src[0].reg.type == RD_SHADER_IMM16)
			inst->src[0].reg.type = RD_SHADER_IMM8;

		if (inst->src[1].reg.type == RD_SHADER_IMM16)
			inst->src[1].reg.type = RD_SHADER_IMM8;
	}

	result->src0 = rdShader_AssembleSrc(0,
										inst->src[0].reg.type,
										inst->src[0].reg.fmt,
										inst->src[0].reg.address,
										inst->src[0].reg.abs,
										inst->src[0].reg.swizzle,
										inst->src[0].reg.negate_or_invert,
										inst->src[0].mult,
										inst->src[0].reduction);

	result->src1 = rdShader_AssembleSrc(1,
										inst->src[1].reg.type,
										inst->src[1].reg.fmt,
										inst->src[1].reg.address,
										inst->src[1].reg.abs,
										inst->src[1].reg.swizzle,
										inst->src[1].reg.negate_or_invert,
										inst->src[1].mult,
										inst->src[1].reduction);

	// for 2 operands or less we can encode floating point immediate values
	if (inst->srcCount < 3)
	{
		float imm0 = inst->src[0].reg.immediate;
		float imm1 = inst->src[1].reg.immediate;
		result->src2 = stdMath_PackHalf2x16(imm0, imm1);
	}
	else
	{
		result->src2 = rdShader_AssembleSrc(2,
											inst->src[2].reg.type,
											inst->src[2].reg.fmt,
											inst->src[2].reg.address,
											inst->src[2].reg.abs,
											inst->src[2].reg.swizzle,
											inst->src[2].reg.negate_or_invert,
											inst->src[2].mult,
											inst->src[2].reduction);
	}

	return 1;
}

static void rdShader_ParseAlias(char* name)
{
	char* comma = strchr(name, ',');
	if (comma)
	{
		*comma = '\0';
		while (isspace(*name))
			name++;

		char* alias = (char*)stdHashTable_GetKeyVal(rdShader_pCurrentAssembler->aliasHash, name);
		if (!alias)
		{
			rdShader_Alias* alias = (rdShader_Alias*)malloc(sizeof(rdShader_Alias));
			if (!alias)
			{
				// todo: error
				return;
			}
			stdString_SafeStrCopy(alias->name, name, 16);

			char* reg = comma + 1;
			while (isspace(*reg))
				reg++;
			stdString_SafeStrCopy(alias->reg, reg, 16);

			alias->next = rdShader_pCurrentAssembler->firstAlias;
			rdShader_pCurrentAssembler->firstAlias = alias;
			alias = stdHashTable_SetKeyVal(rdShader_pCurrentAssembler->aliasHash, alias->name, alias->reg);
		}
	}
}

static void rdShader_ParseSourceOperandWithModifiers(char* token, rdShader_SrcOperand* op)
{
	if (token[0] == '-') // check for negate
	{
		op->reg.negate_or_invert = RD_SHADER_NEGATE;
		token++;
	}
	else if (strncmp(token, "1 - ", 4) == 0) // check for invert, todo: fix spaces
	{
		op->reg.negate_or_invert = RD_SHADER_INVERT;
		token += 4;
	}

	if (strncmp(token, "abs", 3) == 0) // check for abs
	{
		op->reg.abs = 1;
		token += 3;

		char temp[128];
		char* arg = stdString_GetEnclosedStringContents(token, temp, 128, '(', ')');
		if (arg)
		{
			rdShader_ParseSourceOperandExpression(temp, op);
		}
		else
		{
			// todo: error   
		}
	}
	if (token[0] == '(') // check for expression
	{
		char temp[128];
		char* arg = stdString_GetEnclosedStringContents(token, temp, 128, '(', ')');
		if (arg)
		{
			rdShader_ParseSourceOperandExpression(temp, op);
		}
		else
		{
			// todo: error   
		}
	}
	else
	{
		rdShader_ParseSourceOperandExpression(token, op);
	}
}

void rdShader_ParseSourceOperand(char* token, rdShader_SrcOperand* op)
{
	op->reg.address = 0xFF;

	// check for a reduction
	uint8_t reduction = rdShader_ParseReduction(token);
	if (reduction)
	{
		op->reduction = reduction;
		token += 3;

		// fetch the content within the ()
		char temp[128];
		char* arg = stdString_GetEnclosedStringContents(token, temp, 128, '(', ')');
		if (arg)
		{
			rdShader_ParseSourceOperandWithModifiers(temp, op);
		}
		else
		{
			// todo: error
		}
	}
	else
	{
		rdShader_ParseSourceOperandWithModifiers(token, op);
	}
}

void rdShader_ParseInstructionModifiers(const char* modifierStart, rdShader_Instruction* inst)
{
	char buffer[128];
	strncpy(buffer, modifierStart, 128 - 1);
	buffer[128 - 1] = '\0';

	// split the modifiers by commas
	char* token = strtok(buffer, " ");
	if (!token)
		return;

	while (isspace(token[0]))
		token++;

	while (token)
	{
		uint8_t multiplier = rdShader_ParseMultiplier(token);
		if (multiplier)
			inst->dest.multiplier = multiplier;
		else if (strnicmp(token, "precise", 7) == 0)
			inst->precise = 1;
		else if (strnicmp(token, "clamp", 5) == 0)
			inst->clamp = 1;
		else if (strnicmp(token, "abs", 3) == 0)
			inst->dest.reg.abs = 1;
		else if (strnicmp(token, "negate", 6) == 0)
			inst->dest.reg.negate_or_invert = RD_SHADER_NEGATE;
		else
			inst->dest.reg.fmt = rdShader_ParseFormat(token);

		token = strtok(NULL, " ");
	}
}

int rdShader_ParseInstruction(char* line, rdShaderInstr* result)
{
	rdShader_Instruction inst;
	memset(&inst, 0, sizeof(rdShader_Instruction));

	// kill any comments
	char* commentPos = strchr(line, '#');
	if (commentPos)
	{
		// if we're not the first character, trim spaces before the comment too
		if (line != commentPos)
		{
			while (isspace(commentPos[-1]))
				--commentPos;
		}
		*commentPos = '\0';
	}

	// copy the line to a buffer for manipulation
	char buffer[512];
	strncpy(buffer, line, sizeof(buffer));
	buffer[sizeof(buffer) - 1] = '\0';

	// split the line into tokens
	char* token = strtok(buffer, " \t");
	if (!token)
		return 0;

	// parse the opcode
	inst.opcode = rdShader_ParseOpCode(token);

	if (inst.opcode == RD_SHADER_OP_FBR)
		rdShader_pCurrentAssembler->shader->hasReadback = 1;

	// parse the destination operand
	token = strtok(NULL, ",");
	if (!token)
		return 0;

	rdShader_ParseDestinationOperand(token, &inst.dest);

	// parse the source operands
	inst.srcCount = rdShader_OperandCount(inst.opcode);
	for (int i = 0; i < inst.srcCount; ++i)
	{
		token = strtok(NULL, ",");
		if (!token)
			break; // todo: error

		while (isspace(token[0]))
			++token;

		inst.src[i].reg.idx = i;
		rdShader_ParseSourceOperand(token, &inst.src[i]);
	}

	// anything else is a modifier for the instruction
	if (token && token[0] != '\0')
	{
		char* modifiers = strpbrk(token, " \t");
		if (modifiers)
			rdShader_ParseInstructionModifiers(modifiers + 1, &inst);
	}

	return rdShader_AssembleInstruction(result, &inst);
}

static void rdShader_AssembleByteCode(rdShaderByteCode* byteCode, const char* code)
{
	char tmp[512];

	const char* firstLine = strchr(code, '\n');
	if (!firstLine)
		return;

	stdString_SafeStrCopy(tmp, code, (firstLine - code) + 1);
	if (!sscanf_s(tmp, "ps.%f", &byteCode->version))
		return;

	rdShader_Assembler assembler;
	memset(&assembler, 0, sizeof(rdShader_Assembler));
	assembler.aliasHash = stdHashTable_New(16);

	rdShader_pCurrentAssembler = &assembler;

	const char* curLine = firstLine + 1;
	while (curLine)
	{
		const char* nextLine = strchr(curLine, '\n');
		int curLineLen = nextLine ? (nextLine - curLine) : strlen(curLine);
		if (curLineLen > 0)
		{
			stdString_SafeStrCopy(tmp, curLine, curLineLen + 1);

			char* ln = tmp;
			if (ln && ln[0] != '#') // early skip pure comment lines
			{
				if (strnicmp(ln, "var", 3) == 0)
					rdShader_ParseAlias(ln + 3);
				else if (rdShader_ParseInstruction(tmp, &byteCode->instructions[byteCode->instructionCount]))
					++byteCode->instructionCount;
			}
		}

		if (byteCode->instructionCount >= ARRAY_SIZE(byteCode->instructions))
			break;

		curLine = nextLine ? (nextLine + 1) : NULL;
	}

	rdShader_Alias* alias = assembler.firstAlias;
	while (alias)
	{
		rdShader_Alias* next = alias->next;
		free(alias);
		alias = next;
	}

	rdShader_pCurrentAssembler = NULL;
	stdHashTable_Free(assembler.aliasHash);
}


rdShader* rdShader_New(char* fpath)
{
	rdShader* shader;
	shader = (rdShader*)rdroid_pHS->alloc(sizeof(rdShader));
    if ( shader )
    {
		rdShader_NewEntry(shader, fpath);
    }
    
    return shader;
}

int rdShader_NewEntry(rdShader* shader, char* path)
{
    if (path)
        stdString_SafeStrCopy(shader->name, path, 0x20);
	shader->id = -1;
	shader->shaderid = std3D_GenShader();
	return 0;
}

void rdShader_Free(rdShader* shader)
{
    if (shader)
    {
		rdShader_FreeEntry(shader);
        rdroid_pHS->free(shader);
    }
}

void rdShader_FreeEntry(rdShader* shader)
{
	std3D_DeleteShader(shader->shaderid);
}

int rdShader_LoadEntry(char* fpath, rdShader* shader)
{
	stdString_SafeStrCopy(shader->name, stdFileFromPath(fpath), 0x20);

	if (!stdConffile_OpenRead(fpath))
		return 0;

	if (!stdConffile_ReadLine())
	{
		stdConffile_Close();
		return 0;
	}

	if (!sscanf_s(stdConffile_aLine, "ps.%f", &shader->byteCode.version))
	{
		stdConffile_Close();
		return 0;
	}

	rdShader_Assembler assembler;
	memset(&assembler, 0, sizeof(rdShader_Assembler));
	assembler.aliasHash = stdHashTable_New(16);
	assembler.shader = shader;

	rdShader_pCurrentAssembler = &assembler;

	while (stdConffile_ReadLine())
	{
		//stdString_SafeStrCopy(tmp, curLine, curLineLen + 1);

		char* ln = stdConffile_aLine;
		if (ln && ln[0] != '#') // early skip pure comment lines
		{
			if (strnicmp(ln, "alias", 5) == 0)
				rdShader_ParseAlias(ln + 5);
			else if (rdShader_ParseInstruction(stdConffile_aLine, &shader->byteCode.instructions[shader->byteCode.instructionCount]))
				++shader->byteCode.instructionCount;
		}

		if (shader->byteCode.instructionCount >= ARRAY_SIZE(shader->byteCode.instructions))
			break;
	}

	std3D_UploadShader(shader);

	rdShader_Alias* alias = assembler.firstAlias;
	while (alias)
	{
		rdShader_Alias* next = alias->next;
		free(alias);
		alias = next;
	}

	rdShader_pCurrentAssembler = NULL;
	stdHashTable_Free(assembler.aliasHash);

	stdConffile_Close();

	return 1;
}

#endif