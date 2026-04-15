#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

// readline 分配的内存无需手动 free，系统会管理，删除错误的内存释放
char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    // 仅重置指针，不free，避免重复释放
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

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
  uint64_t N=0;
  if(args==NULL)
      N=1;
  else{
      int nRet=sscanf(args,"%llu",&N);
      if(nRet<=0){
          printf("args error in cmd_si\n");
          return 0;
      }
  }
  cpu_exec(N);
  return 0;
}

static int cmd_info(char *args) {
  char s;
  if(args==NULL){
      printf("args error in cmd_info\n");
      return 0;
  }
  int nRet=sscanf(args,"%c",&s);
  if(nRet<=0){
      printf("args error in cmd_info\n");
      return 0;
  }
  if(s=='r'){
    int i;
    for(i=0;i<8;i++)
      printf("%s\t0x%08x\n",regsl[i],reg_l(i));
    printf("eip\t0x%08x\n",cpu.eip);
    for(i=0;i<8;i++)
      printf("%s\t0x%08x\n",regsw[i],reg_w(i));
    for(i=0;i<8;i++)
      printf("%s\t0x%08x\n",regsb[i],reg_b(i));
    return 0;
  }
  printf("args error in cmd info\n");
  return 0;
}

static int cmd_x(char *args) {
  if(!args){
    printf("args error in cmd_x\n");
    return 0;
  }
  char* args_end= args + strlen(args),*first_args=strtok(args," ");
  if(!first_args){
    printf("args error in cmd_x\n");
    return 0;
  }
  char *exprs=first_args+strlen(first_args)+1;
  if(exprs>=args_end){
    printf("args error in cmd_x\n");
    return 0;
  }
  int n=atoi(first_args);
  bool success;
  vaddr_t addr=expr(exprs,&success);
  if(success==false)
    printf("error in expr()\n");
  printf("Memory:\n");
  for(int i=0;i<n;i++){
    printf("0x%08x:0x%08x\n",addr, vaddr_read(addr,4));
    addr+=4;
  }
  return 0;
}

// 统一输出 32位十六进制，无任何格式错误
static int cmd_p(char *args) {
  bool success;
  uint32_t res=expr(args,&success);
  if(success==false)
    printf("error in expr()\n");
  else
    printf("the value of expr is: 0x%08x\n",res);
  return 0;
}

static int cmd_help(char *args);

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "si [N]: execute N instructions step by step", cmd_si },
  { "info", "info r: print registers", cmd_info },
  { "x", "x N EXPR: scan N 4-byte memory from EXPR", cmd_x },
  { "p", "p EXPR: evaluate expression and output in hex", cmd_p },
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    if (str == NULL) continue;

    char *cmd = strtok(str, " ");
    if (cmd == NULL) continue;

    char *args = cmd + strlen(cmd) + 1;
    if (args >= str + strlen(str)) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { 
          // 退出命令，不做任何内存操作
          return; 
        }
        break;
      }
    }

    if (i == NR_CMD) printf("Unknown command '%s'\n", cmd);
  }
}
