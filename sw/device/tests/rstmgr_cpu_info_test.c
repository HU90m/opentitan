// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sw/device/lib/base/abs_mmio.h"
#include "sw/device/lib/base/macros.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_pwrmgr.h"
#include "sw/device/lib/dif/dif_rstmgr.h"
#include "sw/device/lib/dif/dif_rv_core_ibex.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/testing/aon_timer_testutils.h"
#include "sw/device/lib/testing/rstmgr_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_isrs.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

OTTF_DEFINE_TEST_CONFIG();

/**
 * RSTMGR CPU INFO TEST
 *  This test creates a double fault by accessing a register with
 *  a non-existing address.
 *  After a single fault and then a double fault,
 *  the dut gets reset by the watch dog bite,
 *  and the test collects / checks the cpu_info from the rstmgr.
 */

// Unmapped Addresses
static const uint8_t *kIllegalAddr0 = (uint8_t *)0xA0A0DEAFu;
static const uint8_t *kIllegalAddr1 = (uint8_t *)0xA041FFF0u;
static const uint8_t *kIllegalAddr2 = (uint8_t *)0xA0003618u;
#define kCpuDumpSize 8

// Declaring the labels used to calculate the expected current and next pc
// after a double fault.
extern const uint32_t _ottf_interrupt_vector, handler_exception;

// The labels to points in the code of which the memory address is needed.
extern const char kSingleFaultAddrLower[], kSingleFaultAddrUpper[],
    kSingleFaultAddrCurrentPc[], kSingleFaultAddrNextPc[],
    kDoubleFaultFirstAddrLower[], kDoubleFaultFirstAddrUpper[],
    kDoubleFaultSecondAddrLower[], kDoubleFaultSecondAddrUpper[];

// A handle to the reset manager.
static dif_rstmgr_t rstmgr;

/**
 * This variable is used to ensure loads from an address aren't optimised out.
 */
volatile uint8_t addr_val;

/**
 * When true, the exception handler will trigger another fault,
 * causing a double fault,
 * otherwise it triggers a software reset.
 */
static bool double_fault;

/**
 * Overrides the default OTTF exception handler.
 */
void ottf_exception_handler(void) {
  if (double_fault) {
    OT_ADDRESSABLE_LABEL(kDoubleFaultSecondAddrLower);
    addr_val = *kIllegalAddr2;
    OT_ADDRESSABLE_LABEL(kDoubleFaultSecondAddrUpper);
  } else {
    CHECK_DIF_OK(dif_rstmgr_software_device_reset(&rstmgr));
    // Write to `addr_val` so that the 'last data access' address is
    // a known value (the address of addr_val).
    addr_val = 1;
    OT_ADDRESSABLE_LABEL(kSingleFaultAddrCurrentPc);
    wait_for_interrupt();  // wait for the reset
    OT_ADDRESSABLE_LABEL(kSingleFaultAddrNextPc);
    addr_val = 2;
  }
  LOG_ERROR(
      "This should be unreachable; a reset or another fault should have "
      "occured.");
}

/**
 * Gets, parses and returns the cpu info crash dump.
 *
 * @param ibex A handle to the ibex.
 * @return The cpu info crash dump.
 */
static dif_rv_core_ibex_crash_dump_info_t get_dump(
    const dif_rv_core_ibex_t *ibex) {
  size_t size_read;
  dif_rstmgr_cpu_info_dump_segment_t dump[DIF_RSTMGR_CPU_INFO_MAX_SIZE];

  CHECK_DIF_OK(dif_rstmgr_cpu_info_dump_read(
      &rstmgr, dump, DIF_RSTMGR_CPU_INFO_MAX_SIZE, &size_read));
  CHECK(size_read == kCpuDumpSize,
        "The observed cpu info dump's size was %d, "
        "but it was expected to be %d",
        size_read, kCpuDumpSize);

  dif_rv_core_ibex_crash_dump_info_t output;
  CHECK_DIF_OK(
      dif_rv_core_ibex_parse_crash_dump(ibex, dump, size_read, &output));
  return output;
}

/**
 * Checks the 'current' section of the cpu info dump against the given expected
 * values.
 *
 * @param obs_dump The cpu info crash dump.
 * @param last_exception_addr The address of the last exception.
 * @param mpec_lower The expected range lower bound of the last exception PC.
 * @param mpec_upper The expected range upper bound of the last exception PC.
 * @param last_data_addr The address of the last data access.
 * @param curr_pc The current PC.
 * @param next_pc The last PC.
 */
static void check_current_values(dif_rv_core_ibex_crash_dump_info_t *obs_dump,
                                 uint32_t last_exception_addr,
                                 uint32_t mpec_lower, uint32_t mpec_upper,
                                 uint32_t last_data_addr, uint32_t curr_pc,
                                 uint32_t next_pc) {
  dif_rv_core_ibex_crash_dump_state_t observed = obs_dump->fault_state;

  CHECK(last_exception_addr == observed.mtval,
        "Last Exception Access Addr: Expected 0x%x != Observed 0x%x",
        last_exception_addr, observed.mtval);
  CHECK(last_data_addr == observed.mdaa,
        "Last Data Access Addr: Expected 0x%x != Observed 0x%x", last_data_addr,
        observed.mdaa);
  CHECK(curr_pc == observed.mcpc, "Current PC: Expected 0x%x != Observed 0x%x",
        curr_pc, observed.mcpc);
  CHECK(next_pc == observed.mnpc, "Next PC: Expected 0x%x != Observed 0x%x",
        next_pc, observed.mnpc);
  CHECK(
      mpec_lower <= observed.mpec && observed.mpec < mpec_upper,
      "The Observed MPEC, 0x%x, was not in the expected range of [0x%x, 0x%x)",
      observed.mpec, mpec_lower, mpec_upper);
}

/**
 * Checks the 'previous' section of the cpu info dump against the given
 * expected values.
 *
 * @param obs_dump The cpu info crash dump.
 * @param last_exception_addr The address of the last exception.
 * @param mpec_lower The expected range lower bound of the last exception PC.
 * @param mpec_upper The expected range upper bound of the last exception PC.
 */
static void check_previous_values(dif_rv_core_ibex_crash_dump_info_t *obs_dump,
                                  uint32_t last_exception_addr,
                                  uint32_t mpec_lower, uint32_t mpec_upper) {
  dif_rv_core_ibex_previous_crash_dump_state_t observed =
      obs_dump->previous_fault_state;
  CHECK(last_exception_addr == observed.mtval,
        "Last Exception Access Addr: Expected 0x%x != Observed 0x%x",
        last_exception_addr, observed.mtval);
  CHECK(mpec_lower <= observed.mpec && observed.mpec < mpec_upper,
        "The Observed Previous MPEC, 0x%x, "
        "was not in the expected range of [0x%x, 0x%x)",
        observed.mpec, mpec_lower, mpec_upper);
}

bool test_main(void) {
  dif_rv_core_ibex_crash_dump_info_t dump;

  dif_aon_timer_t aon_timer;
  dif_pwrmgr_t pwrmgr;
  dif_rv_core_ibex_t ibex;

  // Initialize Handles
  CHECK_DIF_OK(dif_rstmgr_init(
      mmio_region_from_addr(TOP_EARLGREY_RSTMGR_AON_BASE_ADDR), &rstmgr));
  CHECK_DIF_OK(dif_aon_timer_init(
      mmio_region_from_addr(TOP_EARLGREY_AON_TIMER_AON_BASE_ADDR), &aon_timer));
  CHECK_DIF_OK(dif_pwrmgr_init(
      mmio_region_from_addr(TOP_EARLGREY_PWRMGR_AON_BASE_ADDR), &pwrmgr));
  CHECK_DIF_OK(dif_rv_core_ibex_init(
      mmio_region_from_addr(TOP_EARLGREY_RV_CORE_IBEX_CFG_BASE_ADDR), &ibex));

  switch (rstmgr_testutils_reason_get()) {
    case kDifRstmgrResetInfoPor:  // The first power-up.
      LOG_INFO("Triggering single fault.");

      // Enable cpu info.
      CHECK_DIF_OK(dif_rstmgr_cpu_info_set_enabled(&rstmgr, kDifToggleEnabled));

      double_fault = false;
      OT_ADDRESSABLE_LABEL(kSingleFaultAddrLower);
      addr_val = *kIllegalAddr0;
      OT_ADDRESSABLE_LABEL(kSingleFaultAddrUpper);
      LOG_ERROR(
          "This should be unreachable; a single fault should have occured.");
      break;

    case kDifRstmgrResetInfoSw:  // The power-up after the single fault.
      LOG_INFO("Checking CPU info dump after single fault.");

      dump = get_dump(&ibex);

      CHECK(
          dump.double_fault == kDifToggleDisabled,
          "CPU Info dump shows a double fault after experiencing only a single "
          "fault.");

      check_current_values(&dump,
                           /*last_exception_addr=*/(uint32_t)kIllegalAddr0,
                           /*mpec_lower=*/(uint32_t)kSingleFaultAddrLower,
                           /*mpec_upper=*/(uint32_t)kSingleFaultAddrUpper,
                           /*last_data_addr=*/(uint32_t)&addr_val,
                           /*curr_pc=*/(uint32_t)kSingleFaultAddrCurrentPc,
                           /*next_pc=*/(uint32_t)kSingleFaultAddrNextPc);

      LOG_INFO("Setting up watch dog and triggering a double fault.");
      uint32_t bark_cycles = aon_timer_testutils_get_aon_cycles_from_us(100);
      uint32_t bite_cycles = aon_timer_testutils_get_aon_cycles_from_us(100);

      // Set wdog as a reset source.
      CHECK_DIF_OK(dif_pwrmgr_set_request_sources(
          &pwrmgr, kDifPwrmgrReqTypeReset, kDifPwrmgrResetRequestSourceTwo,
          kDifToggleEnabled));
      // Setup the wdog bark and bite timeouts.
      aon_timer_testutils_watchdog_config(&aon_timer, bark_cycles, bite_cycles,
                                          false);
      // Enable cpu info
      CHECK_DIF_OK(dif_rstmgr_cpu_info_set_enabled(&rstmgr, kDifToggleEnabled));

      double_fault = true;
      OT_ADDRESSABLE_LABEL(kDoubleFaultFirstAddrLower);
      addr_val = *kIllegalAddr1;
      OT_ADDRESSABLE_LABEL(kDoubleFaultFirstAddrUpper);
      LOG_ERROR(
          "This should be unreachable; a double fault should have occured.");
      break;

    case kDifRstmgrResetInfoWatchdog:  // The power-up after the double fault.
      LOG_INFO("Checking CPU info dump after double fault.");

      dump = get_dump(&ibex);

      CHECK(dump.double_fault == kDifToggleEnabled,
            "CPU Info dump doesn't show a double fault has happened.");

      // The current behaviour after a double fault is to capture, in the CPU
      // info dump, the interrupt vector below the one which was taken to jump
      // to the exception handler as the current PC and the start of the
      // exception handler as the next PC. This feels wrong. However, with a
      // lack of a clear definition of what these values should contain, the
      // test enforces this behaviour so that regressions can be caught. This
      // behaviour will be double checked at a later date.
      uint32_t curr_pc = (uint32_t)&_ottf_interrupt_vector + 4;
      uint32_t next_pc = (uint32_t)&handler_exception;

      check_current_values(&dump,
                           /*last_exception_addr=*/(uint32_t)kIllegalAddr2,
                           /*mpec_lower=*/(uint32_t)kDoubleFaultSecondAddrLower,
                           /*mpec_upper=*/(uint32_t)kDoubleFaultSecondAddrUpper,
                           /*last_data_addr=*/(uint32_t)kIllegalAddr2,
                           /*curr_pc=*/curr_pc,
                           /*next_pc=*/next_pc);
      check_previous_values(
          &dump,
          /*last_exception_addr=*/(uint32_t)kIllegalAddr1,
          /*mpec_lower=*/(uint32_t)kDoubleFaultFirstAddrLower,
          /*mpec_upper=*/(uint32_t)kDoubleFaultFirstAddrUpper);
      break;
    default:
      LOG_ERROR("Device was reset by an unexpected source.");
      break;
  }

  // Turn off the AON timer hardware completely before exiting.
  aon_timer_testutils_shutdown(&aon_timer);
  return true;
}
