#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/dif/dif_uart.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/runtime/print.h"
#include "sw/device/lib/runtime/pmp.h"
#include "sw/device/lib/testing/pinmux_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_test_config.h"
#include "sw/device/lib/testing/test_framework/status.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

#include "sw/device/lib/base/csr.h"

// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR
// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES

OTTF_DEFINE_TEST_CONFIG();

extern char __rodata_end[];

extern void (*finish_test)(void);

static dif_uart_t uart0;
static dif_pinmux_t pinmux;

#define NUM_LOCS 4
volatile uint32_t test_locations[NUM_LOCS];

//static volatile bool pmp_load_exception;

void ottf_load_store_fault_handler(void) {
  LOG_INFO("In exception handler");
//  pmp_load_exception = true;
}

void ottf_user_ecall_handler(void) {
  test_status_set(kTestStatusPassed);
  finish_test();
}

static void unlock_sram(void) {
  uint32_t csr;
  CSR_READ(CSR_REG_MSECCFG, &csr);
  CHECK(csr & 0x4, "Expect Rule Locking Bypass to be enabled.");

  CSR_CLEAR_BITS(CSR_REG_PMPCFG3, 1 << 31);
  CSR_READ(CSR_REG_PMPCFG3, &csr);
  CHECK(!(csr >> 31), "Couldn't unlock PMP region 15.");
}

static void setup_sram(void) {


  const uint32_t rodata_end = (uint32_t) __rodata_end;
  const uint32_t sram_end = TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR
                          + TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES;

  pmp_region_config_t config = {
      .lock = kPmpRegionLockLocked,
      .permissions = kPmpRegionPermissionsReadWrite,
  };
  pmp_region_configure_result_t result = pmp_region_configure_tor(
      9, config, rodata_end, sram_end
  );
  CHECK(result == kPmpRegionConfigureOk,
        "Load configuration failed, error code = %d", result);

  unlock_sram();
  // clear the write permissions
  uint32_t csr;
  CSR_CLEAR_BITS(CSR_REG_PMPCFG3, 1 << 25);
  CSR_SET_BITS(CSR_REG_PMPCFG3, 1 << 31);
  CSR_READ(CSR_REG_PMPCFG3, &csr);
  CHECK(!((csr >> 25) & 1), "Couldn't remove write access to PMP region 15.");
  CHECK(csr >> 31, "Couldn't lock PMP region 15.");

  CSR_READ(CSR_REG_PMPCFG2, &csr);
  LOG_INFO("pmpcfg2 %x", csr);
  CSR_READ(CSR_REG_PMPCFG3, &csr);
  LOG_INFO("pmpcfg3 %x", csr);
}

static void check_rlb(void) {
  uint32_t csr;
  CSR_READ(CSR_REG_PMPCFG0, &csr);
  CHECK((csr >> 23) & 1, "Expected PMP region 2 to be locked.");

  CSR_CLEAR_BITS(CSR_REG_PMPCFG0, 1 << 23);
  CSR_READ(CSR_REG_PMPCFG0, &csr);
  CHECK(!((csr >> 23) & 1), "Cloudn't unlock region 2.");

  CSR_SET_BITS(CSR_REG_PMPCFG0, 1 << 23);
  CSR_READ(CSR_REG_PMPCFG0, &csr);
  CHECK((csr >> 23) & 1, "Cloudn't lock region 2.");

  LOG_INFO("Disable Rule Locking Bypass");
  CSR_CLEAR_BITS(CSR_REG_MSECCFG, 1 << 2);
  CSR_READ(CSR_REG_MSECCFG, &csr);
  CHECK(!(csr & 0x4), "Expect Rule Locking Bypass to be disabled.");

  CSR_CLEAR_BITS(CSR_REG_PMPCFG0, 1 << 23);
  CSR_READ(CSR_REG_PMPCFG0, &csr);
  CHECK(
    (csr >> 23) & 1,
    "Expected unlock to have no effect when Rule Locking Bypass isn't enabled."
  );
}

static void epmp_test(void) {
  setup_sram();

  LOG_INFO("rodata %p", __rodata_end);
  //uint32_t a_register;
  //CSR_READ(CSR_REG_MSECCFG, &a_register);
  //LOG_INFO("mseccfg %x", a_register);

  // Unlock SRAM,
  // so we can enable Machine Mode Lockdown
  // and still execute SRAM in machine mode.
  //unlock_sram();
  check_rlb();

  LOG_INFO("Enable Machine Mode Lockdown");
  CSR_SET_BITS(CSR_REG_MSECCFG, 1 << 0);
  return;

  pmp_region_config_t config = {
      .lock = kPmpRegionLockLocked,
      .permissions = kPmpRegionPermissionsNone,
  };

  LOG_INFO("%d", test_locations[0]);
  pmp_region_configure_result_t pmp_result = pmp_region_configure_tor(
    4,
    config,
    (uintptr_t) (test_locations),
    (uintptr_t) (test_locations + 1)
  );
  CHECK(pmp_result == kPmpRegionConfigureOk,
        "Load configuration failed, error code = %d", pmp_result);

  LOG_INFO("one");
  LOG_INFO("%d", test_locations[0]);

  test_locations[0] = 77;

  LOG_INFO("two");

  test_locations[1] = 23;

  LOG_INFO("1.     %08x", test_locations);
  LOG_INFO("2.     %08x", test_locations + 1);
  LOG_INFO("3.     %08x", test_locations + 2);
  LOG_INFO("4.     %08x", test_locations + 3);

  const uint32_t rodata_top = (uint32_t) __rodata_end;
  const uint32_t sram_top = TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR
                          + TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES;

  LOG_INFO("5.     %08x", rodata_top);
  LOG_INFO("6.     %08x", sram_top);
  LOG_INFO("7.     %08x", sram_top - rodata_top);

  void (*rom_ret_gadget)(void) = (void(*)(void)) 0x844a;

  LOG_INFO("This should work");
  rom_ret_gadget();

  // User mode part of the test
  asm volatile (
    "la t0, i_am_become_user\n"
    "csrw mepc, t0\n"
    "mret\n"
    : // The clobber doesn't really matter,
    : // we're not comming back.
    : "t0"
  );
}

static void setup(void) {
  // Initialise DIF handles
  CHECK_DIF_OK(dif_pinmux_init(
    mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR),
    &pinmux
  ));
  CHECK_DIF_OK(dif_uart_init(
    mmio_region_from_addr(TOP_EARLGREY_UART0_BASE_ADDR),
    &uart0
  ));

  // Initialise UART console.
  pinmux_testutils_init(&pinmux);
  CHECK(kUartBaudrate <= UINT32_MAX, "kUartBaudrate must fit in uint32_t");
  CHECK(kClockFreqPeripheralHz <= UINT32_MAX,
        "kClockFreqPeripheralHz must fit in uint32_t");
  CHECK_DIF_OK(dif_uart_configure(
      &uart0, (dif_uart_config_t){
                  .baudrate = (uint32_t)kUartBaudrate,
                  .clk_freq_hz = (uint32_t)kClockFreqPeripheralHz,
                  .parity_enable = kDifToggleDisabled,
                  .parity = kDifUartParityEven,
                  .tx_enable = kDifToggleEnabled,
                  .rx_enable = kDifToggleEnabled,
              }));
  base_uart_stdout(&uart0);
}

void sram_main(void) {
  setup();

  epmp_test();

  test_status_set(kTestStatusPassed);
}
