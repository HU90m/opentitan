// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sw/device/lib/base/memory.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_rv_core_ibex.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/testing/flash_ctrl_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_isrs.h"
#include "sw/device/lib/testing/test_framework/ottf_macros.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

#define TEST_STR "Hello there, WHaT 1S Y0Ur N@ME?"
#define EXPECTED_RESULT_MAKE_LOWER_CASE "hello there, what 1s y0ur n@me?"
#define EXPECTED_RESULT_GET_NAME "My name is Titan, Open Titan"

#define TEST_DATA_SIZE 0x100  // size of test_data in words

OTTF_DEFINE_TEST_CONFIG();

// A function which takes a string as it's only argument.
typedef void (*str_fn_t)(char *);

/**
 * A toy function that replaces the content of a given string with "My name is
 * Titan, Open Titan". If the char buffer given is too small, it fills the
 * buffer as far as is possible.
 *
 * @param input The string to have it's content replaced.
 */
extern void get_name(char *input);

/**
 * A toy function that takes an ASCII string and converts every capital letter
 * into a lowercase letter.
 *
 * @param input The string to be converted to lowercase.
 */
extern void make_lower_case(char *input);

// The toy function sizes in bytes.
extern const uint32_t kGetNameFnSize;
extern const uint32_t kMakeLowerCaseFnSize;

// Stores whether an access fault exception has fired.
static volatile bool access_fault = false;

/**
 * Overrides the default OTTF exception handler.
 *
 * Taken from `sw/device/tests/sram_ctrl_execution_test_ret.c`.
 * Look here for explaination.
 */
void ottf_exception_handler(void) {
  uintptr_t mepc_stack_addr = (uintptr_t)OT_FRAME_ADDR();

  uintptr_t ret_addr = *(uintptr_t *)(mepc_stack_addr + OTTF_WORD_SIZE);

  uint32_t mcause = ibex_mcause_read();
  ottf_exc_id_t exception_id = mcause & kIdMax;

  switch (exception_id) {
    case kInstrAccessFault:
      LOG_INFO("Instruction access fault handler");
      access_fault = true;
      *(uintptr_t *)mepc_stack_addr = ret_addr;
      break;
    default:
      LOG_FATAL("Unexpected exception id = 0x%x", exception_id);
      abort();
  }
}

/**
 * Takes a unsigned 32bit integer and rounds it to the next power of 2.
 * Algorithm explained here:
 *   https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
 *
 * @param n The number to round.
 * @return The next power of two from the given number.
 */
uint32_t next_power_of_two(uint32_t n) {
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return ++n;
}

/**
 * Sets up the flash controller.
 *
 * @return The flash controller's handle.
 */
dif_flash_ctrl_state_t init_flash() {
  dif_flash_ctrl_state_t flash_ctrl;

  CHECK_DIF_OK(dif_flash_ctrl_init_state(
      &flash_ctrl,
      mmio_region_from_addr(TOP_EARLGREY_FLASH_CTRL_CORE_BASE_ADDR)));
  flash_ctrl_testutils_wait_for_init(&flash_ctrl);

  // Set up default access for data partitions.
  flash_ctrl_testutils_default_region_access(
      &flash_ctrl, /*rd_en=*/true, /*prog_en=*/true, /*erase_en=*/true,
      /*scramble_en=*/false, /*ecc_en=*/false, /*high_endurance_en=*/false);

  return flash_ctrl;
}

bool test_main(void) {
  // Calculate 2-byte aligned addresses to put a copy of the toy functions in.
  uintptr_t flash_mem_end_addr = TOP_EARLGREY_FLASH_CTRL_MEM_BASE_ADDR +
                                 TOP_EARLGREY_FLASH_CTRL_MEM_SIZE_BYTES;
  uintptr_t offset = 0x1000;
  uintptr_t make_lower_case_addr = flash_mem_end_addr - offset;
  uintptr_t get_name_addr = flash_mem_end_addr - (2 * offset);

  // Initiate the flash controller.
  dif_flash_ctrl_state_t flash_ctrl = init_flash();

  // Move the toy functions to their aligned addresses.
  CHECK(!flash_ctrl_testutils_erase_and_write_page(
      /*flash_state=*/&flash_ctrl,
      /*byte_address=*/(uint32_t)make_lower_case_addr,
      /*partition_id=*/0,
      /*data=*/(uint32_t *)make_lower_case,
      /*partition_type=*/kDifFlashCtrlPartitionTypeData,
      /*word_count=*/kMakeLowerCaseFnSize / 4));
  CHECK(!flash_ctrl_testutils_erase_and_write_page(
      /*flash_state=*/&flash_ctrl,
      /*byte_address=*/(uint32_t)get_name_addr,
      /*partition_id=*/0,
      /*data=*/(uint32_t *)get_name,
      /*partition_type=*/kDifFlashCtrlPartitionTypeData,
      /*word_count=*/kGetNameFnSize / 4));

  char test_str[] = TEST_STR;
  ((str_fn_t)make_lower_case_addr)(test_str);
  CHECK_STR_EQ(test_str, EXPECTED_RESULT_MAKE_LOWER_CASE);

  ((str_fn_t)get_name_addr)(test_str);
  CHECK_STR_EQ(test_str, EXPECTED_RESULT_GET_NAME);

  // The memory address to which the functions will be mapped.
  const uintptr_t kRemapAddr = 0xA0000000;

  // Create translation descriptions.
  dif_rv_core_ibex_addr_translation_mapping_t make_lower_case_mapping = {
      .matching_addr = kRemapAddr,
      .remap_addr = make_lower_case_addr,
      .size = next_power_of_two(kMakeLowerCaseFnSize),
  };
  dif_rv_core_ibex_addr_translation_mapping_t get_name_mapping = {
      .matching_addr = kRemapAddr,
      .remap_addr = get_name_addr,
      .size = next_power_of_two(kGetNameFnSize),
  };
  const dif_rv_core_ibex_addr_translation_bus_t kBuses[] = {
      kDifRvCoreIbexAddrTranslationIBus, kDifRvCoreIbexAddrTranslationDBus};
  const dif_rv_core_ibex_addr_translation_slot_t kSlots[] = {
      kDifRvCoreIbexAddrTranslationSlot_0, kDifRvCoreIbexAddrTranslationSlot_1};

  // Get ibex core handle.
  dif_rv_core_ibex_t ibex_core;
  CHECK_DIF_OK(dif_rv_core_ibex_init(
      mmio_region_from_addr(TOP_EARLGREY_RV_CORE_IBEX_CFG_BASE_ADDR),
      &ibex_core));

  // Map virtual address space to make_lower_case() using slot 1.
  for (size_t idx = 0; idx < 2; ++idx) {
    CHECK_DIF_OK(dif_rv_core_ibex_configure_addr_translation(
        &ibex_core, kSlots[1], kBuses[idx], make_lower_case_mapping));
  };

  // Enable address translation slot 1.
  for (size_t idx = 0; idx < 2; ++idx) {
    CHECK_DIF_OK(dif_rv_core_ibex_enable_addr_translation(&ibex_core, kSlots[1],
                                                          kBuses[idx]));
  };

  // Reset test string content.
  memcpy(test_str, TEST_STR, sizeof(test_str));

  // Run make_lower_case() from virtual memory and check the result.
  ((str_fn_t)kRemapAddr)(test_str);
  CHECK_STR_EQ(test_str, EXPECTED_RESULT_MAKE_LOWER_CASE);

  // Remap virtual address space to get_name() using slot 1.
  for (size_t idx = 0; idx < 2; ++idx) {
    CHECK_DIF_OK(dif_rv_core_ibex_configure_addr_translation(
        &ibex_core, kSlots[1], kBuses[idx], get_name_mapping));
  };

  // Run get_name() from virtual memory and check the result.
  ((str_fn_t)kRemapAddr)(test_str);
  CHECK_STR_EQ(test_str, EXPECTED_RESULT_GET_NAME);

  /////////////////////////////////////////////////////////////////////////////
  // Check slot 0 has higher priority than slot 1.
  /////////////////////////////////////////////////////////////////////////////
  //
  // Map virtual address space to make_lower_case() but using slot 0.
  for (size_t idx = 0; idx < 2; ++idx) {
    CHECK_DIF_OK(dif_rv_core_ibex_configure_addr_translation(
        &ibex_core, kSlots[0], kBuses[idx], make_lower_case_mapping));
  };

  // Enable address translation slot 0.
  for (size_t idx = 0; idx < 2; ++idx) {
    CHECK_DIF_OK(dif_rv_core_ibex_enable_addr_translation(&ibex_core, kSlots[0],
                                                          kBuses[idx]));
  };

  // Reset test string content.
  memcpy(test_str, TEST_STR, sizeof(test_str));

  // Run get_name() from virtual memory and check the result.
  ((str_fn_t)kRemapAddr)(test_str);
  CHECK_STR_EQ(test_str, EXPECTED_RESULT_MAKE_LOWER_CASE);

  /////////////////////////////////////////////////////////////////////////////
  // Check address translation no longer occurs after being disabled.
  /////////////////////////////////////////////////////////////////////////////
  //
  // Disable all address translation.
  for (size_t slot_i; slot_i < 2; ++slot_i) {
    for (size_t bus_i; bus_i < 2; ++bus_i) {
      CHECK_DIF_OK(dif_rv_core_ibex_disable_addr_translation(
          &ibex_core, kSlots[slot_i], kBuses[bus_i]));
    }
  }

  // Ensure there hasn't already been an access fault.
  CHECK(!access_fault);

  // Try to run the remap address as a function.
  ((str_fn_t)kRemapAddr)(test_str);

  // Ensure the exception has fired.
  CHECK(access_fault);

  return true;
}
