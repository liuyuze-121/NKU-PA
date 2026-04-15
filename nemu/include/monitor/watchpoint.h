#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  char expr[128];
  uint32_t old_val;
  int hit;
} WP;

void init_wp_pool(void);
WP* new_wp(char *expr_str);
void free_wp(int wp_no);
void display_wp(void);
bool check_wp(void);

#endif
