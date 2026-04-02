#include "nemu.h"
#include "cpu/reg.h"
#include <stdlib.h>
#include <string.h>

/* 简易命令行历史 */
#define HISTORY_LEN 16
static char *history[HISTORY_LEN] = {};
static int hist_cnt = 0;

/* 打印帮助信息 */
static void cmd_help(char *args) {
  printf("Welcome to NEMU! The following commands are available:\n");
  printf("c - Continue the execution of the program\n");
  printf("q - Quit NEMU\n");
  printf("help - Display this information\n");
  printf("si [N] - Step N instructions (default: 1)\n");
  printf("info r - Print all registers\n");
  printf("x N EXPR - Examine the memory at EXPR for N words\n");
  printf("p EXPR - Print the value of EXPR\n");
}

/* 打印所有寄存器 */
static void cmd_info_r() {
  printf("==================== Registers ====================\n");
  printf("eax: 0x%08x\t\tecx: 0x%08x\n", cpu.eax, cpu.ecx);
  printf("edx: 0x%08x\t\tebx: 0x%08x\n", cpu.edx, cpu.ebx);
  printf("esp: 0x%08x\t\tebp: 0x%08x\n", cpu.esp, cpu.ebp);
  printf("esi: 0x%08x\t\tedi: 0x%08x\n", cpu.esi, cpu.edi);
  printf("eip: 0x%08x\n", cpu.eip);
  printf("===================================================\n");
}

/* 处理 info 子命令 */
static void cmd_info(char *args) {
  if (args == NULL) {
    printf("Please specify what to print. See 'help' for more information.\n");
    return;
  }

  char *subcmd = strtok(args, " ");
  if (strcmp(subcmd, "r") == 0) {
    cmd_info_r();
  } else {
    printf("Unknown subcommand: %s\n", subcmd);
  }
}

/* 处理 si 单步执行命令 */
static void cmd_si(char *args) {
  int n = 1; // 默认单步执行1条指令
  
  // 如果有参数，解析参数
  if (args != NULL) {
    n = atoi(args);
    if (n <= 0) {
      printf("Error: argument must be a positive integer.\n");
      return;
    }
  }

  printf("Single stepping: %d instruction(s)...\n", n);
  // TODO: 这里后续会添加真正的CPU单步执行逻辑
}

/* 主命令循环 */
void ui_mainloop() {
  printf("(nemu) ");
  fflush(stdout);

  char *buf = NULL;
  size_t bufsize = 0;
  ssize_t len;
  while ((len = getline(&buf, &bufsize, stdin)) != -1) {
    /* 去除末尾换行符 */
    if (buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
    }

    /* 跳过空行 */
    if (buf[0] == '\0') {
      printf("(nemu) ");
      fflush(stdout);
      continue;
    }

    /* 保存命令历史 */
    if (hist_cnt < HISTORY_LEN) {
      history[hist_cnt] = strdup(buf);
      hist_cnt++;
    }

    /* 解析命令 */
    char *cmd = strtok(buf, " ");
    char *args = strtok(NULL, "");

    /* 执行命令 */
    if (strcmp(cmd, "help") == 0) {
      cmd_help(args);
    } else if (strcmp(cmd, "q") == 0) {
      break;
    } else if (strcmp(cmd, "info") == 0) {
      cmd_info(args);
    } else if (strcmp(cmd, "si") == 0) {
      cmd_si(args);
    } else {
      printf("Unknown command: %s\n", cmd);
    }

    printf("(nemu) ");
    fflush(stdout);
  }

  free(buf);
}
