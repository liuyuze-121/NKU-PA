#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <assert.h>

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
  {"0x[1-9A-Fa-f][0-9A-Fa-f]*", TK_HEX},
  {"[0-9][0-9]*", TK_NUMBER},
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

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
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
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        if(substr_len>32)
          assert(0);
        if(rules[i].token_type==TK_NOTYPE)
          break;
        else{
          tokens[nr_token].type=rules[i].token_type;
          switch (rules[i].token_type) {
            case TK_NUMBER:
              strncpy(tokens[nr_token].str,substr_start,substr_len);
              *(tokens[nr_token].str+substr_len)='\0';
              break;
            case TK_HEX:
              strncpy(tokens[nr_token].str,substr_start+2,substr_len-2);
              *(tokens[nr_token].str+substr_len-2)='\0';
              break;
            case TK_REG:
              strncpy(tokens[nr_token].str,substr_start+1,substr_len-1);
              *(tokens[nr_token].str+substr_len-1)='\0';
              break;
            default:
              break;
          }
          nr_token++;
        }
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

uint32_t expr(char *e, bool *success) {
  *success = true;
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  return 0;
}
