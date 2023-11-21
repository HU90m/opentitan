#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/dif/dif_uart.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/runtime/print.h"
#include "sw/device/lib/testing/pinmux_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_test_config.h"
#include "sw/device/lib/testing/test_framework/status.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

#include "sw/device/lib/base/csr.h"

// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR
// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES

OTTF_DEFINE_TEST_CONFIG();

static dif_uart_t uart0;
static dif_pinmux_t pinmux;

//static volatile bool pmp_load_exception;

//void ottf_load_store_fault_handler(void) {
//  LOG_INFO("In exception handler");
//  pmp_load_exception = true;
//}

void ottf_illegal_instr_fault_handler(void) {
  LOG_INFO("That's illegal!");
}

static status_t epmp_test(void) {
  uint32_t a_register;
  CSR_READ(CSR_REG_MSECCFG, &a_register);
  LOG_INFO("mseccfg %x", a_register);

  // Enabling Rule Locking Bypass
  // so we can change to SRAM configuration
  LOG_INFO("Enabling Rule Locking Bypass");
  CSR_CLEAR_BITS(CSR_REG_MSECCFG, 1 << 2);

  CSR_READ(CSR_REG_MSECCFG, &a_register);
  LOG_INFO("mseccfg %x", a_register);

  CSR_READ(CSR_REG_MSTATUS, &a_register);
  LOG_INFO("mstatus %x", a_register);


  const uint32_t sram_top = TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR;
  const uint32_t sram_bottom = sram_top + TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES;
  uint32_t sram_address_space[4];

  CHECK((uint32_t*) sram_top < sram_address_space);
  CHECK(sram_address_space + 3 < (uint32_t*) sram_bottom);

  LOG_INFO("top    %08x", sram_top);
  LOG_INFO("1.     %08x", sram_address_space);
  LOG_INFO("2.     %08x", sram_address_space + 1);
  LOG_INFO("3.     %08x", sram_address_space + 2);
  LOG_INFO("4.     %08x", sram_address_space + 3);
  LOG_INFO("bottom %08x", sram_bottom);

// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR
// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES

  void (*rom_ret_gadget)(void) = (void(*)(void)) 0x844a;

  LOG_INFO("This should work");
  rom_ret_gadget();

  asm volatile ("unimp");

  return OK_STATUS();
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

  CHECK_STATUS_OK(epmp_test());

  test_status_set(kTestStatusPassed);
}
