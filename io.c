#include "obc.h"
#include <stdarg.h>

static void verr(const char *const fmt, va_list args)
{
	fputs("\e[1;31mError: \e[0m", stderr);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
}

int err(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	verr(fmt, args);
	va_end(args);
	return 1;
}

void fatal(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	verr(fmt, args);
	va_end(args);
	exit(1);
}

int read(const char *const path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return err("Can't open file '%s'", path);

	fseek(f, 0, SEEK_END);
	const long length = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (!(inbuf = malloc(length + 1))) {
		fclose(f);
		return err("Can't allocate input buffer");
	}

	fread(inbuf, 1, length, f);
	fclose(f);

	inptr = 0;

	return 0;
}

void unaligned_write32(char *p, int n)
{
	p[0] = (char)n;
	p[1] = (char)(n >> 8);
	p[2] = (char)(n >> 16);
	p[3] = (char)(n >> 24);
}

void out32(int n)
{
	unaligned_write32(&textbuf[textptr], n);
	textptr += 4;
}

void out8(char a)
{
	textbuf[textptr++] = a;
}

void out8_2(char a, char b)
{
	textbuf[textptr    ] = a;
	textbuf[textptr + 1] = b;
	textptr += 2;
}

void out8_3(char a, char b, char c)
{
	textbuf[textptr    ] = a;
	textbuf[textptr + 1] = b;
	textbuf[textptr + 2] = c;
	textptr += 3;
}

void out_at(size_t offset, size_t bytes, int n)
{
	switch (bytes) {
	case 4:	textbuf[offset + 3] = (char)(n >> 24);
	case 3:	textbuf[offset + 2] = (char)(n >> 16);
	case 2:	textbuf[offset + 1] = (char)(n >> 8);
	case 1:	textbuf[offset    ] = (char)n;
	}
}

struct sym *pushsym(struct sym s)
{
	struct sym *p = &symbuf[symptr];
	symbuf[symptr++] = s;
	return p;
}

struct rel *pushrel(struct rel r)
{
	struct rel *p = &relbuf[relptr];
	relbuf[relptr++] = r;
	return p;
}

void outrel(size_t sym)
{
	sym += arch.rel_sym_offset;

	const struct rel r = (struct rel){ textptr, sym };

	switch (arch.bits) {
	case 64:
		out32(0);	// TODO
	case 32:
		out32(-sym - 1);	// TODO: Is this really correct?
		pushrel(r);
		break;
	default:
		fatal("Invalid arch in outrel");
	}
}
