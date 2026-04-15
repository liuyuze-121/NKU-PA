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
  {"\\$(eax|ecx|edx|ebx|esp|ebp|esi|edi|eip|ax|cx|dx|bx|sp|bp|si|di|al|cl|dl|bl|ah|ch|dh|bh)", TK_REG},
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
  {")", ')'}
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))
static regex_t re[NR_REGEX];

void init_regex(void) {
  int i; char errbuf[128];
  for (i = 0; i < NR_REGEX; i++) {
    int ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) { regerror(ret, &re[i], errbuf, 128); panic("regex err: %s", errbuf); }
  }
}

typedef struct token { int type; char str[32]; } Token;
Token tokens[32];
int nr_token;

static bool make_token(char *e) {
  int pos = 0, i; regmatch_t pm;
  nr_token = 0;
  while (e[pos] != '\0') {
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e+pos, 1, &pm, 0) == 0 && pm.rm_so == 0) {
        int len = pm.rm_eo;
        if (rules[i].token_type == TK_NOTYPE) { pos += len; break; }
        tokens[nr_token].type = rules[i].token_type;
        strncpy(tokens[nr_token].str, e+pos, len);
        tokens[nr_token].str[len] = '\0';
        nr_token++; pos += len; break;
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
    case TK_NEGATIVE: case TK_DEREF: case '!': return 4; // 单目运算符最高优先级
    case '*': case '/': return 3;
    case '+': case '-': return 2;
    case TK_EQ: case TK_NEQ: return 1;
    case TK_AND: return 0; case TK_OR: return -1;
    default: return -2;
  }
}

// 核心修复：优先查找单目运算符
static int find_dominant_op(int p, int q) {
  int cnt = 0;
  // 第一步：优先找单目负号/解引用/逻辑非
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') cnt++;
    if (tokens[i].type == ')') cnt--;
    if (cnt != 0) continue;
    if (tokens[i].type == TK_NEGATIVE || tokens[i].type == TK_DEREF || tokens[i].type == '!') {
      return i;
    }
  }

  // 第二步：找双目运算符
  cnt = 0;
  int min_pri = 100, pos = -1;
  for (int i = p; i <= q; i++) {
    int t = tokens[i].type;
    if (t == '(') cnt++; else if (t == ')') cnt--;
    if (cnt != 0) continue;
    int pri = get_pri(t);
    if (pri < 0) continue;
    if (pri <= min_pri) { min_pri = pri; pos = i; }
  }
  return pos;
}

static uint32_t eval(int p, int q, bool *success) {
  if (!*success || p > q) { *success = false; return 0; }
  if (p == q) {
    uint32_t v = 0;
    if (tokens[p].type == TK_NUMBER) v = atoi(tokens[p].str);
    else if (tokens[p].type == TK_HEX) sscanf(tokens[p].str, "%x", &v);
    else if (tokens[p].type == TK_REG) {
      if (!strcmp(tokens[p].str, "eip")) return cpu.eip;
      for (int i=0;i<8;i++) {
        if (!strcmp(tokens[p].str, regsl[i])) return reg_l(i);
        if (!strcmp(tokens[p].str, regsw[i])) return reg_w(i);
        if (!strcmp(tokens[p].str, regsb[i])) return reg_b(i);
      }
    }
    return v;
  }

  if (check_parentheses(p, q)) return eval(p+1, q-1, success);
  int op = find_dominant_op(p, q);
  if (op == -1) { *success = false; return 0; }

  if (tokens[op].type == TK_NEGATIVE) {
    uint32_t val = eval(op+1, q, success);
    return 0 - val;
  }
  if (tokens[op].type == TK_DEREF) return vaddr_read(eval(op+1,q,success),4);
  if (tokens[op].type == '!') return !eval(op+1,q,success);

  uint32_t l = eval(p, op-1, success);
  uint32_t r = eval(op+1, q, success);
  switch(tokens[op].type) {
    case '+': return l+r; case '-': return l-r;
    case '*': return l*r; case '/': return l/r;
    case TK_EQ: return l==r; case TK_NEQ: return l!=r;
    case TK_AND: return l&&r; case TK_OR: return l||r;
    default: *success = false; return 0;
  }
}

uint32_t expr(char *e, bool *success) {
  *success = true;
  if (!make_token(e)) { *success = false; return 0; }

  // 核心修复：强制标记所有连续单目负号
  for (int i=0; i<nr_token; i++) {
    if (tokens[i].type == '-') {
      // 满足任意条件：开头 / 左括号 / 前一个是运算符 → 标记为单目负号
      if (i == 0 || tokens[i-1].type == '(' || get_pri(tokens[i-1].type) != -2) {
        tokens[i].type = TK_NEGATIVE;
      }
    }
    if (tokens[i].type == '*') {
      if (i == 0 || tokens[i-1].type == '(' || get_pri(tokens[i-1].type) != -2) {
        tokens[i].type = TK_DEREF;
      }
    }
  }
  return eval(0, nr_token-1, success);
}
