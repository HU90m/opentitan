// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sw/device/lib/base/abs_mmio.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_pwrmgr.h"
#include "sw/device/lib/dif/dif_rstmgr.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/testing/aon_timer_testutils.h"
#include "sw/device/lib/testing/flash_ctrl_testutils.h"
#include "sw/device/lib/testing/rstmgr_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_isrs.h"
#include "sw/device/lib/testing/test_framework/ottf_macros.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

OTTF_DEFINE_TEST_CONFIG();

/**
 * RSTMGR CPU INFO TEST
 *  This test creates a double fault by accessing a register with
 *  a non-existing address.
 *  After the double fault, the dut gets reset by the watch dog bite,
 *  and the test collects / checks the cpu_info from the rstmgr.
 */

// non existing address
#define kIllegalAddr1 0x4041FFF0u
#define kIllegalAddr2 0x40003618u
#define kCpuDumpSize 8

// Declaring the labels used to calculate the expected current and next pc.
extern const uint32_t _ottf_interrupt_vector, handler_exception;

/**
 * CPU info dump index map:
 *     0: current.exception_addr
 *     1: current.exception_pc
 *     2: current.last_data_addr
 *     3: current.next_pc
 *     4: current.pc
 *     5: previous.exception_addr
 *     6: previous.exception_pc
 *     7: previous_valid
 */
enum {
  kCpuDumpIdxCurrentExceptionAddr = 0,
  kCpuDumpIdxCurrentExceptionPc = 1,
  kCpuDumpIdxCurrentLastDataAddr = 2,
  kCpuDumpIdxCurrentNextPc = 3,
  kCpuDumpIdxCurrentPc = 4,
  kCpuDumpIdxPreviousExceptionAddr = 5,
  kCpuDumpIdxPreviousExceptionPc = 6,
  kCpuDumpIdxPreviousValid = 7,
};

/**
 * Reserve expected cpu dump area in flash
 */
OT_SECTION(".non_volatile_scratch")
dif_rstmgr_cpu_info_dump_segment_t exp_dump[kCpuDumpSize];

static dif_flash_ctrl_state_t flash_ctrl;

// Count number of faluts
static volatile uint32_t global_error_cnt;

// Access non-existing address
// Each call will create a fault
static void read_error(void) {
  uint32_t addr;
  global_error_cnt++;

  if (global_error_cnt == 1) {
    addr = kIllegalAddr1;
  } else {
    LOG_INFO("double fault");
    addr = kIllegalAddr2;
  }
  // I can't add a new variable to call mmio_
  // because mmio call will never be returned.
  // Use current variable, just to avoid unused error.
  addr = mmio_region_read32(mmio_region_from_addr(addr), 0);
}

/**
 * Overrides the default OTTF exception handler.
 */
void ottf_exception_handler(void) {
  dif_rstmgr_cpu_info_dump_segment_t temp_dump[kCpuDumpSize];

  // The exception address ends up being the same since both are
  // are referencing the same read function
  temp_dump[kCpuDumpIdxCurrentExceptionPc] =
      (dif_rstmgr_cpu_info_dump_segment_t)ibex_mepc_read();
  temp_dump[kCpuDumpIdxCurrentExceptionAddr] = kIllegalAddr2;
  temp_dump[kCpuDumpIdxCurrentLastDataAddr] = kIllegalAddr2;
  temp_dump[kCpuDumpIdxPreviousExceptionPc] =
      temp_dump[kCpuDumpIdxCurrentExceptionPc];
  temp_dump[kCpuDumpIdxPreviousExceptionAddr] = kIllegalAddr1;
  temp_dump[kCpuDumpIdxPreviousValid] = 1;

  // The current behaviour after a double fault is to capture, in the CPU info
  // dump, the interrupt vector below the one which was taken to jump to the
  // exception handler as the current PC and the start of the exception handler
  // as the next PC. This feels wrong. However, with a lack of a clear
  // definition of what these values should contain, the test enforces this
  // behaviour so that regressions can be caught. This behaviour will be double
  // checked at a later date.
  temp_dump[kCpuDumpIdxCurrentPc] =
      (dif_rstmgr_cpu_info_dump_segment_t)&_ottf_interrupt_vector + 4;
  temp_dump[kCpuDumpIdxCurrentNextPc] =
      (dif_rstmgr_cpu_info_dump_segment_t)&handler_exception;

  CHECK(flash_ctrl_testutils_write(
      &flash_ctrl, (uintptr_t)exp_dump - TOP_EARLGREY_FLASH_CTRL_MEM_BASE_ADDR,
      0, temp_dump, kDifFlashCtrlPartitionTypeData, kCpuDumpSize));

  for (size_t i = 0; i < kCpuDumpSize; ++i) {
    dif_rstmgr_cpu_info_dump_segment_t rdata = exp_dump[i];
    LOG_INFO("Expected dump:%d: 0x%x", i, rdata);
  }

  read_error();
}

bool test_main(void) {
  dif_rstmgr_t rstmgr;
  dif_aon_timer_t aon_timer;
  dif_pwrmgr_t pwrmgr;

  CHECK_DIF_OK(dif_rstmgr_init(
      mmio_region_from_addr(TOP_EARLGREY_RSTMGR_AON_BASE_ADDR), &rstmgr));
  CHECK_DIF_OK(dif_aon_timer_init(
      mmio_region_from_addr(TOP_EARLGREY_AON_TIMER_AON_BASE_ADDR), &aon_timer));
  CHECK_DIF_OK(dif_pwrmgr_init(
      mmio_region_from_addr(TOP_EARLGREY_PWRMGR_AON_BASE_ADDR), &pwrmgr));
  // Initialize flash_ctrl
  CHECK_DIF_OK(dif_flash_ctrl_init_state(
      &flash_ctrl,
      mmio_region_from_addr(TOP_EARLGREY_FLASH_CTRL_CORE_BASE_ADDR)));

  // Enable flash access
  flash_ctrl_testutils_default_region_access(&flash_ctrl,
                                             /*rd_en*/ true,
                                             /*prog_en*/ true,
                                             /*erase_en*/ true,
                                             /*scramble_en*/ false,
                                             /*ecc_en*/ false,
                                             /*he_en*/ false);

  dif_rstmgr_reset_info_bitfield_t rst_info;
  rst_info = rstmgr_testutils_reason_get();

  if (rst_info == kDifRstmgrResetInfoPor) {
    LOG_INFO("Booting for the first time, setting wdog");

    global_error_cnt = 0;
    uint32_t bark_cycles = aon_timer_testutils_get_aon_cycles_from_us(100);
    uint32_t bite_cycles = aon_timer_testutils_get_aon_cycles_from_us(100);

    // Set wdog as a reset source.
    CHECK_DIF_OK(dif_pwrmgr_set_request_sources(&pwrmgr, kDifPwrmgrReqTypeReset,
                                                kDifPwrmgrResetRequestSourceTwo,
                                                kDifToggleEnabled));
    // Setup the wdog bark and bite timeouts.
    aon_timer_testutils_watchdog_config(&aon_timer, bark_cycles, bite_cycles,
                                        false);
    // Enable cpu info
    CHECK_DIF_OK(dif_rstmgr_cpu_info_set_enabled(&rstmgr, kDifToggleEnabled));
    read_error();
  } else {
    LOG_INFO("Comes back after bite");
    rstmgr_testutils_compare_cpu_info(&rstmgr, exp_dump, kCpuDumpSize);
  }

  // Turn off the AON timer hardware completely before exiting.
  aon_timer_testutils_shutdown(&aon_timer);
  return true;
}
