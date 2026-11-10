.syntax unified
.cpu cortex-m3
.thumb

.global g_pfnVectors
.global Reset_Handler

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr r0, =_sidata
  ldr r1, =_sdata
  ldr r2, =_edata
copy_data:
  cmp r1, r2
  bcc copy_data_loop
  b zero_bss
copy_data_loop:
  ldr r3, [r0], #4
  str r3, [r1], #4
  b copy_data
zero_bss:
  ldr r1, =_sbss
  ldr r2, =_ebss
  movs r3, #0
zero_bss_loop:
  cmp r1, r2
  bcc zero_bss_store
  b call_main
zero_bss_store:
  str r3, [r1], #4
  b zero_bss_loop
call_main:
  bl main
hang:
  b hang

.weak Default_Handler
.type Default_Handler, %function
Default_Handler:
  b Default_Handler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  .rept 60
  .word Default_Handler
  .endr

.weak NMI_Handler
.thumb_set NMI_Handler,Default_Handler
.weak HardFault_Handler
.thumb_set HardFault_Handler,Default_Handler
.weak MemManage_Handler
.thumb_set MemManage_Handler,Default_Handler
.weak BusFault_Handler
.thumb_set BusFault_Handler,Default_Handler
.weak UsageFault_Handler
.thumb_set UsageFault_Handler,Default_Handler
.weak SVC_Handler
.thumb_set SVC_Handler,Default_Handler
.weak DebugMon_Handler
.thumb_set DebugMon_Handler,Default_Handler
.weak PendSV_Handler
.thumb_set PendSV_Handler,Default_Handler
.weak SysTick_Handler
.thumb_set SysTick_Handler,Default_Handler
