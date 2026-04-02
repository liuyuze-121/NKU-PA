#ifndef __CPU_REG_H__
#define __CPU_REG_H__

#include "common.h"

/* 寄存器索引枚举 */
enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };
enum { R_AX, R_CX, R_DX, R_BX, R_SP, R_BP, R_SI, R_DI };
enum { R_AL, R_CL, R_DL, R_BL, R_AH, R_CH, R_DH, R_BH };

/* 分层定义1：单个通用寄存器的联合体（32/16/8位重叠） */
typedef union {
  uint32_t _32;
  uint16_t _16;
  uint8_t _8[2];
} GPR;

/* 分层定义2：CPU状态结构体 */
typedef struct {
  /* 8个通用寄存器 */
  union {
    GPR gpr[8];
    struct { uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi; };
    struct { uint16_t ax, cx, dx, bx, sp, bp, si, di; };
    struct { uint8_t al, ah, cl, ch, dl, dh, bl, bh; };
  };
  /* 独立的指令指针EIP（不与通用寄存器重叠） */
  vaddr_t eip;
} CPU_state;

extern CPU_state cpu;

/* 寄存器访问宏 */
#define reg_l(index) (cpu.gpr[index]._32)
#define reg_w(index) (cpu.gpr[index]._16)
#define reg_b(index) (cpu.gpr[index & 0x3]._8[index >> 2])

/* 寄存器名称数组声明 */
extern const char* regsl[];
extern const char* regsw[];
extern const char* regsb[];

/* 寄存器名称获取函数 */
static inline const char* reg_name(int index, int width) {
  switch (width) {
    case 4: return regsl[index];
    case 1: return regsb[index];
    case 2: return regsw[index];
    default: assert(0);
  }
}

#endif
