#ifndef __CPU_REG_H__
#define __CPU_REG_H__

#include "common.h"
/* 寄存器索引枚举 */
enum {
      R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI,
          NR_REG,
          
};

enum {
      R_AL, R_CL, R_DL, R_BL, R_AH, R_CH, R_DH, R_BH,
      
};

enum {
      R_AX, R_CX, R_DX, R_BX, R_SP, R_BP, R_SI, R_DI,
      
};
/* 通用寄存器联合体：实现32/16/8位寄存器的硬件重叠映射 */
typedef union {
  union {
            uint32_t _32;
            uint16_t _16;
            uint8_t _8;
                                
  } gpr[8];

      /* 32位通用寄存器别名，对应i386的8个32位GPR */
  struct {
            uint32_t eax;
            uint32_t ecx;
            uint32_t edx;
            uint32_t ebx;
            uint32_t esp;
            uint32_t ebp;
            uint32_t esi;
            uint32_t edi;
                                                                        
  };

      /* 16位通用寄存器别名，对应32位寄存器的低16位 */
  struct {
            uint16_t ax;
            uint16_t cx;
            uint16_t dx;
            uint16_t bx;
            uint16_t sp;
            uint16_t bp; 
            uint16_t si;
            uint16_t di;
                                                                        
  };

  /* 8位通用寄存器别名，对应16位寄存器的高8位/低8位 */
  struct {
            uint8_t al;
            uint8_t ah;
            uint8_t cl;
            uint8_t ch;
            uint8_t dl;
            uint8_t dh;
            uint8_t bl;
            uint8_t bh;
                                                                        
  };

      /* 指令指针寄存器EIP，核心的程序计数器 */
      vaddr_t eip;

      
} CPU_state;

extern CPU_state cpu;

/* 寄存器访问宏，与框架代码的reg_test()完全匹配 */
#define reg_l(index) (cpu.gpr[index]._32)
#define reg_w(index) (cpu.gpr[index]._16)
#define reg_b(index) (cpu.gpr[index]._8)

/* 寄存器名称数组，用于后续info r命令打印 */
extern const char* regsl[];
extern const char* regsw[];
extern const char* regsb[];
/* reg_name 函数声明 */
const char* reg_name(int index, int width);

#endif

