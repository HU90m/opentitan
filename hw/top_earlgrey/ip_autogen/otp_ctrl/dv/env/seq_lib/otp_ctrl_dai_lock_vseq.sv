// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// otp_ctrl_dai_lock_vseq is developed to read/write lock DAI interface by partitions, and request
// read/write access to check if correct status and error code is triggered

// Partition's legal range covers offset to digest addresses, dai_rd/dai_wr function will
// randomize the address based on the granularity.
`define PART_ADDR_RANGE(i) \
    {[PartInfo[``i``].offset : (PartInfo[``i``].offset + PartInfo[``i``].size - 8)]}

class otp_ctrl_dai_lock_vseq extends otp_ctrl_smoke_vseq;
  `uvm_object_utils(otp_ctrl_dai_lock_vseq)

  `uvm_object_new

  // enable access_err for each cycle
  constraint no_access_err_c {access_locked_parts == 1;}

  constraint num_trans_c {
    num_trans  inside {[1:10]};
    num_dai_op inside {[1:50]};
  }

  // the LC partition is always the last one
  constraint partition_index_c {part_idx inside {[0:LifeCycleIdx]};}

  constraint dai_wr_legal_addr_c {
    if (part_idx == VendorTestIdx) {
      dai_addr inside `PART_ADDR_RANGE(VendorTestIdx);
    }
    if (part_idx == CreatorSwCfgIdx) {
      dai_addr inside `PART_ADDR_RANGE(CreatorSwCfgIdx);
    }
    if (part_idx == OwnerSwCfgIdx) {
      dai_addr inside `PART_ADDR_RANGE(OwnerSwCfgIdx);
    }
    if (part_idx == RotCreatorAuthCodesignIdx) {
      dai_addr inside `PART_ADDR_RANGE(RotCreatorAuthCodesignIdx);
    }
    if (part_idx == RotCreatorAuthStateIdx) {
      dai_addr inside `PART_ADDR_RANGE(RotCreatorAuthStateIdx);
    }
    if (part_idx == HwCfg0Idx) {
      dai_addr inside `PART_ADDR_RANGE(HwCfg0Idx);
    }
    if (part_idx == HwCfg1Idx) {
      dai_addr inside `PART_ADDR_RANGE(HwCfg1Idx);
    }
    if (part_idx == Secret0Idx) {
      dai_addr inside `PART_ADDR_RANGE(Secret0Idx);
    }
    if (part_idx == Secret1Idx) {
      dai_addr inside `PART_ADDR_RANGE(Secret1Idx);
    }
    if (part_idx == Secret2Idx) {
      dai_addr inside `PART_ADDR_RANGE(Secret2Idx);
    }
    if (part_idx == LifeCycleIdx) {
      if (write_unused_addr) {
        dai_addr inside {[PartInfo[LifeCycleIdx].offset : {OTP_ADDR_WIDTH{1'b1}}]};
      } else {
        dai_addr inside `PART_ADDR_RANGE(LifeCycleIdx);
      }
    }
    solve part_idx before dai_addr;
  }

  constraint dai_wr_digests_c {
    {dai_addr[TL_AW-1:2], 2'b0} dist {
      {
        VendorTestDigestOffset,
        CreatorSwCfgDigestOffset,
        OwnerSwCfgDigestOffset,
        RotCreatorAuthCodesignDigestOffset,
        RotCreatorAuthStateDigestOffset,
        HwCfg0DigestOffset,
        HwCfg1DigestOffset,
        Secret0DigestOffset,
        Secret1DigestOffset,
        Secret2DigestOffset
      } :/ 1,
      [VendorTestOffset : '1] :/ 9
    };
  }

  virtual task pre_start();
    super.pre_start();
    is_valid_dai_op = 0;
  endtask

  virtual task dut_init(string reset_kind = "HARD");
    super.dut_init(reset_kind);
    if ($urandom_range(0, 1)) begin
      cfg.otp_ctrl_vif.drive_lc_creator_seed_sw_rw_en(get_rand_lc_tx_val(.t_weight(0)));
    end
    if ($urandom_range(0, 1)) begin
      cfg.otp_ctrl_vif.drive_lc_owner_seed_sw_rw_en(get_rand_lc_tx_val(.t_weight(0)));
    end
  endtask

endclass

`undef PART_ADDR_RANGE
