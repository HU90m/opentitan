#include "sw/device/silicon_creator/manuf/lib/sram_start.h"

  /**
   * The size of the `i_am_become_user` function in bytes.
   */
  .balign 4
  .global i_am_become_user
  .type i_am_become_user, @function
i_am_become_user:
  la t0, test_locations

  addi t1, t0, 4 * 4 // test_location + (4 * NUM_LOCATIONS)
1:
  ecall
  lw t2, 0(t0)
  sw t2, 0(t0)
  jalr   0(t0)

  .global exp_u_instr_acc_fault_ret
exp_u_instr_acc_fault_ret:
  add t0, t0, 4
  blt t0, t1, 1b


/*
  lw t2, 0(t1)
  sw t2, 0(t1)
  //jalr ra, t1, 0

  lw t2, 4(t1)
  sw t2, 4(t1)
  //jalr ra, t1, 4

  lw t2, 8(t1)
  sw t2, 8(t1)
  jalr ra, t1, 8

  lw t2, 12(t1)
  sw t2, 12(t1)
  jalr   12(t1)
  */

  .global sys_print
sys_print:
  ecall
  .size i_am_become_user, .-i_am_become_user

  .balign 4
  .global i_am_become_user_end
i_am_become_user_end:

  /**
   * Goodbye
   */
  .balign 4
  .global finish_test
  .type finish_test, @function
finish_test:
  li  sp, SRAM_MAGIC_SP_EXECUTION_DONE
  ebreak
  .size finish_test, .-finish_test
