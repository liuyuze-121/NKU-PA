#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum {
  TK_NOTYPE = 256,
  TK_NUMBER,
  TK_HEX,
  TK_REG,
  TK_EQ,
  TK_NEQ,
  TK_AND,
  TK_OR,
  TK_NEGATIVE,
  TK_DEREF
};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE},
  {"0x[0-9A-Fa-f]+", TK_HEX},
  {"[0-9]+", TK_NUMBER},
  {"\\$eip", TK_REG},
  {"\\$eax", TK_REG},
  {"\\$ecx", TK_REG},
  {"\\$edx", TK_REG},
  {"\\$ebx", TK_REG},
  {"\\$esp", TK_REG},
  {"\\$ebp", TK_REG},
  {"\\$esi", TK_REG},
  {"\\$edi", TK_REG},
  {"==", TK_EQ},
  {"!=", TK_NEQ},
  {"&&", TK_AND},
  {"\\|\\|", TK_OR},
  {"!", '!'},
  {"\\+", '+'},
  {"-", '-'},
  {"\\*", '*'},
  {"\\/", '/'},
  {"\\(", '('},
  {"\\)", ')'}
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))
static regex_t re[NR_REGEX];

void init_regex(void) {
  for (int i = 0; i < NR_REGEX; i++) {
    char errbuf[128];
    int ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) { regerror(ret, &re[i], errbuf, 128); panic("%s", errbuf); }
  }
}

typedef struct token { int type; char str[32]; } Token;
Token tokens[32];
int nr_token;

static bool make_token(char *e) {
  int pos = 0;
  regmatch_t pm;
  nr_token = 0;
  while (e[pos] != '\0') {
    int i;
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + pos, 1, &pm, 0) == 0 && pm.rm_so == 0) {
        int len = pm.rm_eo;
        if (rules[i].token_type == TK_NOTYPE) { pos += len; break; }
        tokens[nr_token].type = rules[i].token_type;
        strncpy(tokens[nr_token].str, e + pos, len);
        tokens[nr_token].str[len] = '\0';
        nr_token++;
        pos += len;
        break;
      }
    }
    if (i == NR_REGEX) return false;
  }
  return true;
}

static bool check_parentheses(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') return false;
  int cnt = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') cnt++;
    if (tokens[i].type == ')') cnt--;
    if (cnt < 0) return false;
  }
  return cnt == 0;
}

static int get_pri(int op) {
  switch(op) {
    case TK_DEREF: case TK_NEGATIVE: case '!': return 4;
    case '*': case '/': return 3;
    case '+': case '-': return 2;
    case TK_EQ: case TK_NEQ: return 1;
    case TK_AND: return 0; case TK_OR: return -1;
    default: return -2;
  }
}

static int find_dominant_op(int p, int q) {
  int cnt = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') cnt++;
    if (tokens[i].type == ')') cnt--;
    if (cnt != 0) continue;
    if (tokens[i].type == TK_DEREF || tokens[i].type == TK_NEGATIVE || tokens[i].type == '!') {
      return i;
    }
  }
  cnt = 0;
  int min_pri = 100, pos = -1;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') cnt++;
    if (tokens[i].type == ')') cnt--;
    if (cnt != 0) continue;
    int pri = get_pri(tokens[i].type);
    if (pri < 0) continue;
    if (pri <= min_pri) { min_pri = pri; pos = i; }
  }
  return pos;
}

static uint32_t eval(int p, int q, bool *success) {
  if (!*success || p > q) { *success = false; return 0; }
  if (p == q) {
    switch (tokens[p].type) {
      case TK_NUMBER: return atoi(tokens[p].str);
      case TK_HEX: { uint32_t v; sscanf(tokens[p].str, "%x", &v); return v; }
      case TK_REG: {
        if (!strcmp(tokens[p].str, "$eip")) return cpu.eip;
        if (!strcmp(tokens[p].str, "$eax")) return reg_l(0);
        if (!strcmp(tokens[p].str, "$ecx")) return reg_l(1);
        if (!strcmp(tokens[p].str, "$edx")) return reg_l(2);
        if (!strcmp(tokens[p].str, "$ebx")) return reg_l(3);
        if (!strcmp(tokens[p].str, "$esp")) return reg_l(4);
        if (!strcmp(tokens[p].str, "$ebp")) return reg_l(5);
        if (!strcmp(tokens[p].str, "$esi")) return reg_l(6);
        if (!strcmp(tokens[p].str, "$edi")) return reg_l(7);
        *success = false; return 0;
      }
      default: *success = false; return 0;
    }
  }
  if (check_parentheses(p, q)) return eval(p+1, q-1, success);
  int op = find_dominant_op(p, q);
  if (op == -1) { *success = false; return 0; }

  if (tokens[op].type == TK_NEGATIVE) return 0 - eval(op+1, q, success);
  if (tokens[op].type == TK_DEREF) {
    uint32_t addr = eval(op+1, q, success);
    return vaddr_read(addr, 4);
  }
  if (tokens[op].type == '!') return !eval(op+1, q, success);

  uint32_t l = eval(p, op-1, success);
  uint32_t r = eval(op+1, q, success);
  switch(tokens[op].type) {
    case '+': return l + r;
    case '-': return l - r;
    case '*': return l * r;
    case '/': return l / r;
    case TK_EQ: return l == r;
    case TK_NEQ: return l != r;
    case TK_AND: return l && r;
    case TK_OR: return l || r;
    default: *success = false; return 0;
  }
}

uint32_t expr(char *e, bool *success) {
  *success = make_token(e);
  if (!*success) return 0;

  // 🔥 核心修复：强制识别所有单目负号（带空格/不带空格都生效）
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == '-') {
      int prev_type = (i == 0) ? '(' : tokens[i-1].type;
      if (prev_type == '(' || get_pri(prev_type) >= 0) {
        tokens[i].type = TK_NEGATIVE;
      }
    }
    if (tokens[i].type == '*') {
      int prev_type = (i == 0) ? '(' : tokens[i-1].type;
      if (prev_type == '(' || get_pri(prev_type) >= 0) {
        tokens[i].type = TK_DEREF;
      }
    }
  }
  return eval(0, nr_token - 1, success);
}
