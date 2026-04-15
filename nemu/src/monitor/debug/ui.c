#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

/* 外部函数声明 */
void cpu_exec(uint64_t);

/* 读取命令行输入 */
char* rl_gets() {
  static char *line = NULL;
  line = readline("(nemu) ");
  if (line && *line) 
    add_history(line);
  return line;
}

/* ==================== 基础调试命令 ==================== */
static int cmd_c(char *args) { 
  cpu_exec(-1); 
  return 0; 
}

static int cmd_q(char *args) { 
  return -1; 
}

static int cmd_si(char *args) {
  uint64_t n = args ? atoll(args) : 1;
  cpu_exec(n);
  return 0;
}

/* ==================== 信息查看命令 ==================== */
static int cmd_info(char *args) {
  if (!args) {
    printf("Usage: info r (registers) or info w (watchpoints)\n");
    return 0;
  }

  if (*args == 'r') {
    for(int i = 0; i < 8; i++) 
      printf("%s\t0x%08x\n", regsl[i], reg_l(i));
    printf("eip\t0x%08x\n", cpu.eip);
  } 
  else if (*args == 'w') {
    display_wp();
  } 
  else {
    printf("Unknown info subcommand\n");
  }
  return 0;
}

/* ==================== 内存/表达式命令 ==================== */
static int cmd_x(char *args) {
  if (!args) return 0;
  char *space = strchr(args, ' ');
  if (!space) return 0;
  
  *space = '\0';
  int n = atoi(args);
  bool ok;
  uint32_t addr = expr(space + 1, &ok);
  
  if (ok) {
    for(int i = 0; i < n; i++) {
      printf("0x%08x: 0x%08x\n", addr, vaddr_read(addr, 4));
      addr += 4;
    }
  }
  return 0;
}

static int cmd_p(char *args) {
  if (!args) { 
    printf("error: no expression\n"); 
    return 0; 
  }
  
  bool ok;
  uint32_t res = expr(args, &ok);
  if (ok) {
    printf("the value of expr is: 0x%08x\n", res);
  } else {
    printf("error in expr()\n");
  }
  return 0;
}

/* ==================== 监视点命令 ==================== */
static int cmd_w(char *args) {
  if (!args) {
    printf("Usage: w <expression>\n");
    return 0;
  }
  new_wp(args);
  return 0;
}

static int cmd_d(char *args) {
  if (!args) {
    printf("Usage: d <watchpoint_number>\n");
    return 0;
  }
  int num = atoi(args);
  free_wp(num);
  return 0;
}

/* 命令映射表 */
static struct {
  char *name;
  int (*handler)(char*);
} cmd_table[] = {
  {"c",     cmd_c},
  {"q",     cmd_q},
  {"si",    cmd_si},
  {"info",  cmd_info},
  {"x",     cmd_x},
  {"p",     cmd_p},
  {"w",     cmd_w},
  {"d",     cmd_d},
};

/* UI主循环 */
void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) { 
    cmd_c(NULL); 
    return; 
  }

  while(1) {
    char *str = rl_gets();
    if (!str) continue;

    char *cmd = strtok(str, " ");
    char *arg = strtok(NULL, "");
    int ret = 0;
    int i;

    for(i = 0; i < sizeof(cmd_table)/sizeof(cmd_table[0]); i++) {
      if (!strcmp(cmd, cmd_table[i].name)) {
        ret = cmd_table[i].handler(arg);
        break;
      }
    }

    if (ret == -1) break;
    if (i >= sizeof(cmd_table)/sizeof(cmd_table[0])) 
      printf("Unknown command\n");
  }
}
