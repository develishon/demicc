/* FILE: demicc.c - DemiC compiler that outputs Static Little Endian x86-64 ELF Executables */
/* BUILD: cc -o demicc demicc.c */
/* USAGE: demicc <output> <input> */
/* VERSION: 1.2.1 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef signed int s32;
typedef int c89_static_assert_1[sizeof(s32) == 4 ? 1 : -1];
typedef unsigned long u64;
typedef int c89_static_assert_2[sizeof(u64) == 8 ? 1 : -1];

typedef struct Sym {
  char *name;
  s32 local; /* 0 means it is global not local */
  s32 global; /* 0 means undefined */
  s32 toprela; /* a linked list of relocations in exebuff */
  s32 frame_size;
} Sym;

static char  srcbuff[0x1000000]; /* oh noooo! globalization is a threat to our local culture */
static char *cp = srcbuff;
static char  exebuff[0x1000000];
static s32   exesize;
static char *currtok;
static char *prevtok;
static Sym   symtab[0x10000]; /* the first symbol is "null" */
static Sym  *topsym = symtab;

static char *optable[] = { /* FEATURE: binary operators: = -= += &= |= ^= *= /= %= <<= >>= <= >= == != < > - + & | ^ * / % << >> */
  "\xC\3\2", "<<=", "\x59\x5b\x48\x8b\x03\x48\xd3\xe0\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; shl %c, %a; mov %a, (%b); push %a; */
  "\xC\3\2", ">>=", "\x59\x5b\x48\x8b\x03\x48\xd3\xe8\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; shr %c, %a; mov %a, (%b); push %a; */
  "\x6\6\7", "<<",  "\x59\x58\x48\xd3\xe0\x50", /* ASM: pop %rcx; pop %rax; shl %cl, %rax; push %rax; */
  "\x6\6\7", ">>",  "\x59\x58\x48\xd3\xe8\x50", /* ASM: pop %rcx; pop %rax; shr %cl, %rax; push %rax; */
  "\xE\4\5", "<=",  "\x59\x58\x48\x31\xd2\x48\x39\xc8\x7f\x03\x48\xff\xc2\x52", /*ASM: pop %c; pop %a; xor %d, %d; cmp %c, %a; jg  +3; inc %d; push %d;*/
  "\xE\4\5", ">=",  "\x59\x58\x48\x31\xd2\x48\x39\xc8\x7c\x03\x48\xff\xc2\x52", /*ASM: pop %c; pop %a; xor %d, %d; cmp %c, %a; jl  +3; inc %d; push %d;*/
  "\xE\4\5", "==",  "\x59\x58\x48\x31\xd2\x48\x39\xc8\x75\x03\x48\xff\xc2\x52", /*ASM: pop %c; pop %a; xor %d, %d; cmp %c, %a; jne +3; inc %d; push %d;*/
  "\xE\4\5", "!=",  "\x59\x58\x48\x31\xd2\x48\x39\xc8\x74\x03\x48\xff\xc2\x52", /*ASM: pop %c; pop %a; xor %d, %d; cmp %c, %a; je  +3; inc %d; push %d;*/
  "\xC\3\2", "+=",  "\x59\x5b\x48\x8b\x03\x48\x01\xc8\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; add %c, %a; mov %a, (%b); push %a; */
  "\xC\3\2", "-=",  "\x59\x5b\x48\x8b\x03\x48\x29\xc8\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; sub %c, %a; mov %a, (%b); push %a; */
  "\xC\3\2", "&=",  "\x59\x5b\x48\x8b\x03\x48\x21\xc8\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; and %c, %a; mov %a, (%b); push %a; */
  "\xC\3\2", "|=",  "\x59\x5b\x48\x8b\x03\x48\x09\xc8\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; or  %c, %a; mov %a, (%b); push %a; */
  "\xC\3\2", "^=",  "\x59\x5b\x48\x8b\x03\x48\x31\xc8\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; xor %c, %a; mov %a, (%b); push %a; */
  "\xC\3\2", "*=",  "\x59\x5b\x48\x8b\x03\x48\xf7\xe1\x48\x89\x03\x50", /* ASM: pop %c; pop %b; mov (%b), %a; mul %c;     mov %a, (%b); push %a; */
  "\xE\3\2", "/=",  "\x59\x5b\x48\x8b\x03\x48\x99\x48\xf7\xf9\x48\x89\x03\x50", /*ASM: pop %c; pop %b; mov (%b),%a; cqto; idiv %c; mov %a,(%b); push %a;*/
  "\xE\3\2", "%=",  "\x59\x5b\x48\x8b\x03\x48\x99\x48\xf7\xf9\x48\x89\x13\x52", /*ASM: pop %c; pop %b; mov (%b),%a; cqto; idiv %c; mov %d,(%b); push %d;*/
  "\xE\4\5", "<",   "\x59\x58\x48\x31\xd2\x48\x39\xc8\x7d\x03\x48\xff\xc2\x52", /*ASM: pop %c; pop %a; xor %d, %d; cmp %c, %a; jge +3; inc %d; push %d;*/
  "\xE\4\5", ">",   "\x59\x58\x48\x31\xd2\x48\x39\xc8\x7e\x03\x48\xff\xc2\x52", /*ASM: pop %c; pop %a; xor %d, %d; cmp %c, %a; jle +3; inc %d; push %d;*/
  "\x6\6\7", "-",   "\x59\x58\x48\x29\xc8\x50", /* ASM: pop %rcx; pop %rax; sub %rcx, %rax; push %rax; */
  "\x6\6\7", "+",   "\x59\x58\x48\x01\xc8\x50", /* ASM: pop %rcx; pop %rax; add %rcx, %rax; push %rax; */
  "\x6\6\7", "&",   "\x59\x58\x48\x21\xc8\x50", /* ASM: pop %rcx; pop %rax; and %rcx, %rax; push %rax; */
  "\x6\6\7", "|",   "\x59\x58\x48\x09\xc8\x50", /* ASM: pop %rcx; pop %rax; or  %rcx, %rax; push %rax; */
  "\x6\6\7", "^",   "\x59\x58\x48\x31\xc8\x50", /* ASM: pop %rcx; pop %rax; xor %rcx, %rax; push %rax; */
  "\x6\6\7", "*",   "\x59\x58\x48\xf7\xe1\x50", /* ASM: pop %rcx; pop %rax; mul %rcx;       push %rax; */
  "\x8\6\7", "/",   "\x59\x58\x48\x99\x48\xf7\xf9\x50", /* ASM: pop %rcx; pop %rax; cqto; idiv %rcx; push %rax; */
  "\x8\6\7", "%",   "\x59\x58\x48\x99\x48\xf7\xf9\x52", /* ASM: pop %rcx; pop %rax; cqto; idiv %rcx; push %rdx; */
  "\x6\3\2", "=",   "\x59\x58\x48\x89\x08\x51", /* ASM: pop %rcx; pop %rax; mov %rcx, (%rax); push %rcx; */
  NULL /* NOTE: it goes like: length, level to test, level to pass, token, and instructions */
};

static void die_if(s32 condition, char *message)
{
  if (condition)
  {
    fprintf(stderr, "error: %s\ncontext: '%s' '%s'\n", message, prevtok, currtok);
    exit(1);
  }
}

static s32 emit_bytes(s32 size, void *data)
{
  exesize += size;
  die_if(exesize > (s32)sizeof(exebuff), "out of buffer");
  memcpy(exebuff + (exesize - size), data, size);
  return (exesize - size);
}

static char *scan_if(s32 condition, char *optional_death_message_if_false)
{
  char *start = NULL;
  int i = 0;
  
  if (!condition)
  {
    die_if(optional_death_message_if_false != NULL, optional_death_message_if_false);
    return NULL;
  }
  
skip_spaces_and_comments:

  while (isspace(*cp))
  {
    ++cp;
  }
  
  if (cp[0] == '/' && cp[1] == '/')
  {
    while (*cp != '\0' && *cp != '\n')
    {
      ++cp;
    }
    
    goto skip_spaces_and_comments;
  }
  
  start = cp;

  while (isalnum(*cp) || *cp == '_')
  {
    ++cp;
  }

  if (start[0] == '\'' || start[0] == '\"')
  {
    for (++cp; *cp != '\0' && !(*cp == start[0]); /**/)
    {
      ++cp;
    }
    
    die_if(*cp == '\0', "missing \' or \" at the end");
    ++cp;
  }

  for (i = 0; optable[i] != NULL; i += 3)
  {
    if (memcmp(start, optable[i + 1], strlen(optable[i + 1])) == 0)
    {
      cp += strlen(optable[i + 1]);
      break;
    }
  }
  
  cp += (start[0] != '\0' && cp == start) ? 1 : 0;
  
  prevtok = currtok;
  currtok = calloc(cp - start + 1, 1);
  die_if(currtok == NULL, "out of memory");
  memcpy(currtok, start, cp - start);
  return prevtok;
}

static s32 rvalue_from(s32 value)
{
  if (value == 8) /* lvalue of u64/s64 */
  {
    emit_bytes(5, "\x58\x48\x8b\x00\x50"); /* ASM: pop %rax; mov (%rax), %rax; push %rax; */
  }
  
  return 0;
}

static Sym *sym_declare(char *name, s32 define)
{
  Sym *sym = NULL;
  
  for (sym = topsym; sym != symtab; --sym)
  {
    if (strcmp(sym->name, name) == 0)
    {
      die_if(define && (sym->local || sym->global), "name collision"); /* scopes don't allow it anyway so might as well be an error */
      return sym;
    }
  }
  
  topsym += 1;
  die_if((char *)topsym == (char *)symtab + sizeof(symtab), "out of symbol table");
  memset(topsym, 0, sizeof(*topsym));
  topsym->name = name;
  return topsym;
}

static u64 u64_from_string(s32 length, char *s, u64 base) /* base 10 means auto-detect */
{
  u64 value = 0;
  s32 i = 0;
  
  if (base == 10 && length >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'b'))
  {
    return u64_from_string(length - 2, s + 2, (s[1] == 'x') ? 16 : 2);
  }
  
  for (i = 0; i < length; ++i)
  {
    die_if(!isxdigit(s[i]), "invalid integer constant or escape sequence");
    value = (value * base) + (isdigit(s[i]) ? s[i] - '0' : isupper(s[i]) ? s[i] - 'A' + 10 : s[i] - 'a' + 10);
  }
  
  return value;
}

static s32 compile_expression(s32 level)
{
  Sym *sym = NULL;
  u64 constant = 0;
  s32 value = 0;
  s32 off1 = 0;
  s32 i = 0;
  s32 count = 0;
  s32 length = 0;
  unsigned char byte = 0;
  
  if (scan_if(isdigit(currtok[0]), NULL)) /* FEATURE: integer constant */
  {
    off1 = 2 + emit_bytes(11, "\x48\xb8\0\0\0\0\0\0\0\0\x50"); /* ASM: movabs $0, %rax; push %rax; */
    *(u64 *)(exebuff + off1) = u64_from_string(strlen(prevtok), prevtok, 10);
  }
  else if (scan_if(currtok[0] == '\'' || currtok[0] == '\"', NULL)) /* FEATURE: textual constants */
  {
    off1 = 1 + emit_bytes(5, "\xe9\0\0\0\0"); /* ASM: jmp <rel32> */
    length = strlen(prevtok) - 1;

    for (i = 1; i < length; ++i)
    {
      byte = prevtok[i];
    
      if (byte == '\\')
      {
        count = isxdigit(prevtok[i + 2]) ? 2 : 1;
        byte = u64_from_string(count, prevtok + i + 1, 16);
        i += count;
      }
      
      emit_bytes(1, &byte);
      constant = (constant << 8) + byte;
    }
    
    emit_bytes(1, "\0");
    *(s32 *)(exebuff + off1) = exesize - off1 - 4;
    i = 2 + emit_bytes(11, "\x48\xb8\0\0\0\0\0\0\0\0\x50"); /* ASM: movabs $0, %rax; push %rax; */
    *(u64 *)(exebuff + i) = (prevtok[0] == '\"') ? 0x400000 + (u64)off1 + 4 : constant;
  }
  else if (scan_if(strcmp(currtok, "return") == 0, NULL)) /* FEATURE: return statement */
  {
    value = rvalue_from(compile_expression(0)); /* technically that makes it a prefix operator */
    emit_bytes(3, "\x58\xc9\xc3"); /* ASM: pop rax; leave; ret; */
  }
  else if (scan_if(isalpha(currtok[0]) || currtok[0] == '_', NULL)) /* FEATURE: get symbol value */
  {
    sym = sym_declare(prevtok, 0);
    value = 8;
  
    if (sym->local)
    {
      off1 = 3 + emit_bytes(8, "\x48\x8d\x85\0\0\0\0\x50"); /* ASM: lea 0(%rbp), %rax; push %rax; */
      *(s32 *)(exebuff + off1) = sym->local;
    }
    else /* FEATURE: implicit declaration */
    {
      off1 = 3 + emit_bytes(8, "\x48\x8d\x05\0\0\0\0\x50"); /* ASM: lea 0(%rip), %rax; push %rax; */
      *(s32 *)(exebuff + off1) = sym->toprela; /* save the old top rela */
      sym->toprela = off1; /* the new top rela points to the old one forming a linked list */
    }
  }
  else if (scan_if(strcmp(currtok, "&") == 0, NULL)) /* FEATURE: pointer to value */
  {
    value = compile_expression(3);
    die_if(value == 0, "prefix '&' operator requires an lvalue");
    value = 0;
  }
  else if (scan_if(strcmp(currtok, "*") == 0, NULL)) /* FEATURE: value by pointer */
  {
    rvalue_from(compile_expression(3));
    value = 8;
  }
  else /* FEATURE: sub-expression */
  {
    scan_if(strcmp(currtok, "(") == 0, "unknown primary expression");
    value = compile_expression(0);
    scan_if(strcmp(currtok, ")") == 0, "missing ')'");
  }
  
  for (count = 0; scan_if(strcmp(currtok, "(") == 0, NULL); count = 0) /* FEATURE: function call */
  {
    if (strcmp(currtok, ")") != 0)
    {
      do
      {
        rvalue_from(compile_expression(0));
        count += 1;
      }
      while (scan_if(strcmp(currtok, ",") == 0, NULL));
    }
    
    scan_if(strcmp(currtok, ")") == 0, "missing ')'");
    
    for (i = 0; i < count + 1; ++i) /* repush the arguments and the function pointer in the correct order */
    {
      off1 = 3 + emit_bytes(7, "\xff\xb4\x24\0\0\0\0"); /* ASM: push 0(%rsp); */
      *(s32 *)(exebuff + off1) = 2 * (i * 8);
    }
    
    value = rvalue_from(value);
    emit_bytes(3, "\x58\xff\xd0"); /* ASM: pop %rax; call *%rax; */
    off1 = 3 + emit_bytes(8, "\x48\x81\xc4\0\0\0\0\x50"); /* ASM: add $0, %rsp; push rax; */
    *(s32 *)(exebuff + off1) = 2 * (count * 8) + 8; /* remove the arguments and the function pointer */
  }
  
binary_operators_left_to_right:

  for (i = 0; optable[i] != NULL; i += 3)
  {
    if (optable[i][1] > level && scan_if(strcmp(currtok, optable[i + 1]) == 0, NULL))
    {
      die_if(optable[i][1] == 3 && value == 0, "need an lvalue on the left"); /* only level 3 operators need an lvalue */
      rvalue_from((optable[i][1] == 3) ? 0 : value); /* only level 3 operators don't need an rvalue */
      value = rvalue_from(compile_expression(optable[i][2]));
      emit_bytes(optable[i][0], optable[i + 2]);
      
      goto binary_operators_left_to_right;
    }
  }
  
  return value;
}

static void compile_statement(Sym *func)
{
  Sym *sym = NULL;
  Sym *base_sym = NULL;
  s32 off1 = 0;
  s32 off2 = 0;

  base_sym = topsym; /* FEATURE: limit the scope for local variables and function parameters */

  if (scan_if(strcmp(currtok, "{") == 0, NULL)) /* FEATURE: statement list */
  {
    while (currtok[0] != '\0' && strcmp(currtok, "}") != 0)
    {
      compile_statement(func);
    }
  
    scan_if(strcmp(currtok, "}") == 0, "missing '}'");
  }
  else if (scan_if(strcmp(currtok, "if") == 0 || strcmp(currtok, "while") == 0, NULL)) /* FEATURE: if & while */
  {
    off2 = (strcmp(prevtok, "while") == 0) ? exesize : 0;
    rvalue_from(compile_expression(0));
    off1 = 6 + emit_bytes(10, "\x58\x48\x85\xc0\x0f\x84\0\0\0\0"); /* ASM: pop %rax; test %rax, %rax; jz <rel32>; */
    compile_statement(func);
    *(s32 *)(exebuff + off1) = (exesize + 5) - off1 - 4; /* break or jump over the true-branch */
    off1 = 1 + emit_bytes(5, "\xe9\0\0\0\0"); /* ASM: jmp <rel32>; */
    *(s32 *)(exebuff + off1) = (off2 != 0) ? off2 - off1 - 4 : 0; /* loop or leave zero in case of an else branch */
    
    if (off2 == 0 && scan_if(strcmp(currtok, "else") == 0, NULL)) /* FEATURE: else */
    {
      compile_statement(func);
      *(s32 *)(exebuff + off1) = exesize - off1 - 4; /* jump over the else-branch */
    }
  }
  else if (scan_if(strcmp(currtok, "int") == 0, NULL)) /* FEATURE: symbol declaration */
  {
    scan_if(isalpha(currtok[0]) || currtok[0] == '_', "expected a name after int");
    base_sym = sym_declare(prevtok, 1);
    
    if (func == NULL) /* global variable OR function */
    {
      emit_bytes((exesize % 8 != 0) ? 8 - exesize % 8 : 0, "\0\0\0\0\0\0\0\0"); /* align 8 */
      base_sym->global = emit_bytes(8, "\0\0\0\0\0\0\0\0"); /* FEATURE: global variables */
      
      if (scan_if(strcmp(currtok, "(") == 0, NULL)) /* FEATURE: function definition */
      {
        if (strcmp(currtok, ")") != 0) /* FEATURE: function parameters */
        {
          do
          {
            scan_if(strcmp(currtok, "int") == 0, "expected a type before argument name");
            scan_if(isalpha(currtok[0]) || currtok[0] == '_', "expected the name of the argument");
            sym_declare(prevtok, 1)->local = 16 + off2; /* 16 skips rbp and the return address */
            off2 += 8; /* the offset for the next one */
          }
          while (scan_if(strcmp(currtok, ",") == 0, NULL));
        }
        
        scan_if(strcmp(currtok, ")") == 0, "expected ')'");
        *(u64 *)(exebuff + base_sym->global) = 0x00400000 + exesize; /* store absolute address */
        off1 = 7 + emit_bytes(11, "\x55\x48\x89\xe5\x48\x81\xec\0\0\0\0"); /* ASM: push %rbp; mov %rsp, %rbp; sub $0, %rsp; */
        compile_statement(base_sym);
        emit_bytes(5, "\x48\x31\xc0\xc9\xc3"); /* ASM: xor %rax, %rax; leave; ret; */
        *(s32 *)(exebuff + off1) = base_sym->frame_size;
      }
    }
    else
    {
      func->frame_size += 8;
      base_sym->local = -(func->frame_size); /* FEATURE: local variables */
    }
    
    scan_if(currtok[0] == ';', (prevtok[0] != '}' && prevtok[0] != ';') ? "missing ';'" : NULL);
  }
  else /* FEATURE: expression statement */
  {
    rvalue_from(compile_expression(0));
    emit_bytes(1, "\x58"); /* ASM: pop rax; */
    scan_if(strcmp(currtok, ";") == 0, "missing ';'");
  }
  
  for (sym = topsym; sym != base_sym; --sym)
  {
    if (sym->local)
    {
      *sym = *topsym;
      topsym -= 1;
    }
  }
}

int main(int argc, char *argv[])
{
  FILE *stream = NULL;
  Sym *sym = NULL;
  s32 offset = 0;
  
  die_if(argc != 3, "need output then input as command line arguments");
  stream = fopen(argv[2], "rb");
  die_if(stream == NULL, "no such file");
  fread(srcbuff, 1, sizeof(srcbuff) - 1, stream); /* who needs fclose anyway... */

  emit_bytes(13 * 16, /* Elf64_Ehdr and Elf64_Phdr are the same every time except for p_filesz and p_memsz */
    "\x7F\x45\x4C\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* ELF64 LE SystemV */
    "\x02\x00\x3E\x00\x01\x00\x00\x00\x78\x00\x40\x00\x00\x00\x00\x00" /* EXEC x86-64 */
    "\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x40\x00\x38\x00\x01\x00\x00\x00\x00\x00\x00\x00"
    "\x01\x00\x00\x00\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* LOAD RWX at 0x00400000 */
    "\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\xC5\x00\x00\x00\x00\x00\x00\x00\xC5\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x10\x00\x00\x00\x00\x00\x00\x48\x31\xed\x48\x8d\x44\x24\x08"   /* 0x78 _start */
    "\x50\xff\x74\x24\x08\x48\x8b\x05\x00\x00\x00\x00\xff\xd0\x48\x31"
    "\xff\xb8\x3c\x00\x00\x00\x0f\x05\xcc\x48\x89\xe3\x48\x8b\x43\x08"   /* 0x99 syscall */
    "\x48\x8b\x7b\x10\x48\x8b\x73\x18\x48\x8b\x53\x20\x4c\x8b\x53\x28"
    "\x4c\x8b\x43\x30\x4c\x8b\x4b\x38\x0f\x05\xc3\x48\x8b\x44\x24\x08"   /* 0xBB ld_u8 */
    "\x0f\xb6\x00\xc3\x48\x8b\x44\x24\x08\x8b\x4c\x24\x10\x88\x08\xc3"); /* 0xC4 st_u8 */
  /* ASM: _start: xor %rbp, %rbp; lea 8(%rsp), %rax; push %rax; push 8(%rsp); mov 0x0(%rip), %rax; call *%rax;
    xor %rdi, %rdi; mov $60, %eax; syscall; int3; syscall: mov %rsp, %rbx; mov 8(%rbx), %rax; mov 16(%rbx), %rdi;
    mov 24(%rbx), %rsi; mov 32(%rbx), %rdx; mov 40(%rbx), %r10; mov 48(%rbx), %r8; mov 56(%rbx), %r9; syscall; ret;
    ld_u8: mov 8(%rsp), %rax; movzb (%rax), %eax; ret; st_u8: mov 8(%rsp), %rax; mov 16(%rsp), %ecx; mov %cl, (%rax); ret; */
  
  sym_declare("syscall", 1)->global = emit_bytes(8, "\x99\x00\x40\x00\x00\x00\x00\x00"); /* YES! hardcoded addresses */
  sym_declare("ld_u8"  , 1)->global = emit_bytes(8, "\xBB\x00\x40\x00\x00\x00\x00\x00");
  sym_declare("st_u8"  , 1)->global = emit_bytes(8, "\xC4\x00\x40\x00\x00\x00\x00\x00");
  
  sym_declare("main", 0)->toprela = 0x88; /* FEATURE: user defines "int main(int argc, int argv)" as the entry point */
  
  scan_if(1, NULL); /* setup the lexer */
  
  while (currtok[0] != '\0')
  {
    compile_statement(NULL);
  }
  
  for (sym = topsym; sym != symtab; --sym)
  {
    die_if(sym->global == 0, "undefined symbol");
    
    while (sym->toprela != 0)
    {
      offset = sym->toprela;
      sym->toprela = *(s32 *)(exebuff + offset);
      *(s32 *)(exebuff + offset) = sym->global - offset - 4;
    }
  }
  
  *(s32 *)(exebuff + 64 + 32) = exesize; /* p_filesz */
  *(s32 *)(exebuff + 64 + 40) = exesize; /* p_memsz */
  
  stream = fopen(argv[1], "wb");
  die_if(stream == NULL, "fopen");
  fwrite(exebuff, 1, exesize, stream);
  return 0;
}


