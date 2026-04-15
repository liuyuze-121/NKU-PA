#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

char* rl_gets() {
  static char *line_read = NULL;
  if (line_read) { free(line_read); line_read = NULL; }
  line_read = readline("(nemu) ");
  if (line_read && *line_read) add_history(line_read);
  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_si(char *args) {
  uint64_t N = args ? atoll(args) : 1;
  cpu_exec(N);
  return 0;
}

static int cmd_info(char *args) {
  if (!args) { printf("args error\n"); return 0; }
  if (*args == 'r') {
    for(int i=0;i<8;i++) printf("%s\t0x%08x\n", regsl[i], reg_l(i));
    printf("eip\t0x%08x\n", cpu.eip);
    for(int i=0;i<8;i++) printf("%s\t0x%08x\n", regsw[i], reg_w(i));
    for(int i=0;i<8;i++) printf("%s\t0x%08x\n", regsb[i], reg_b(i));
  }
  return 0;
}

static int cmd_x(char *args) {
  if (!args) { printf("args error\n"); return 0; }
  char *space = strchr(args, ' ');
  if (!space) { printf("args error\n"); return 0; }
  *space = '\0';
  int n = atoi(args);
  bool success;
  uint32_t addr = expr(space+1, &success);
  if (!success) { printf("expr error\n"); return 0; }
  printf("Memory:\n");
  for(int i=0; i<n; i++){
    printf("0x%08x: 0x%08x\n", addr, vaddr_read(addr,4));
    addr +=4;
  }
  return 0;
}

// 统一输出 32位十六进制 (标准格式)
static int cmd_p(char *args) {
  if (!args) { printf("error in expr()\n"); return 0; }
  bool success;
  uint32_t res = expr(args, &success);
  if (success) printf("the value of expr is: 0x%08x\n", res);
  else printf("error in expr()\n");
  return 0;
}

static int cmd_help(char *args);

static struct {
  char *name;
  char *description;
  int (*handler)(char*);
} cmd_table[] = {
  {"help", "show help information", cmd_help},
  {"c",    "continue execution", cmd_c},
  {"q",    "quit nemu", cmd_q},
  {"si",   "step i instructions", cmd_si},
  {"info", "info r - show registers", cmd_info},
  {"x",    "x N expr - scan memory", cmd_x},
  {"p",    "p expr - evaluate expr (hex)", cmd_p},
};

#define NR_CMD (sizeof(cmd_table)/sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  for(int i=0; i<NR_CMD; i++){
    printf("%s\t%s\n", cmd_table[i].name, cmd_table[i].description);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) { cmd_c(NULL); return; }
  while(1) {
    char *str = rl_gets();
    if (!str) continue;
    char *cmd = strtok(str, " ");
    char *args = strtok(NULL, "");
    int i;
    for(i=0; i<NR_CMD; i++){
      if (!strcmp(cmd, cmd_table[i].name)) {
        if (cmd_table[i].handler(args) < 0) break;
        break;
      }
    }
    if (i == NR_CMD) printf("Unknown command\n");
    free(str);
  }
}
