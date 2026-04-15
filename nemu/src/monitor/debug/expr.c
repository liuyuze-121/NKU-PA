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
  int i;
  char errbuf[128];
  int ret;
  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], errbuf, 128);
      panic("regex failed: %s", errbuf);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
  int pos = 0;
  int i;
  regmatch_t pm;
  nr_token = 0;
  while (e[pos] != '\0') {
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + pos, 1, &pm, 0) == 0 && pm.rm_so == 0) {
        int len = pm.rm_eo;
        if (len >= 32) assert(0);
        if (rules[i].token_type == TK_NOTYPE) {
          pos += len;
          break;
        }
        tokens[nr_token].type = rules[i].token_type;
        switch (rules[i].token_type) {
          case TK_NUMBER:
            strncpy(tokens[nr_token].str, e + pos, len);
            tokens[nr_token].str[len] = '\0';
            break;
          case TK_HEX:
            strncpy(tokens[nr_token].str, e + pos, len);
            tokens[nr_token].str[len] = '\0';
            break;
          case TK_REG:
            strncpy(tokens[nr_token].str, e + pos + 1, len - 1);
            tokens[nr_token].str[len - 1] = '\0';
            break;
          default:
            tokens[nr_token].str[0] = '\0';
            break;
        }
        nr_token++;
        pos += len;
        break;
      }
    }
    if (i == NR_REGEX) {
      printf("no match at %d\n%s\n%*.s^\n", pos, e, pos, "");
      return false;
    }
  }
  return true;
}

static bool check_parentheses(int p, int q) {
  if (p >= q) return false;
  if (tokens[p].type != '(' || tokens[q].type != ')') return false;
  int cnt = 0;
  int i;
  for (i = p; i <= q; i++) {
    if (tokens[i].type == '(') cnt++;
    else if (tokens[i].type == ')') cnt--;
    if (cnt < 0) return false;
  }
  return cnt == 0;
}

static int get_pri(int op) {
  switch (op) {
    case TK_DEREF:
    case TK_NEGATIVE:
    case '!': return 4;
    case '*':
    case '/': return 3;
    case '+':
    case '-': return 2;
    case TK_EQ:
    case TK_NEQ: return 1;
    case TK_AND: return 0;
    case TK_OR: return -1;
    default: return -2;
  }
}

static int find_dominant_op(int p, int q) {
  int pri = 100;
  int pos = -1;
  int cnt = 0;
  int i;
  for (i = p; i <= q; i++) {
    int t = tokens[i].type;
    if (t == '(') cnt++;
    else if (t == ')') cnt--;
    if (cnt != 0) continue;
    
    // 单目运算符优先级最高，直接返回
    if(t == TK_NEGATIVE || t == TK_DEREF || t == '!'){
      return i;
    }

    int cp = get_pri(t);
    if (cp < 0) continue;
    if (cp <= pri) {
      pri = cp;
      pos = i;
    }
  }
  return pos;
}

static uint32_t eval(int p, int q, bool *success) {
  if (!*success) return 0;
  if (p > q) {
    *success = false;
    return 0;
  }
  if (p == q) {
    uint32_t v = 0;
    if (tokens[p].type == TK_NUMBER) {
      v = atoi(tokens[p].str);
    } else if (tokens[p].type == TK_HEX) {
      sscanf(tokens[p].str, "%x", &v);
    } else if (tokens[p].type == TK_REG) {
      char *s = tokens[p].str;
      if (strcmp(s, "eip") == 0) return cpu.eip;
      int i;
      for (i = 0; i < 8; i++) {
        if (strcmp(s, regsl[i]) == 0) return reg_l(i);
        if (strcmp(s, regsw[i]) == 0) return reg_w(i);
        if (strcmp(s, regsb[i]) == 0) return reg_b(i);
      }
      *success = false;
      return 0;
    } else {
      *success = false;
    }
    return v;
  }
  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }
  int op = find_dominant_op(p, q);
  if (op < 0) {
    *success = false;
    return 0;
  }
  int t = tokens[op].type;

  if (t == TK_NEGATIVE) {
    uint32_t v = eval(op + 1, q, success);
    return 0 - v;
  }
  if (t == TK_DEREF) {
    uint32_t addr = eval(op + 1, q, success);
    return vaddr_read(addr, 4);
  }
  if (t == '!') {
    uint32_t v = eval(op + 1, q, success);
    return !v;
  }

  uint32_t l = eval(p, op - 1, success);
  uint32_t r = eval(op + 1, q, success);
  switch (t) {
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
  *success = true;
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  // ✅ 强制修复：连续负号全部标记为单目负号
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == '-') {
      if( i == 0 || 
          tokens[i-1].type == '(' || 
          get_pri(tokens[i-1].type) != -2)  // 只要前一个是运算符，就标记
      {
        tokens[i].type = TK_NEGATIVE;
      }
    }
    if (tokens[i].type == '*') {
      if( i == 0 || tokens[i-1].type == '(' || get_pri(tokens[i-1].type) != -2){
        tokens[i].type = TK_DEREF;
      }
    }
  }

  return eval(0, nr_token - 1, success);
}
