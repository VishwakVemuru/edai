/* Minimal start-up for a Cortex-M: the vector table, the FPU switched on (where there is one), .data copied from flash,
   .bss cleared, then main().  For a real board, use your vendor's start-up code instead (Pico SDK, STM32Cube, nRF SDK):
   the SDK needs nothing from it except the FPU switched on before the first call. */
#include <stdint.h>

extern uint32_t __data_load[], __data_start[], __data_end[], __bss_start[], __bss_end[], __stack_top[];
int main(void);
void Reset_Handler(void);

static void Default_Handler(void) { for (;;) {} }

__attribute__((section(".vectors"), used)) static const uintptr_t vectors[16] = {
    (uintptr_t)__stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,      /* NMI, HardFault */
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,      /* MemManage, BusFault */
    (uintptr_t)Default_Handler, 0, 0, 0, 0,                      /* UsageFault, reserved */
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, 0,   /* SVCall, DebugMonitor, reserved */
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,      /* PendSV, SysTick */
};

void Reset_Handler(void)
{
#if defined(__ARM_FP)
    *(volatile uint32_t *)0xE000ED88u |= 0xFu << 20;            /* CPACR: full access to CP10 and CP11, the FPU */
    __asm__ volatile ("dsb\n\tisb" ::: "memory");
#endif
    for (uint32_t *s = __data_load, *d = __data_start; d < __data_end;) *d++ = *s++;
    for (uint32_t *d = __bss_start; d < __bss_end;) *d++ = 0;
    main();
    for (;;) {}
}
