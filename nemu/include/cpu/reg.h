#ifndef __CPU_REG_H__
#define __CPU_REG_H__

#include "common.h"

enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };
enum { R_AX, R_CX, R_DX, R_BX, R_SP, R_BP, R_SI, R_DI };
enum { R_AL, R_CL, R_DL, R_BL, R_AH, R_CH, R_DH, R_BH };

typedef union {
  uint32_t _32;
  uint16_t _16;
  uint8_t _8[2];
} GPR;

typedef struct {
  union {
    GPR gpr[8];
    struct { uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi; };
    struct { uint16_t ax, cx, dx, bx, sp, bp, si, di; };
    struct { uint8_t al, ah, cl, ch, dl, dh, bl, bh; };
  };
  vaddr_t eip;
} CPU_state;

extern CPU_state cpu;

#define reg_l(index) (cpu.gpr[index]._32)
#define reg_w(index) (cpu.gpr[index]._16)
#define reg_b(index) (cpu.gpr[index & 0x3]._8[index >> 2])

extern const char* regsl[];
extern const char* regsw[];
extern const char* regsb[];

static inline const char* reg_name(int index, int width) {
  switch (width) {
    case 4: return regsl[index];
    case 1: return regsb[index];
    case 2: return regsw[index];
    default: assert(0);
  }
}

#endif
