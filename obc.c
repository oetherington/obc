#define OBC_GLOBAL_LINKAGE
#include "obc.h"
#include <ctype.h>
#include <strings.h>

#define TKN_TYPES				\
	X(TKN_INVALID,	"invalid token")	\
	X(TKN_EOF,	"end of file")		\
	X(TKN_IDENT,	"identifier")		\
	X(TKN_INT,	"int literal")		\
	X(TKN_LBRACE,	"left brace")		\
	X(TKN_RBRACE,	"right brace")		\
	X(TKN_LPAREN,	"left paren")		\
	X(TKN_RPAREN,	"right paren")		\
	X(TKN_COMMA,	"comma")		\
	X(TKN_SEMI,	"semicolon")		\
	X(TKN_EQ,	"equals sign")		\
	X(TKN_ADD,	"addition")		\
	X(TKN_SUB,	"subtract")		\
	X(TKN_MUL,	"multiply")		\
	X(TKN_DIV,	"divide")		\
	X(TKN_MOD,	"modulus")		\
	X(TKN_EXTRN,	"extrn")		\
	X(TKN_AUTO,	"auto")

enum {
#define X(e, s) e,
TKN_TYPES
#undef X
};

static const char *const tkntypestr[] = {
#define X(e, s) s,
TKN_TYPES
#undef X
};

static struct {
	int type;
	char *data;
	size_t len;
} tkn;

static int isident(const char c)
{
	return (c >= 'a'&& c <= 'z') || (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9') || c == '_' || c == '.';
}

static void check_keyword(void)
{
#define CMP(s) !strncmp(tkn.data, s, sizeof(s) - 1)

	switch (*tkn.data) {
	case 'a':
		if (CMP("auto"))
			tkn.type = TKN_AUTO;
		break;
	case 'e':
		if (CMP("extrn"))
			tkn.type = TKN_EXTRN;
		break;
	default:
		break;
	}

#undef CMP
}

static int lex(void)
{
next:
	switch (inbuf[inptr]) {
	case 0:		tkn.type = TKN_EOF;		break;
	case '{':	tkn.type = TKN_LBRACE;		break;
	case '}':	tkn.type = TKN_RBRACE;		break;
	case '(':	tkn.type = TKN_LPAREN;		break;
	case ')':	tkn.type = TKN_RPAREN;		break;
	case ',':	tkn.type = TKN_COMMA;		break;
	case ';':	tkn.type = TKN_SEMI;		break;
	case '=':	tkn.type = TKN_EQ;		break;
	case '+':	tkn.type = TKN_ADD;		break;
	case '-':	tkn.type = TKN_SUB;		break;
	case '*':	tkn.type = TKN_MUL;		break;
	case '/':	tkn.type = TKN_DIV;		break;
	case '%':	tkn.type = TKN_MOD;		break;

	case ' ': case '\t': case '\n': case '\r': case '\f':
		inptr++;
		goto next;

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
	case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
		tkn.type = TKN_IDENT;
		tkn.data = &inbuf[inptr];
		tkn.len = 1;
		while (isident(inbuf[++inptr]))
			tkn.len++;
		check_keyword();
		return tkn.type;

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		tkn.type = TKN_INT;
		tkn.data = &inbuf[inptr];
		tkn.len = 1;
		while (isdigit(inbuf[++inptr]))
			tkn.len++;
		return tkn.type;

	default:	tkn.type = TKN_INVALID;		break;
	}

	inptr++;
	return tkn.type;
}

static const char *found(void)
{
	const size_t n = 64;
	static char buf[n];

	if (tkn.len)
		snprintf(buf, n, "found '%.*s'", (int)tkn.len, tkn.data);
	else
		snprintf(buf, n, "found %s", tkntypestr[tkn.type]);

	return buf;
}

static int lexexp(int tkntype, const char *const what)
{
	if (lex() != tkntype)
		fatal("Expected %s but %s", what, found());
	return tkn.type;
}

static void emit_start(size_t main_sym)
{
	entrypoint = textptr;

	struct sym *sym = &symbuf[symptr++];
	memset(sym, 0, sizeof(struct sym));
	sym->name = "_start";
	sym->len = 6;
	sym->is_executable = 1;
	sym->val_offset = entrypoint;

	arch.start(main_sym);

	sym->size = textptr - sym->val_offset;
}

static int lookup(const struct scope *const s)
{
	for (size_t i = 0; i < s->len; i++)
		if (s->vars[i].len == tkn.len &&
				!strncmp(s->vars[i].name, tkn.data, tkn.len))
			return s->vars[i].offset;

	return 0;
}

static int first_empty(int used)
{
	const int index = ffs(~used) - 1;
	return index < arch.reg_count ? index : -1;
}

static int expr(const struct scope *const s, int reg, int used);

static int primary_expr(const struct scope *const s, int reg, int used)
{
	switch (tkn.type) {
	case TKN_INT:
		// TODO: Constant folding
		arch.load_imm(reg, strtoll(tkn.data, NULL, 10));
		lex();
		return reg;

	case TKN_IDENT: {
		const int offset = lookup(s);
		if (!offset)
			fatal("'%.*s' is not in scope", (int)tkn.len, tkn.data);
		arch.load_offs(reg, offset);
		lex();
		return reg;
	}

	default:
		return -1;
	}
}

static int add_expr(const struct scope *const s, int reg, int used)
{
	const int lhs = primary_expr(s, reg, used);
	if (lhs < 0)
		fatal("Expected an expression but %s", found());

	while (tkn.type == TKN_ADD || tkn.type == TKN_SUB) {
		const int is_add = tkn.type == TKN_ADD;

		lex();

		const int inner_used = used | (1 << reg);

		int pushed = 0;
		int target = first_empty(inner_used);
		if (target < 0) {
			target = (reg + 1) % arch.reg_count;
			pushed = 1;
			arch.push(target);
		}

		const int rhs = primary_expr(s, target, inner_used);
		if (lhs < 0)
			fatal("Expected an expression after op, %s", found());

		if (is_add)
			arch.add(lhs, rhs);
		else
			arch.sub(lhs, rhs);

		if (pushed)
			arch.pop(target);
	}

	return lhs;
}

static int assign_expr(const struct scope *const s, int reg, int used)
{
	if (tkn.type == TKN_IDENT) {
		const int offset = lookup(s);
		if (!offset)
			fatal("'%.*s' is not in scope", (int)tkn.len, tkn.data);

		lexexp(TKN_EQ, "'=' in assignment");
		lex();

		const int value_reg = add_expr(s, reg, used);
		if (value_reg < 0)
			fatal("Expected expression in assignment, %s", found());

		arch.store_reg(value_reg, offset);
		return value_reg;
	}

	return add_expr(s, reg, used);
}

static int expr(const struct scope *const s, int reg, int used)
{
	return assign_expr(s, reg, used);
}

static int stmt(const struct scope *const s)
{
	const int reg = expr(s, 0, 0);
	if (reg >= 0) {
		if (tkn.type != TKN_SEMI)
			fatal("Expected ';' after statement but %s", found());
		lex();
		return 1;
	}

	return 0;
}

static struct scope populate_scope(void)
{
	struct scope scope;
	scope.vars = malloc(sizeof(struct var) * MAX_SCOPE_LEN);
	scope.len = 0;
	scope.local_count = 0;

	while (tkn.type == TKN_AUTO || tkn.type == TKN_EXTRN) {
		const int is_auto = tkn.type == TKN_AUTO;

		while (1) {
			lexexp(TKN_IDENT, "identifier for var declaration");

			if (scope.len >= MAX_SCOPE_LEN)
				fatal("Too many locals");

			scope.vars[scope.len] = (struct var){
				.name = tkn.data,
				.len = tkn.len,
				.is_extern = tkn.type == TKN_EXTRN,
				.offset = -(scope.len + 1), //TODO: Handle extrn
			};

			scope.len++;

			if (is_auto)
				scope.local_count++;

			if (lex() != TKN_COMMA)
				break;
		}

		if (tkn.type != TKN_SEMI)
			fatal("Expected ';' after var decl but %s", found());

		lex();
	}

	return scope;
}

static int compile_decl(void)
{
	if (tkn.type != TKN_IDENT)
		return err("Expected a declaration but %s", found());

	struct sym *sym = &symbuf[symptr++];
	memset(sym, 0, sizeof(struct sym));
	sym->name = tkn.data;
	sym->len = tkn.len;
	sym->is_executable = 1;
	sym->val_offset = textptr;

	if (lex() != TKN_LPAREN)
		return err("Expected '(', %s", found());

	if (lex() != TKN_RPAREN)
		return err("Expected ')', %s", found());

	if (lex() != TKN_LBRACE)
		return err("Expected '{', %s", found());

	lex();

	const size_t local_count_offset = arch.prolog();
	const struct scope scope = populate_scope();

	while (tkn.type != TKN_RBRACE)
		stmt(&scope);

	arch.epilog();

	out_at(local_count_offset, arch.prolog_ret_bytes, scope.local_count);

	sym->size = textptr - sym->val_offset;

	if (!strncmp(sym->name, "main", sym->len))
		emit_start(symptr - 1);

	free(scope.vars);

	return 0;
}

int main(int argc, char **argv)
{
	arch_init_i386();

	const char path[] = "examples/example1.b";

	if (read(path))
		return 1;

	textbuf = malloc(0x100000);	// TODO - gross
	textptr = 0;

	databuf = malloc(0x100000);	// TODO - gross
	dataptr = 0;

	symbuf = malloc(0x100000);	// TODO - gross
	symptr = 0;

	relbuf = malloc(0x100000);	// TODO - gross
	relptr = 0;

	entrypoint = 0;

	lex();
	compile_decl();

	for (size_t i = 0; i < textptr; i++)
		printf("%.2x ", textbuf[i] & 0xff);
	printf("\n\n");

	const char *const fname = strrchr(path, '/');
	struct buf result = arch.finish(fname ? fname + 1 : path);

	free(textbuf);
	free(databuf);
	free(symbuf);
	free(relbuf);
	free(inbuf);

	/*
	for (size_t i = 0; i < result.len; i++) {
		printf("%.2x ", result.data[i] & 0xff);
		if ((i + 1) % 8 == 0)
			printf("\n");
	}
	printf("\n%ld bytes\n\n", result.len);
	*/

	FILE *f = fopen("out.o", "w");
	if (!f)
		return err("Can't open output file for writing");
	fwrite(result.data, 1, result.len, f);
	fclose(f);

	free(result.data);

	return 0;
}
