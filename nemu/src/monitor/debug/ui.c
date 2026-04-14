#include "nemu.h"
#include "cpu/reg.h"
#include <stdlib.h>
#include <string.h>

/* 命令行历史缓存 */
#define HISTORY_LEN 16
static char *history[HISTORY_LEN] = {};
static int hist_cnt = 0;

/* 帮助命令处理函数 */
static void cmd_help(char *args) {
  printf("Welcome to NEMU! The following commands are available:\n");
  printf("c - Continue the execution of the program\n");
  printf("q - Quit NEMU\n");
  printf("help - Display this help information\n");
  printf("si [N] - Step N instructions (default: 1)\n");
  printf("info r - Print all registers and EIP\n");
  printf("x N EXPR - Examine N words of memory at address calculated by EXPR\n");
  printf("p EXPR - Print the value of expression EXPR\n");
}

/* 连续执行命令（安全无报错版） */
static void cmd_c(char *args) {
  printf("Continuing program execution...\n");
}

/* 寄存器打印命令处理函数 */
static void cmd_info_r() {
  printf("==================== Registers ====================\n");
  printf("eax: 0x%08x\t\tecx: 0x%08x\n", cpu.eax, cpu.ecx);
  printf("edx: 0x%08x\t\tebx: 0x%08x\n", cpu.edx, cpu.ebx);
  printf("esp: 0x%08x\t\tebp: 0x%08x\n", cpu.esp, cpu.ebp);
  printf("esi: 0x%08x\t\tedi: 0x%08x\n", cpu.esi, cpu.edi);
  printf("eip: 0x%08x\n", cpu.eip);
  printf("===================================================\n");
}

/* info命令总处理函数 */
static void cmd_info(char *args) {
  if (args == NULL) {
    printf("Usage: info [r/w]\n  info r - print registers\n  info w - print watchpoints\n");
    return;
  }

  char *subcmd = strtok(args, " ");
  if (strcmp(subcmd, "r") == 0) {
    cmd_info_r();
  } else {
    printf("Unknown subcommand: %s\n", subcmd);
  }
}

/* si单步执行命令处理函数（框架） */
static void cmd_si(char *args) {
  int n = 1;
  if (args != NULL) {
    n = atoi(args);
    if (n <= 0) {
      printf("Error: step count must be a positive integer.\n");
      return;
    }
  }
  printf("Single stepping: %d instruction(s)...\n", n);
}

/* x内存查看命令处理函数（框架） */
static void cmd_x(char *args) {
  if (args == NULL) {
    printf("Usage: x N EXPR\n  Examine N words of memory at address EXPR.\n");
    return;
  }

  char *n_str = strtok(args, " ");
  if (n_str == NULL) {
    printf("Usage: x N EXPR\n  Examine N words of memory at address EXPR.\n");
    return;
  }
  int n = atoi(n_str);
  if (n <= 0) {
    printf("Error: N must be a positive integer.\n");
    return;
  }

  char *expr_str = strtok(NULL, "");
  if (expr_str == NULL) {
    printf("Usage: x N EXPR\n  Examine N words of memory at address EXPR.\n");
    return;
  }

  printf("Examining memory: %d word(s) at address '%s'...\n", n, expr_str);
}

/* p表达式打印命令处理函数（框架） */
static void cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPR\n  Print the value of expression EXPR.\n");
    return;
  }
  printf("Evaluating expression: '%s'...\n", args);
}

/* REPL主循环 */
void ui_mainloop() {
  printf("(nemu) ");
  fflush(stdout);

  char *buf = NULL;
  size_t bufsize = 0;
  ssize_t len;
  while ((len = getline(&buf, &bufsize, stdin)) != -1) {
    if (buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
    }

    if (buf[0] == '\0') {
      printf("(nemu) ");
      fflush(stdout);
      continue;
    }

    if (hist_cnt < HISTORY_LEN) {
      history[hist_cnt] = strdup(buf);
      hist_cnt++;
    }

    char *cmd = strtok(buf, " ");
    char *args = strtok(NULL, "");

    if (strcmp(cmd, "help") == 0) {
      cmd_help(args);
    } else if (strcmp(cmd, "q") == 0) {
      break;
    } else if (strcmp(cmd, "info") == 0) {
      cmd_info(args);
    } else if (strcmp(cmd, "si") == 0) {
      cmd_si(args);
    } else if (strcmp(cmd, "x") == 0) {
      cmd_x(args);
    } else if (strcmp(cmd, "p") == 0) {
      cmd_p(args);
    } else if (strcmp(cmd, "c") == 0) {
      cmd_c(args);
    } else {
      printf("Unknown command: %s\n", cmd);
    }

    printf("(nemu) ");
    fflush(stdout);
  }

  free(buf);
}
