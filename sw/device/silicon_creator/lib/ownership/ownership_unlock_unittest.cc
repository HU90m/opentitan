// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sw/device/silicon_creator/lib/ownership/ownership_unlock.h"

#include <stdint.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "sw/device/lib/base/global_mock.h"
#include "sw/device/lib/base/hardened.h"
#include "sw/device/silicon_creator/lib/boot_data.h"
#include "sw/device/silicon_creator/lib/boot_svc/mock_boot_svc_header.h"
#include "sw/device/silicon_creator/lib/drivers/mock_hmac.h"
#include "sw/device/silicon_creator/lib/drivers/mock_lifecycle.h"
#include "sw/device/silicon_creator/lib/drivers/mock_rnd.h"
#include "sw/device/silicon_creator/lib/error.h"
#include "sw/device/silicon_creator/lib/ownership/datatypes.h"
#include "sw/device/silicon_creator/lib/ownership/mock_ownership_key.h"
#include "sw/device/silicon_creator/lib/ownership/owner_block.h"
#include "sw/device/silicon_creator/testing/rom_test.h"

namespace {
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

/*
 * The OwnershipUnlockTest fixture provides a pre-initialized bootdata and
 * boot_svc_msg with relevant values filled in.  The tests will have to
 * modify some of these values (ie: ownership_state or the unlock request
 * type) to test the various code-paths in the ownership unlock module.
 */
class OwnershipUnlockTest : public rom_test::RomTest {
 protected:
  boot_data_t bootdata_ = {
      .nonce = {1, 2},
      .ownership_state = kOwnershipStateLockedOwner,
  };
  boot_svc_msg_t message_ = {
      .ownership_unlock_req =
          {
              .header =
                  {
                      .type = kBootSvcOwnershipUnlockReqType,
                  },
              .din = {0, 0},
              .nonce = {1, 2},
          },
  };

  void SetUp() override {
    // Most tests operate with the owner configuration
    // in the Open mode.
    owner_page[0].update_mode = kOwnershipUpdateModeOpen;
  }

  rom_test::MockHmac hmac_;
  rom_test::MockRnd rnd_;
  rom_test::MockBootSvcHeader hdr_;
  rom_test::MockLifecycle lifecycle_;
  rom_test::MockOwnershipKey ownership_key_;
};

class OwnershipUnlockAnyStateTest
    : public OwnershipUnlockTest,
      public testing::WithParamInterface<ownership_state_t> {};

class OwnershipUnlockEndorsedStateTest
    : public OwnershipUnlockTest,
      public testing::WithParamInterface<ownership_state_t> {};

class OwnershipUnlockedUpdateStateTest
    : public OwnershipUnlockTest,
      public testing::WithParamInterface<ownership_state_t> {};

class OwnershipUnlockAbortValidStateTest
    : public OwnershipUnlockTest,
      public testing::WithParamInterface<ownership_state_t> {};

class OwnershipUnlockAbortInvalidStateTest
    : public OwnershipUnlockTest,
      public testing::WithParamInterface<ownership_state_t> {};

class OwnershipUnlockUpdateModesTest
    : public OwnershipUnlockTest,
      public testing::WithParamInterface<ownership_update_mode_t> {};

// Tests that a bad `unlock_mode` returns an Invalid Request.
TEST_F(OwnershipUnlockTest, BadUnlockMode) {
  message_.ownership_unlock_req.unlock_mode = 12345;
  EXPECT_CALL(hdr_, Finalize(_, _, _));
  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidRequest);
}

// Test that requesting LockedOwner->UnlockedAny works.
TEST_F(OwnershipUnlockTest, UnlockAny) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAny;
  EXPECT_CALL(ownership_key_,
              validate(0,
                       static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                    kOwnershipKeyRecovery),
                       _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(lifecycle_, DeviceId(_))
      .WillOnce(SetArgPointee<0>((lifecycle_device_id_t){0}));
  EXPECT_CALL(rnd_, Uint32()).WillRepeatedly(Return(5));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorWriteBootdataThenReboot);
  EXPECT_EQ(bootdata_.nonce.value[0], 5);
  EXPECT_EQ(bootdata_.nonce.value[1], 5);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateUnlockedAny);
}

// Test that requesting LockedOwner->UnlockedAny fails when the signature is
// bad.
TEST_F(OwnershipUnlockTest, UnlockAnyBadSignature) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAny;
  EXPECT_CALL(ownership_key_,
              validate(0,
                       static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                    kOwnershipKeyRecovery),
                       _, _, _))
      .WillOnce(Return(kHardenedBoolFalse));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidSignature);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

// Test that requesting LockedOwner->UnlockedAny fails when the DIN doesn't
// match.
TEST_F(OwnershipUnlockTest, UnlockAnyBadDin) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAny;
  EXPECT_CALL(ownership_key_,
              validate(0,
                       static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                    kOwnershipKeyRecovery),
                       _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(lifecycle_, DeviceId(_))
      .WillOnce(SetArgPointee<0>((lifecycle_device_id_t){0, 1, 1}));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidDin);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

// Test that requesting LockedOwner->UnlockedAny fails when the nonce doesn't
// match.
TEST_F(OwnershipUnlockTest, UnlockAnyBadNonce) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAny;
  message_.ownership_unlock_req.nonce = {3, 4};
  EXPECT_CALL(ownership_key_,
              validate(0,
                       static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                    kOwnershipKeyRecovery),
                       _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidNonce);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

// Test that requesting UnlockedAny all non-LockedOwner states fails.
TEST_P(OwnershipUnlockAnyStateTest, InvalidState) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAny;
  bootdata_.ownership_state = static_cast<uint32_t>(GetParam());
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidState);
}

INSTANTIATE_TEST_SUITE_P(AllCases, OwnershipUnlockAnyStateTest,
                         testing::Values(kOwnershipStateUnlockedSelf,
                                         kOwnershipStateUnlockedAny,
                                         kOwnershipStateUnlockedEndorsed));

// Test that requesting LockedOwner->UnlockedEndorsed works.
TEST_F(OwnershipUnlockTest, UnlockEndorsed) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockEndorsed;
  EXPECT_CALL(ownership_key_,
              validate(0,
                       static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                    kOwnershipKeyRecovery),
                       _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(lifecycle_, DeviceId(_))
      .WillOnce(SetArgPointee<0>((lifecycle_device_id_t){0}));
  EXPECT_CALL(hmac_, sha256(_, _, _))
      .WillOnce([&](const void *, size_t, hmac_digest_t *digest) {
        for (size_t i = 0; i < ARRAYSIZE(digest->digest); ++i) {
          digest->digest[i] = i;
        }
      });
  EXPECT_CALL(rnd_, Uint32()).WillRepeatedly(Return(5));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorWriteBootdataThenReboot);
  EXPECT_EQ(bootdata_.nonce.value[0], 5);
  EXPECT_EQ(bootdata_.nonce.value[1], 5);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateUnlockedEndorsed);
  for (size_t i = 0; i < ARRAYSIZE(bootdata_.next_owner); ++i) {
    EXPECT_EQ(bootdata_.next_owner[i], i);
  }
}

// Test that requesting LockedOwner->UnlockedEndorsed fails when the signature
// is bad.
TEST_F(OwnershipUnlockTest, UnlockEndorsedBadSignature) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockEndorsed;
  EXPECT_CALL(ownership_key_,
              validate(0,
                       static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                    kOwnershipKeyRecovery),
                       _, _, _))
      .WillOnce(Return(kHardenedBoolFalse));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidSignature);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

// Test that requesting LockedOwner->UnlockedEndorsed fails when the nonce
// doesn't match.
TEST_F(OwnershipUnlockTest, UnlockEndorsedBadNonce) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockEndorsed;
  message_.ownership_unlock_req.nonce = {3, 4};
  EXPECT_CALL(ownership_key_,
              validate(0,
                       static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                    kOwnershipKeyRecovery),
                       _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidNonce);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

// Test that requesting UnlockedEndorsed all non-LockedOwner states fails.
TEST_P(OwnershipUnlockEndorsedStateTest, InvalidState) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockEndorsed;
  bootdata_.ownership_state = static_cast<uint32_t>(GetParam());
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidState);
}

INSTANTIATE_TEST_SUITE_P(AllCases, OwnershipUnlockEndorsedStateTest,
                         testing::Values(kOwnershipStateUnlockedSelf,
                                         kOwnershipStateUnlockedAny,
                                         kOwnershipStateUnlockedEndorsed));

// Test that requesting LockedOwner->UnlockedSelf works.
TEST_F(OwnershipUnlockTest, UnlockUpdate) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockUpdate;
  EXPECT_CALL(
      ownership_key_,
      validate(0, static_cast<ownership_key_t>(kOwnershipKeyUnlock), _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(lifecycle_, DeviceId(_))
      .WillOnce(SetArgPointee<0>((lifecycle_device_id_t){0}));
  EXPECT_CALL(rnd_, Uint32()).WillRepeatedly(Return(5));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorWriteBootdataThenReboot);
  EXPECT_EQ(bootdata_.nonce.value[0], 5);
  EXPECT_EQ(bootdata_.nonce.value[1], 5);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateUnlockedSelf);
}

// Test that requesting LockedOwner->UnlockedSelf fails when the signature is
// bad.
TEST_F(OwnershipUnlockTest, UnlockedUpdateBadSignature) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockUpdate;
  EXPECT_CALL(
      ownership_key_,
      validate(0, static_cast<ownership_key_t>(kOwnershipKeyUnlock), _, _, _))
      .WillOnce(Return(kHardenedBoolFalse));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidSignature);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

// Test that requesting LockedOwner->UnlockedSelf fails when the nonce doesn't
// match.
TEST_F(OwnershipUnlockTest, UnlockedUpdateBadNonce) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockUpdate;
  message_.ownership_unlock_req.nonce = {3, 4};

  EXPECT_CALL(
      ownership_key_,
      validate(0, static_cast<ownership_key_t>(kOwnershipKeyUnlock), _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidNonce);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

// Test that requesting UnlockUpdate all non-LockedOwner states fails.
TEST_P(OwnershipUnlockedUpdateStateTest, InvalidState) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockUpdate;
  bootdata_.ownership_state = static_cast<uint32_t>(GetParam());
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidState);
}

INSTANTIATE_TEST_SUITE_P(AllCases, OwnershipUnlockedUpdateStateTest,
                         testing::Values(kOwnershipStateUnlockedSelf,
                                         kOwnershipStateUnlockedEndorsed,
                                         kOwnershipStateRecovery));

// Test that requesting an UnlockAbort from valid states works.
TEST_P(OwnershipUnlockAbortValidStateTest, UnlockAbort) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAbort;
  bootdata_.ownership_state = static_cast<uint32_t>(GetParam());
  EXPECT_CALL(
      ownership_key_,
      validate(0, static_cast<ownership_key_t>(kOwnershipKeyUnlock), _, _, _))
      .WillOnce(Return(kHardenedBoolTrue));
  EXPECT_CALL(lifecycle_, DeviceId(_))
      .WillOnce(SetArgPointee<0>((lifecycle_device_id_t){0}));
  EXPECT_CALL(rnd_, Uint32()).WillRepeatedly(Return(5));
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorWriteBootdataThenReboot);
  EXPECT_EQ(bootdata_.nonce.value[0], 5);
  EXPECT_EQ(bootdata_.nonce.value[1], 5);
  EXPECT_EQ(bootdata_.ownership_state, kOwnershipStateLockedOwner);
}

INSTANTIATE_TEST_SUITE_P(AllCases, OwnershipUnlockAbortValidStateTest,
                         testing::Values(kOwnershipStateUnlockedSelf,
                                         kOwnershipStateUnlockedEndorsed,
                                         kOwnershipStateUnlockedAny));

// Test that requesting an UnlockAbort from valid states works.
TEST_P(OwnershipUnlockAbortInvalidStateTest, UnlockAbort) {
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAbort;
  bootdata_.ownership_state = static_cast<uint32_t>(GetParam());
  EXPECT_CALL(hdr_, Finalize(_, _, _));

  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, kErrorOwnershipInvalidState);
}

INSTANTIATE_TEST_SUITE_P(AllCases, OwnershipUnlockAbortInvalidStateTest,
                         testing::Values(kOwnershipStateLockedOwner,
                                         kOwnershipStateRecovery));

// Test that UnlockAny succeeds in Open mode and fails in Self and NewVersion
// mode.
TEST_P(OwnershipUnlockUpdateModesTest, UnlockAny) {
  ownership_update_mode_t mode = GetParam();
  owner_page[0].update_mode = static_cast<uint32_t>(mode);
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockAny;
  rom_error_t expect;
  switch (mode) {
    case kOwnershipUpdateModeOpen:
      EXPECT_CALL(ownership_key_,
                  validate(0,
                           static_cast<ownership_key_t>(kOwnershipKeyUnlock |
                                                        kOwnershipKeyRecovery),
                           _, _, _))
          .WillOnce(Return(kHardenedBoolTrue));
      EXPECT_CALL(lifecycle_, DeviceId(_))
          .WillOnce(SetArgPointee<0>((lifecycle_device_id_t){0}));
      EXPECT_CALL(rnd_, Uint32()).WillRepeatedly(Return(5));
      expect = kErrorWriteBootdataThenReboot;
      break;
    case kOwnershipUpdateModeNewVersion:
      expect = kErrorOwnershipUnlockDenied;
      break;
    case kOwnershipUpdateModeSelf:
    case kOwnershipUpdateModeSelfVersion:
      expect = kErrorOwnershipInvalidMode;
      break;
  }

  EXPECT_CALL(hdr_, Finalize(_, _, _));
  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, expect);
}

// Test that UnlockUpdate succeeds in Open and Self mode and fails in NewVersion
// mode.
TEST_P(OwnershipUnlockUpdateModesTest, UnlockUpdate) {
  ownership_update_mode_t mode = GetParam();
  owner_page[0].update_mode = static_cast<uint32_t>(mode);
  message_.ownership_unlock_req.unlock_mode = kBootSvcUnlockUpdate;
  rom_error_t expect;
  switch (mode) {
    case kOwnershipUpdateModeOpen:
    case kOwnershipUpdateModeSelf:
    case kOwnershipUpdateModeSelfVersion:
      EXPECT_CALL(ownership_key_,
                  validate(0, static_cast<ownership_key_t>(kOwnershipKeyUnlock),
                           _, _, _))
          .WillOnce(Return(kHardenedBoolTrue));
      EXPECT_CALL(lifecycle_, DeviceId(_))
          .WillOnce(SetArgPointee<0>((lifecycle_device_id_t){0}));
      EXPECT_CALL(rnd_, Uint32()).WillRepeatedly(Return(5));
      expect = kErrorWriteBootdataThenReboot;
      break;
    case kOwnershipUpdateModeNewVersion:
      expect = kErrorOwnershipUnlockDenied;
      break;
  }

  EXPECT_CALL(hdr_, Finalize(_, _, _));
  rom_error_t error = ownership_unlock_handler(&message_, &bootdata_);
  EXPECT_EQ(error, expect);
}

INSTANTIATE_TEST_SUITE_P(AllCases, OwnershipUnlockUpdateModesTest,
                         testing::Values(kOwnershipUpdateModeOpen,
                                         kOwnershipUpdateModeSelf,
                                         kOwnershipUpdateModeSelfVersion,
                                         kOwnershipUpdateModeNewVersion));

}  // namespace
