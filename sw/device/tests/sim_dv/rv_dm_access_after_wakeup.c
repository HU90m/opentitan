// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sw/device/lib/dif/dif_pinmux.h"
#include "sw/device/lib/dif/dif_pwrmgr.h"
#include "sw/device/lib/dif/dif_rv_plic.h"
#include "sw/device/lib/dif/dif_sysrst_ctrl.h"
#include "sw/device/lib/runtime/irq.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/testing/aon_timer_testutils.h"
#include "sw/device/lib/testing/pwrmgr_testutils.h"
#include "sw/device/lib/testing/rv_plic_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"
#include "sw/device/lib/testing/test_framework/status.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"
#include "pwrmgr_regs.h"

/*
 * RV_DM access after wakeup test.
 */

OTTF_DEFINE_TEST_CONFIG();

// This location will be update from SV to contain the expected alert.
static volatile uint32_t kSequenceRunning;

dif_rv_plic_t rv_plic;

/**
 * External interrupt handler.
 */
void ottf_external_isr(void) {
  LOG_INFO("In external ISR");
  dif_rv_plic_irq_id_t plic_irq_id;
  CHECK_DIF_OK(dif_rv_plic_irq_claim(&rv_plic, kTopEarlgreyPlicTargetIbex0,
                                     &plic_irq_id));
}

bool test_main(void) {
  // Enable global and external IRQ at Ibex.
  irq_global_ctrl(true);
  irq_external_ctrl(true);

  dif_pinmux_t pinmux;
  dif_pwrmgr_t pwrmgr;
  dif_sysrst_ctrl_t sysrst_ctrl;

  CHECK_DIF_OK(dif_pinmux_init(
      mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR), &pinmux));
  CHECK_DIF_OK(dif_pwrmgr_init(
      mmio_region_from_addr(TOP_EARLGREY_PWRMGR_AON_BASE_ADDR), &pwrmgr));
  CHECK_DIF_OK(dif_rv_plic_init(
      mmio_region_from_addr(TOP_EARLGREY_RV_PLIC_BASE_ADDR), &rv_plic));
  CHECK_DIF_OK(dif_sysrst_ctrl_init(
      mmio_region_from_addr(TOP_EARLGREY_SYSRST_CTRL_AON_BASE_ADDR),
      &sysrst_ctrl));

  kSequenceRunning = true;
  LOG_INFO("Handover to sequence.");
  while (!kSequenceRunning) {
  }

  // Enable all the AON interrupts used in this test.
  rv_plic_testutils_irq_range_enable(&rv_plic, kTopEarlgreyPlicTargetIbex0,
                                     kTopEarlgreyPlicIrqIdPwrmgrAonWakeup,
                                     kTopEarlgreyPlicIrqIdPwrmgrAonWakeup);

  // Enable pwrmgr interrupt
  CHECK_DIF_OK(dif_pwrmgr_irq_set_enabled(&pwrmgr, kDifPwrmgrIrqWakeup,
                                          kDifToggleEnabled));

  // Set up power button as wake up source
  dif_sysrst_ctrl_input_change_config_t config = {
      .input_changes = kDifSysrstCtrlInputPowerButtonH2L,
      .debounce_time_threshold = 1,  // 5us
  };
  CHECK_DIF_OK(
      dif_sysrst_ctrl_input_change_detect_configure(&sysrst_ctrl, config));
  CHECK_DIF_OK(dif_pinmux_input_select(
      &pinmux, kTopEarlgreyPinmuxPeripheralInSysrstCtrlAonPwrbIn,
      kTopEarlgreyPinmuxInselIor13));

  // Put to sleep
  dif_pwrmgr_domain_config_t cfg;
  CHECK_DIF_OK(dif_pwrmgr_get_domain_config(&pwrmgr, &cfg));
  cfg = cfg & (kDifPwrmgrDomainOptionIoClockInLowPower |
               kDifPwrmgrDomainOptionUsbClockInLowPower |
               kDifPwrmgrDomainOptionUsbClockInActivePower) |
        kDifPwrmgrDomainOptionMainPowerInLowPower;

  pwrmgr_testutils_enable_low_power(&pwrmgr, kDifPwrmgrWakeupRequestSourceOne,
                                    cfg);

  LOG_INFO("Sleeping... ZZZZZZ");
  wait_for_interrupt();
  LOG_INFO("Waking up.");

  // Clean up wakeup source after sleep.
  CHECK_DIF_OK(dif_sysrst_ctrl_ulp_wakeup_clear_status(&sysrst_ctrl));

  return true;
}
