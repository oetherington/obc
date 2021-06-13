#include "obc.h"

#define SYMINFO(binding, type) ((binding << 4) + (type & 0xf))
#define ELF_BASE 0x17d

enum {
	EAX,
	EBX,
	ECX,
	EDX,
	ESI,
	EDI,
	REG_COUNT,
};

static const char arithmetic_reg_table[6][6] = {
	//  eax,..  ebc,..  ecx,..  edx,..  edi,..  esi,..
	{   0xc0,   0xd8,   0xc8,   0xd0,   0xf8,   0xf0,   }, // ..,eax
	{   0xc3,   0xdb,   0xcb,   0xd3,   0xfb,   0xf3,   }, // ..,ebx
	{   0xc1,   0xd9,   0xc9,   0xd1,   0xf9,   0xf1,   }, // ..,ecx
	{   0xc2,   0xda,   0xca,   0xd2,   0xfa,   0xf2,   }, // ..,edx
	{   0xc7,   0xdf,   0xcf,   0xd7,   0xff,   0xf7,   }, // ..,edi
	{   0xc6,   0xde,   0xce,   0xd6,   0xfe,   0xf6,   }, // ..,esi
};

static const char shstrtab[] =
	"\0.data\0.text\0.shstrtab\0.symtab\0.strtab\0.rel.text";

static void start(size_t main_sym)
{
	out8_2(0x31, 0xc0);		// xor eax, eax
	out8(0xe8); outrel(main_sym);	// call main
	out8_2(0x89, 0xc3);		// mov ebx, eax
	out8(0xb8); out32(1);		// mov eax, 1
	out8_2(0xcd, 0x80);		// int 0x80
}

static size_t prolog(void)
{
	out32(0xc8);		// enter 0, 0
	return textptr - 3;
}

static void epilog(void)
{
	out8_2(0xc9, 0xc3);	// leave; ret
}

static void load_imm(int reg, long long val)
{
	static const char instrs[] = { 0xb8, 0xbb, 0xb9, 0xba, 0xbe, 0xbf, };
	out8(instrs[reg]);	// mov reg, val
	out32(val);
}

static void load_offs(int reg, int base_offset)
{
	static const char instrs[] = { 0x45, 0x5d, 0x4d, 0x55, 0x7d, 0x75, };
	const char offset = 4 * (char)base_offset;
	out8_3(0x8b, instrs[reg], offset);	// mov reg, [ebp+offset]
}

static void store_reg(int reg, int base_offset)
{
	static const char instrs[] = { 0x45, 0x5d, 0x4d, 0x55, 0x7d, 0x75, };
	const char offset = 4 * (char)base_offset;
	out8_3(0x89, instrs[reg], offset);	// mov [ebp+offset], reg
}

static void push(int reg)
{
	static const char instrs[] = { 0x50, 0x53, 0x51, 0x52, 0x57, 0x56, };
	out8(instrs[reg]);	// push reg
}

static void pop(int reg)
{
	const char instrs[] = { 0x58, 0x5b, 0x59, 0x5a, 0x5f, 0x5e, };
	out8(instrs[reg]);	// pop reg
}

static void add(int r1, int r2)
{
	out8_2(0x01, arithmetic_reg_table[r1][r2]);	// add r1, r2
}

static void sub(int r1, int r2)
{
	out8_2(0x29, arithmetic_reg_table[r1][r2]);	// sub r1, r2
}

static struct buf finish(const char *const srcfilename)
{
#define o1(k) do { data[n++] = k; } while(0)
#define o2(k) do {			\
	const unsigned k_ = k;		\
	data[n++] = k_ & 0xff;		\
	data[n++] = (k_ >> 8) & 0xff;	\
} while(0)
#define o4(k) do {			\
	const unsigned k_ = k;		\
	data[n++] = k_ & 0xff;		\
	data[n++] = (k_ >> 8) & 0xff;	\
	data[n++] = (k_ >> 16) & 0xff;	\
	data[n++] = (k_ >> 24) & 0xff;	\
} while (0)

	const size_t srcfilename_len = strlen(srcfilename);

	size_t strtab_size = 1 + srcfilename_len + 1;
	for (size_t i = 0; i < symptr; i++)
		strtab_size += symbuf[i].len + 1;

	const size_t symtab_size = 0x10 * (symptr + 3);

	unsigned char *data = malloc(0x10000000); // TODO gross
	size_t n = 0;

	// See www.etherington.xyz/elfguide.htm
	// ELF header
	o4(0x464c457f);
	o4(0x00010101);
	o4(0);
	o4(0);
	o4(0x00030001);
	o4(1);
	o4(0);
	o4(0);
	o4(0x34);
	o4(0);
	o4(0x34);
	o4(0x280000);
	o4(0x030007);

	// Index 0 section header
	o4(0);
	o4(0);
	o4(0);
	o4(0);
	o4(0);
	o4(0);
	o4(0);
	o4(0);
	o4(0);
	o4(0);

	// Data section header
	o4(0x1);
	o4(0x1);
	o4(0x3);
	o4(0);
	o4(ELF_BASE);
	o4(dataptr);
	o4(0);
	o4(0);
	o4(0x4);
	o4(0);

	// Text section header
	o4(0x7);
	o4(0x1);
	o4(0x6);
	o4(0);
	o4(ELF_BASE + dataptr);
	o4(textptr);
	o4(0);
	o4(0);
	o4(0x10);
	o4(0);

	// Shstrtab section header
	o4(0xd);
	o4(0x3);
	o4(0);
	o4(0);
	o4(0x14c);
	o4(0x31);
	o4(0);
	o4(0);
	o4(0x1);
	o4(0);

	// Symtab section header
	o4(0x17);
	o4(0x2);
	o4(0);
	o4(0);
	o4(ELF_BASE + dataptr + textptr + strtab_size);
	o4(symtab_size);
	o4(0x5);
	o4(0x3);
	o4(0x4);
	o4(0x10);

	// Strtab section header
	o4(0x1f);
	o4(0x3);
	o4(0);
	o4(0);
	o4(ELF_BASE + dataptr + textptr);
	o4(strtab_size);
	o4(0);
	o4(0);
	o4(0x1);
	o4(0);

	// Relocation section header
	o4(0x27);
	o4(0x9);
	o4(0);
	o4(0);
	o4(ELF_BASE + dataptr + textptr + strtab_size + symtab_size);
	o4(relptr * 0x8);
	o4(0x4);
	o4(0x2);
	o4(0x4);
	o4(0x8);

	// Shstrtab section
	memcpy(&data[n], shstrtab, sizeof(shstrtab));
	n += sizeof(shstrtab);

	// Data section
	memcpy(&data[n], databuf, dataptr);
	n += dataptr;

	// Text section
	memcpy(&data[n], textbuf, textptr);
	n += textptr;

	// Strtab section
	const size_t strtab_start = n;
	o1(0);
	memcpy((char *)&data[n], srcfilename, srcfilename_len);
	n += srcfilename_len;
	o1(0);
	for (size_t i = 0; i < symptr; i++) {
		symbuf[i].sym_offset = n - strtab_start;
		memcpy((char *)&data[n], symbuf[i].name, symbuf[i].len);
		n += symbuf[i].len;
		o1(0);
	}

	// Symtab section
	// Index 0 symbol
	o4(0);
	o4(0);
	o4(0);
	o1(0);
	o1(0);
	o2(0);

	// File name symbol
	o4(0x1);
	o4(0);
	o4(0);
	o1(SYMINFO(0x0, 0x4));
	o1(0);
	o2(0xfff1);

	// Text section symbol
	o4(0);
	o4(0);
	o4(0);
	o1(SYMINFO(0x0, 0x3));
	o1(0);
	o2(0x2);

	// Program symbols
	for (size_t i = 0; i < symptr; i++) {
		const char binding = symbuf[i].is_static ? 0 : 1;
		const char type = symbuf[i].is_executable ? 2 : 1;
		o4(symbuf[i].sym_offset);
		o4(symbuf[i].val_offset);
		o4(symbuf[i].size);
		o1(SYMINFO(binding, type));
		o1(0);
		o2(0x2);
	}

	// Relocations
	for (size_t i = 0; i < relptr; i++) {
		o4(relbuf[i].offset);
		o4((relbuf[i].sym << 8) + (unsigned char)2);
	}

	return (struct buf){ data, n, };

#undef o1
#undef o2
#undef o4
}

void arch_init_i386(void)
{
	arch.bits = 32;
	arch.reg_count = REG_COUNT;
	arch.rel_sym_offset = 3;
	arch.start = start;
	arch.prolog_ret_bytes = 2;
	arch.prolog = prolog;
	arch.epilog = epilog;
	arch.load_imm = load_imm;
	arch.load_offs = load_offs;
	arch.store_reg = store_reg;
	arch.push = push;
	arch.pop = pop;
	arch.add = add;
	arch.sub = sub;
	arch.finish = finish;
}
