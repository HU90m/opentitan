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

  task write_and_readback_check(uvm_reg_data_t data, string error_suffix);
    uvm_status_e   status;

    // Write a value to the DMI program buffer.
    cfg.jtag_dmi_ral.progbuf[0].write(
      status,
      data,
      .path(UVM_FRONTDOOR),
      .parent(this),
      .fname(`__FILE__),
      .lineno(`__LINE__)
    );
    `DV_CHECK_EQ(status, UVM_IS_OK,
      {"Could not write to the RV_DM DMI program buffer 0 register ", error_suffix})

    readback_check(data, error_suffix);

  endtask : write_and_readback_check

  task readback_check(uvm_reg_data_t data, string error_suffix);
    uvm_status_e   status;
    // Read the program buffer and check it still has the value that was
    // written to it.
    cfg.jtag_dmi_ral.progbuf[0].read(
      status,
      data,
      .path(UVM_FRONTDOOR),
      .parent(this),
      .fname(`__FILE__),
      .lineno(`__LINE__)
    );
    `DV_CHECK_EQ(status, UVM_IS_OK,
      {"Could not read from the RV_DM DMI program buffer 0 register ", error_suffix})
    `DV_CHECK_EQ(data, 'hDEAF,
      {"RV_DM DMI program does not contain the expected value. ", error_suffix})
  endtask : readback_check

  virtual task body();
    uint           timeout_long       = 10_000_000;
    uint           timeout_short      = 1_000_000;
    bit [7:0]      software_barrier[] = '{0};

    super.body();

    `uvm_info(`gfn, "Started RV DM Sequence", UVM_LOW)

    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Handover to sequence.");,
                 "Timed out waiting first handover.", timeout_long)

    // Attempt to activate RV_DM via JTAG.
    csr_wr(.ptr(cfg.jtag_dmi_ral.dmcontrol.dmactive), .value(1), .blocking(1), .predict(1));
    cfg.clk_rst_vif.wait_clks(5);

    write_and_readback_check(32'hDEAF, "first");

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

    readback_check(32'hDEAF, "after sleep");

    // Allow the software to continue execution.
    software_barrier[0] = 2;
    `uvm_info(`gfn, "Handing back to software.", UVM_LOW)
    sw_symbol_backdoor_overwrite("kSoftwareBarrier", software_barrier);

  endtask : body

endclass : chip_sw_rv_dm_access_after_wakeup_vseq
