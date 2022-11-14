// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

class chip_sw_rv_dm_access_after_wakeup_vseq extends chip_sw_base_vseq;
  `uvm_object_utils(chip_sw_rv_dm_access_after_wakeup_vseq)

  `uvm_object_new

  virtual task pre_start();
    super.pre_start();
    // Set up JTAG RV_DM TAP.
    cfg.chip_vif.tap_straps_if.drive(JtagTapRvDm);
    cfg.m_jtag_riscv_agent_cfg.is_rv_dm = 1;
    // Release power button.
    cfg.chip_vif.pwrb_in_if.drive(1'b1);
  endtask : pre_start


  task write_and_readback_check(uvm_reg_data_t exp_data, string error_suffix);
    csr_wr(
      .ptr(cfg.jtag_dmi_ral.progbuf[0]),
      .value(exp_data),
      .blocking(1),
      .path(UVM_FRONTDOOR)
    );
    readback_check(exp_data, error_suffix);
  endtask : write_and_readback_check

  task readback_check(uvm_reg_data_t exp_data, string error_suffix);
    uvm_reg_data_t obs_data;
    csr_rd(
      .ptr(cfg.jtag_dmi_ral.progbuf[0]),
      .value(obs_data),
      .blocking(1),
      .path(UVM_FRONTDOOR)
    );
    `DV_CHECK_EQ(obs_data, exp_data,
      {"RV_DM DMI progbuf[0] does not contain the expected value ", error_suffix})
  endtask : readback_check

  virtual task body();
    uint           timeout_long       = 10_000_000;
    uint           timeout_short      = 1_000_000;
    bit [7:0]      software_barrier[] = '{0};
    uvm_reg_data_t exp_data = $urandom();

    super.body();

    `uvm_info(`gfn, "Started RV DM Sequence", UVM_LOW)

    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Handover to sequence.");,
                 "Timed out waiting first handover.", timeout_long)

    // Attempt to activate RV_DM via JTAG.
    csr_wr(.ptr(cfg.jtag_dmi_ral.dmcontrol.dmactive), .value(1), .blocking(1), .predict(1));
    cfg.clk_rst_vif.wait_clks(5);

    write_and_readback_check(exp_data, "straight after write.");

    // Allow the software to continue execution.
    software_barrier[0] = 1;
    `uvm_info(`gfn, "Handing back to software.", UVM_LOW)
    sw_symbol_backdoor_overwrite("kSoftwareBarrier", software_barrier);

    // Wait for the software to fall asleep.
    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Sleeping... ZZZZZZ");,
                 "Timed out waiting for device to sleep", timeout_short)

    // Press the power button to wake up the device.
    `uvm_info(`gfn, "Pushing power button.", UVM_LOW)
    cfg.chip_vif.pwrb_in_if.drive(1'b0); // pressing power button

    // Wait for the software to wake up.
    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Waking up.");,
                 "Timed out waiting for device to wakeup", timeout_short)

    `uvm_info(`gfn, "Releasing power button.", UVM_LOW)
    cfg.chip_vif.pwrb_in_if.drive(1'b1); // releasing power button

    readback_check(exp_data, "after sleep.");

    // Allow the software to continue execution.
    software_barrier[0] = 2;
    `uvm_info(`gfn, "Handing back to software.", UVM_LOW)
    sw_symbol_backdoor_overwrite("kSoftwareBarrier", software_barrier);

  endtask : body

endclass : chip_sw_rv_dm_access_after_wakeup_vseq
