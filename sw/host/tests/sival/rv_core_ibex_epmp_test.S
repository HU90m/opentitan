#include "sw/device/silicon_creator/manuf/lib/sram_start.h"

  /**
   * The size of the `i_am_become_user` function in bytes.
   */
  .balign 4
  .global i_am_become_user
  .type i_am_become_user, @function
i_am_become_user:
  //call ottf_load_store_fault_handler
  //call finish_test
  li t0, 1
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
