// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

class chip_sw_rv_dm_access_after_wakeup_vseq extends chip_sw_base_vseq;
  `uvm_object_utils(chip_sw_rv_dm_access_after_wakeup_vseq)

  `uvm_object_new

  virtual task pre_start();
    super.pre_start();
    cfg.chip_vif.pwrb_in_if.drive(1'b1);    // off
  endtask

  virtual task body();
    uint                timeout_long = 10_000_000;
    bit [7:0] false[] = '{0, 0, 0, 0};


    //uint                timeout_short = 1_000_000;
    super.body();

    // Give the pin a default value.
    cfg.chip_vif.pinmux_wkup_if.set_pulldown_en(1'b1);

    `uvm_info(`gfn, "Started RV DM Sequence", UVM_LOW)

    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Handover to sequence.");,
                 "Timed out waiting first handover.", timeout_long)



    `uvm_info(`gfn, "Handing back to software.", UVM_LOW)
    sw_symbol_backdoor_overwrite("kSequenceRunning", false);

    // @6.3ms
    `DV_SPINWAIT(wait(cfg.sw_logger_vif.printed_log == "Control Back.");,
                 "Timed out waiting for acknowledgement of hand-back", timeout_long)

 endtask : body

endclass : chip_sw_rv_dm_access_after_wakeup_vseq
