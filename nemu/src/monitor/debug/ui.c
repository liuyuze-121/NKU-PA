#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "nemu.h"
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

char* rl_gets() {
  char *line = readline("(nemu) ");
  if (line && *line) add_history(line);
  return line;
}

static int cmd_c(char *args) { cpu_exec(-1); return 0; }
static int cmd_q(char *args) { return -1; }

static int cmd_si(char *args) {
  uint64_t n = args ? atoll(args) : 1;
  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args) {
  if (args && *args == 'r') {
    for(int i=0; i<8; i++) printf("%s\t0x%08x\n", regsl[i], reg_l(i));
    printf("eip\t0x%08x\n", cpu.eip);
  }
  return 0;
}

static int cmd_x(char *args) {
  if (!args) return 0;
  char *space = strchr(args, ' ');
  if (!space) return 0;
  *space = 0;
  int n = atoi(args);
  bool ok;
  uint32_t addr = expr(space+1, &ok);
  if (ok) for(int i=0; i<n; i++) {
    printf("0x%08x: 0x%08x\n", addr, vaddr_read(addr,4));
    addr +=4;
  }
  return 0;
}

static int cmd_p(char *args) {
  if (!args) return 0;
  bool ok;
  uint32_t res = expr(args, &ok);
  if (ok) printf("the value of expr is: 0x%08x\n", res);
  return 0;
}

static struct {
  char *name;
  int (*handler)(char*);
} cmd_table[] = {
  {"c", cmd_c}, {"q", cmd_q}, {"si", cmd_si},
  {"info", cmd_info}, {"x", cmd_x}, {"p", cmd_p},
};

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) { cmd_c(NULL); return; }
  while(1) {
    char *str = rl_gets();
    if (!str) continue;
    char *cmd = strtok(str, " ");
    char *arg = strtok(NULL, "");
    int ret = 0, i;
    for(i=0; i<sizeof(cmd_table)/sizeof(cmd_table[0]); i++) {
      if (!strcmp(cmd, cmd_table[i].name)) {
        ret = cmd_table[i].handler(arg);
        break;
      }
    }
    if (ret == -1) break;
  }
}
