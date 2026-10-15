#include <stdint.h>
#include <stdio.h>

/* Populated by HardFault_Handler before halting — readable in debugger Variables view
   even without a serial connection. */
volatile uint32_t fault_pc    = 0;
volatile uint32_t fault_lr    = 0;
volatile uint32_t fault_cfsr  = 0;
volatile uint32_t fault_hfsr  = 0;
volatile uint32_t fault_mmfar = 0;
volatile uint32_t fault_bfar  = 0;

/* Override the weak default from startup_psoc6.S */
void HardFault_Handler(void)
{
    /* Grab whichever stack was active when the fault fired */
    uint32_t *frame;
    __asm volatile (
        "TST   LR, #4       \n"   /* LR bit 2 = 0 → MSP, 1 → PSP */
        "ITE   EQ           \n"
        "MRSEQ %0, MSP      \n"
        "MRSNE %0, PSP      \n"
        : "=r" (frame) :: "memory"
    );

    /* Exception frame layout (same for all Cortex-M):
     *   frame[0] = r0
     *   frame[1] = r1
     *   frame[2] = r2
     *   frame[3] = r3
     *   frame[4] = r12
     *   frame[5] = lr  (return address of the faulting call)
     *   frame[6] = pc  <-- THE INSTRUCTION THAT CAUSED THE FAULT
     *   frame[7] = xpsr
     */
    fault_pc    = frame[6];
    fault_lr    = frame[5];
    fault_cfsr  = *(volatile uint32_t *)0xE000ED28U;  /* Configurable Fault Status */
    fault_hfsr  = *(volatile uint32_t *)0xE000ED2CU;  /* Hard Fault Status */
    fault_mmfar = *(volatile uint32_t *)0xE000ED34U;  /* MemManage fault address */
    fault_bfar  = *(volatile uint32_t *)0xE000ED38U;  /* BusFault address */

    printf("\r\n========== HARD FAULT ==========\r\n");
    printf("  PC   = 0x%08lx  <-- faulting instruction\r\n", (unsigned long)fault_pc);
    printf("  LR   = 0x%08lx  <-- caller\r\n",               (unsigned long)fault_lr);
    printf("  CFSR = 0x%08lx\r\n",                            (unsigned long)fault_cfsr);
    printf("  HFSR = 0x%08lx\r\n",                            (unsigned long)fault_hfsr);
    printf("  MMFAR= 0x%08lx\r\n",                            (unsigned long)fault_mmfar);
    printf("  BFAR = 0x%08lx\r\n",                            (unsigned long)fault_bfar);

    /* CFSR decoder */
    if (fault_cfsr & 0x0001U) printf("  >> IACCVIOL: MPU instruction access violation\r\n");
    if (fault_cfsr & 0x0002U) printf("  >> DACCVIOL: MPU data access violation (MMFAR valid if bit7 set)\r\n");
    if (fault_cfsr & 0x0008U) printf("  >> MUNSTKERR: fault on exception unstacking\r\n");
    if (fault_cfsr & 0x0010U) printf("  >> MSTKERR: fault on exception stacking\r\n");
    if (fault_cfsr & 0x0100U) printf("  >> IBUSERR: instruction bus error\r\n");
    if (fault_cfsr & 0x0200U) printf("  >> PRECISERR: precise data bus error (BFAR valid if bit15 set)\r\n");
    if (fault_cfsr & 0x0400U) printf("  >> IMPRECISERR: imprecise data bus error\r\n");
    if (fault_cfsr & 0x1000U) printf("  >> BUSTKERR: fault on exception stacking\r\n");
    if (fault_cfsr & 0x10000U) printf("  >> UNDEFINSTR: undefined instruction\r\n");
    if (fault_cfsr & 0x20000U) printf("  >> INVSTATE: invalid CPU state (EPSR)\r\n");
    if (fault_cfsr & 0x40000U) printf("  >> INVPC: invalid PC on exception return\r\n");
    if (fault_cfsr & 0x100000U) printf("  >> UNALIGNED: unaligned memory access\r\n");
    if (fault_cfsr & 0x200000U) printf("  >> DIVBYZERO: divide by zero\r\n");

    printf("=================================\r\n");
    fflush(stdout);

    /* Halt. With debugger: stops here, fault_* visible in Variables view.
       Without debugger: spins so you can attach later and inspect fault_* in RAM. */
    __asm volatile ("BKPT #0");
    for (;;) {}
}
