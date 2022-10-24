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
  endtask

  virtual task body();
    uint           timeout_long  = 10_000_000;
    uint           timeout_short = 1_000_000;
    bit [7:0]      false[]       = '{0, 0, 0, 0};
    uvm_status_e   status;
    uvm_reg_data_t data;

    super.body();

    // Give the pin a default value.
    cfg.chip_vif.pinmux_wkup_if.set_pulldown_en(1'b1);

    `uvm_info(`gfn, "Started RV DM Sequence", UVM_LOW)

    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Handover to sequence.");,
                 "Timed out waiting first handover.", timeout_long)

    cfg.jtag_dmi_ral.progbuf[0].write(
      status,
      32'hDEAF,
      .path(UVM_FRONTDOOR),
      .parent(this),
      .fname(`__FILE__),
      .lineno(`__LINE__)
    );
    `DV_CHECK_EQ(status, UVM_IS_OK,
      "Could not write to the RV_DM DMI program buffer 0 register")

    `uvm_info(`gfn, "Handing back to software.", UVM_LOW)
    sw_symbol_backdoor_overwrite("kSequenceRunning", false);

    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Sleeping... ZZZZZZ");,
                 "Timed out waiting for device to sleep", timeout_short)

    `uvm_info(`gfn, "Pushing power button.", UVM_LOW)
    cfg.chip_vif.pwrb_in_if.drive(1'b0); // pressing power button

    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Waking up.");,
                 "Timed out waiting for device to wakeup", timeout_short)

    `uvm_info(`gfn, "Releasing power button.", UVM_LOW)
    cfg.chip_vif.pwrb_in_if.drive(1'b1); // releasing power button


    cfg.jtag_dmi_ral.progbuf[0].read(
      status,
      data,
      .path(UVM_FRONTDOOR),
      .parent(this),
      .fname(`__FILE__),
      .lineno(`__LINE__)
    );
    `DV_CHECK_EQ(status, UVM_IS_OK,
      "Could not read from the RV_DM DMI program buffer 0 register")
    `DV_CHECK_EQ(data, 'hDEAF,
      "RV_DM DMI program does not contain the expected value.")

    `uvm_info(`gfn, "Handing back to software.", UVM_LOW)
    sw_symbol_backdoor_overwrite("kSequenceRunning", false);

  endtask : body

endclass : chip_sw_rv_dm_access_after_wakeup_vseq
