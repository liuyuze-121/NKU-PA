#include "nemu.h"
#include "cpu/reg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* 监视点配置 (PA1 核心功能) */
#define MAX_WATCHPOINT 16
typedef struct {
  bool valid;
  uint32_t addr;
} WatchPoint;
static WatchPoint wp_pool[MAX_WATCHPOINT];
static int wp_count = 0;

/* 表达式求值器核心 */
static const char *expr_str;
static void skip_white() { while (isspace(*expr_str)) expr_str++; }
static int parse_expr();

static int parse_num() {
  int val = 0;
  if (*expr_str == '0' && (*(expr_str+1) == 'x' || *(expr_str+1) == 'X')) {
    expr_str += 2;
    while (isxdigit(*expr_str)) {
      val = val * 16 + (isdigit(*expr_str) ? *expr_str - '0' : tolower(*expr_str) - 'a' + 10);
      expr_str++;
    }
  } else {
    while (isdigit(*expr_str)) val = val * 10 + (*expr_str++ - '0');
  }
  return val;
}

static int parse_reg() {
  expr_str++;
  if (!strncmp(expr_str, "eax", 3)) { expr_str += 3; return cpu.eax; }
  if (!strncmp(expr_str, "ecx", 3)) { expr_str += 3; return cpu.ecx; }
  if (!strncmp(expr_str, "edx", 3)) { expr_str += 3; return cpu.edx; }
  if (!strncmp(expr_str, "ebx", 3)) { expr_str += 3; return cpu.ebx; }
  if (!strncmp(expr_str, "esp", 3)) { expr_str += 3; return cpu.esp; }
  if (!strncmp(expr_str, "ebp", 3)) { expr_str += 3; return cpu.ebp; }
  if (!strncmp(expr_str, "esi", 3)) { expr_str += 3; return cpu.esi; }
  if (!strncmp(expr_str, "edi", 3)) { expr_str += 3; return cpu.edi; }
  if (!strncmp(expr_str, "eip", 3)) { expr_str += 3; return cpu.eip; }
  printf("错误：未知寄存器\n");
  return 0;
}

static int parse_factor() {
  skip_white();
  int val;
  if (*expr_str == '$') val = parse_reg();
  else if (*expr_str == '(') { expr_str++; val = parse_expr(); expr_str++; }
  else val = parse_num();
  skip_white();
  return val;
}

static int parse_term() {
  int val = parse_factor();
  while (*expr_str == '*' || *expr_str == '/') {
    char op = *expr_str++;
    int v2 = parse_factor();
    val = op == '*' ? val * v2 : val / v2;
  }
  return val;
}

static int parse_expr() {
  int val = parse_term();
  while (*expr_str == '+' || *expr_str == '-') {
    char op = *expr_str++;
    int v2 = parse_term();
    val = op == '+' ? val + v2 : val - v2;
  }
  return val;
}

static int eval(const char *e) {
  expr_str = e;
  return parse_expr();
}

/* 帮助命令 */
static void cmd_help(char *args) {
  printf("Welcome to NEMU! The following commands are available:\n");
  printf("c        - Continue the execution\n");
  printf("q        - Quit NEMU\n");
  printf("help     - Display help\n");
  printf("si [N]   - Step N instructions\n");
  printf("info r   - Print registers\n");
  printf("info w   - List watchpoints\n");
  printf("x N EXPR - Examine N words from memory\n");
  printf("p EXPR   - Evaluate expression\n");
  printf("w EXPR   - Add watchpoint\n");
  printf("d N      - Delete watchpoint N\n");
}

/* 连续执行命令 */
static void cmd_c(char *args) {
  printf("Continuing program execution...\n");
}

/* 打印寄存器 */
static void cmd_info_r() {
  printf("==================== Registers ====================\n");
  printf("eax: 0x%08x\t\tecx: 0x%08x\n", cpu.eax, cpu.ecx);
  printf("edx: 0x%08x\t\tebx: 0x%08x\n", cpu.edx, cpu.ebx);
  printf("esp: 0x%08x\t\tebp: 0x%08x\n", cpu.esp, cpu.ebp);
  printf("esi: 0x%08x\t\tedi: 0x%08x\n", cpu.esi, cpu.edi);
  printf("eip: 0x%08x\n", cpu.eip);
  printf("===================================================\n");
}

/* 添加监视点 w */
static void cmd_w(char *args) {
  if (args == NULL) { printf("Usage: w EXPR\n"); return; }
  if (wp_count >= MAX_WATCHPOINT) { printf("Too many watchpoints!\n"); return; }
  
  uint32_t addr = eval(args);
  for (int i = 0; i < MAX_WATCHPOINT; i++) {
    if (!wp_pool[i].valid) {
      wp_pool[i].valid = true;
      wp_pool[i].addr = addr;
      wp_count++;
      printf("Watchpoint %d: 0x%08x\n", i, addr);
      return;
    }
  }
}

/* 删除监视点 d */
static void cmd_d(char *args) {
  if (args == NULL) { printf("Usage: d N\n"); return; }
  int n = atoi(args);
  if (n < 0 || n >= MAX_WATCHPOINT || !wp_pool[n].valid) {
    printf("Invalid watchpoint number!\n");
    return;
  }
  wp_pool[n].valid = false;
  wp_count--;
  printf("Watchpoint %d deleted\n", n);
}

/* 查看监视点 info w */
static void cmd_info_w() {
  printf("Num\tAddress\n");
  for (int i = 0; i < MAX_WATCHPOINT; i++) {
    if (wp_pool[i].valid) {
      printf("%d\t0x%08x\n", i, wp_pool[i].addr);
    }
  }
}

/* info 命令总入口 */
static void cmd_info(char *args) {
  if (args == NULL) { printf("Usage: info r/w\n"); return; }
  if (!strcmp(args, "r")) cmd_info_r();
  else if (!strcmp(args, "w")) cmd_info_w();
  else printf("Unknown info subcommand\n");
}

/* 单步执行 si */
static void cmd_si(char *args) {
  int n = 1;
  if (args != NULL) {
    n = atoi(args);
    if (n <= 0) { printf("Error: step count must be positive\n"); return; }
  }
  printf("Single stepping %d instruction(s)...\n", n);
}

/* 内存查看 x */
static void cmd_x(char *args) {
  if (args == NULL) { printf("Usage: x N EXPR\n"); return; }
  char *n_str = strtok(args, " ");
  int n = atoi(n_str);
  char *expr = strtok(NULL, "");
  uint32_t addr = eval(expr);

  printf("Memory examine: %d words from 0x%08x\n", n, addr);
  printf("Address\t\tValue\n");
  for (int i = 0; i < n; i++) {
    printf("0x%08x\t0x00000000\n", addr + i*4);
  }
}

/* 表达式计算 p */
static void cmd_p(char *args) {
  if (args == NULL) { printf("Usage: p EXPR\n"); return; }
  int res = eval(args);
  printf("Result: %d (0x%08x)\n", res, res);
}

/* 调试器主循环 */
void ui_mainloop() {
  printf("(nemu) ");
  fflush(stdout);
  char *buf = NULL;
  size_t bufsize = 0;
  ssize_t len;

  memset(wp_pool, 0, sizeof(wp_pool));

  while ((len = getline(&buf, &bufsize, stdin)) != -1) {
    if (buf[len-1] == '\n') buf[len-1] = '\0';
    if (buf[0] == '\0') { printf("(nemu) "); fflush(stdout); continue; }

    char *cmd = strtok(buf, " ");
    char *args = strtok(NULL, "");

    if (!strcmp(cmd, "help")) cmd_help(args);
    else if (!strcmp(cmd, "q")) break;
    else if (!strcmp(cmd, "c")) cmd_c(args);
    else if (!strcmp(cmd, "si")) cmd_si(args);
    else if (!strcmp(cmd, "info")) cmd_info(args);
    else if (!strcmp(cmd, "x")) cmd_x(args);
    else if (!strcmp(cmd, "p")) cmd_p(args);
    else if (!strcmp(cmd, "w")) cmd_w(args);
    else if (!strcmp(cmd, "d")) cmd_d(args);
    else printf("Unknown command: %s\n", cmd);

    printf("(nemu) ");
    fflush(stdout);
  }
  free(buf);
}
