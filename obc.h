#ifndef OBC_H
#define OBC_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SCOPE_LEN 64

struct buf {
	unsigned char *data;
	size_t len;
};

struct sym {
	char *name;
	size_t len;
	int is_static;
	int is_executable;
	size_t size;
	size_t val_offset;
	size_t sym_offset;
};

enum {
	REL_DATA,
	REL_TEXT,
};

struct rel {
	size_t offset;
	size_t sym;
};

struct var {
	char *name;
	size_t len;
	int is_extern;
	int offset;
};

struct scope {
	struct var *vars;
	size_t len;
	size_t local_count;
};

struct arch {
	int bits;
	int reg_count;
	int rel_sym_offset;
	void (*start)(size_t main_sym);
	size_t prolog_ret_bytes;
	size_t (*prolog)(void);
	void (*epilog)(void);
	void (*load_imm)(int reg, long long val);
	void (*store_reg)(int reg, int base_offset);
	void (*push)(int reg);
	void (*pop)(int reg);
	void (*add)(int r1, int r2);
	void (*sub)(int r1, int r2);
	struct buf (*finish)(const char *const srcfilename);
};

void arch_init_i386(void);

__attribute__((__format__ (__printf__, 1, 2)))
int err(const char *const fmt, ...);
__attribute__((__format__ (__printf__, 1, 2)))
void fatal(const char *const fmt, ...);
int read(const char *const path);
void unaligned_write32(char *p, int n);
void out32(int n);
void out8(char a);
void out8_2(char a, char b);
void out8_3(char a, char b, char c);
void out_at(size_t offset, size_t bytes, int n);
struct sym *pushsym(struct sym s);
struct rel *pushrel(struct rel r);
void outrel(size_t sym);

#ifndef OBC_GLOBAL_LINKAGE
#define OBC_GLOBAL_LINKAGE extern
#endif

OBC_GLOBAL_LINKAGE struct sym *symbuf;
OBC_GLOBAL_LINKAGE struct rel *relbuf;
OBC_GLOBAL_LINKAGE char *inbuf, *textbuf, *databuf;
OBC_GLOBAL_LINKAGE size_t symptr, relptr, inptr, textptr, dataptr, entrypoint;
OBC_GLOBAL_LINKAGE struct arch arch;

#endif
