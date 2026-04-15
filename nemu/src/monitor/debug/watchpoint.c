#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include <stdio.h>
#include <string.h>

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;
static int wp_id = 0;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
    memset(wp_pool[i].expr, 0, sizeof(wp_pool[i].expr));
    wp_pool[i].old_val = 0;
    wp_pool[i].hit = 0;
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

WP* new_wp(char *expr_str) {
  if (free_ == NULL) {
    printf("No free watchpoint!\n");
    return NULL;
  }

  WP *p = free_;
  free_ = free_->next;

  p->NO = wp_id++;
  strncpy(p->expr, expr_str, sizeof(p->expr)-1);
  bool success;
  p->old_val = expr(p->expr, &success);
  p->hit = 0;

  p->next = head;
  head = p;

  printf("Set watchpoint %d: %s\n", p->NO, p->expr);
  return p;
}

void free_wp(int wp_no) {
  WP **pp = &head;
  while (*pp) {
    if ((*pp)->NO == wp_no) {
      WP *p = *pp;
      *pp = p->next;
      p->next = free_;
      free_ = p;
      printf("Delete watchpoint %d\n", wp_no);
      return;
    }
    pp = &(*pp)->next;
  }
  printf("Watchpoint %d not found\n", wp_no);
}

void display_wp(void) {
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }
  printf("NO\tEXPR\t\tHIT\n");
  WP *p = head;
  while (p) {
    printf("%d\t%s\t%d\n", p->NO, p->expr, p->hit);
    p = p->next;
  }
}

bool check_wp(void) {
  WP *p = head;
  bool hit = false;
  while (p) {
    bool success;
    uint32_t new_val = expr(p->expr, &success);
    if (success && new_val != p->old_val) {
      hit = true;
      p->hit ++;
      printf("\nWatchpoint %d hit!\n", p->NO);
      printf("Expr: %s\n", p->expr);
      printf("Old: 0x%08x, New: 0x%08x\n", p->old_val, new_val);
      p->old_val = new_val;
    }
    p = p->next;
  }
  return !hit;
}
