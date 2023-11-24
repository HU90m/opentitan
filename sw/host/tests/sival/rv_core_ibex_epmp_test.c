#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/base/csr.h"
#include "sw/device/lib/dif/dif_uart.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/runtime/pmp.h"
#include "sw/device/lib/runtime/print.h"
#include "sw/device/lib/testing/pinmux_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_test_config.h"
#include "sw/device/lib/testing/test_framework/status.h"
#include "sw/device/silicon_creator/lib/dbg_print.h"
#include "sw/device/silicon_creator/lib/epmp_defs.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR
// TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES

OTTF_DEFINE_TEST_CONFIG();

extern const char __rodata_end[];
extern const char i_am_become_user[];
extern const char i_am_become_user_end[];
// Expected U Mode Instruction Access Fault Return Address
extern const char exp_u_instr_acc_fault_ret[];
// Expected M Mode Instruction Access Fault Return Address
extern const char kExpMInstrAccFaultRet[];
extern const char sys_print[];

extern void (*finish_test)(void);

static dif_uart_t uart0;
static dif_pinmux_t pinmux;

#define NUM_LOCATIONS 4
volatile uint32_t test_locations[NUM_LOCATIONS] = {
    [0] = 0x3779cdc5,
    [1] = 0x76bce080,
    [2] = 0xae8a4ed7,
    [3] = 0x00008067,
};

static inline bool was_in_machine_mode(void) {
  uint32_t mstatus;
  CSR_READ(CSR_REG_MSTATUS, &mstatus);
  // If no MPP bits are set, then I was in machine mode.
  const uint32_t mpp_offset = 10;
  return ((mstatus >> mpp_offset) & 0x3) ? true : false;
}

void ottf_exception_handler(void) {
  uint32_t mtval = ibex_mtval_read();
  ibex_exc_t mcause = ibex_mcause_read();
  bool m_mode_exception = was_in_machine_mode();

  // The frame address is the address of the stack location that holds the
  // `mepc`, since the OTTF exception handler entry code saves the `mepc` to
  // the top of the stack before transferring control flow to the exception
  // handler function (which is overridden here). See the `handler_exception`
  // subroutine in `sw/device/lib/testing/testing/ottf_isrs.S` for more details.
  uintptr_t *mepc_stack_addr = (uintptr_t *)OT_FRAME_ADDR();
  uint32_t mpec = *mepc_stack_addr;

  switch (mcause) {
    case kIbexExcLoadAccessFault:
      LOG_INFO("Load Fault");
      if (m_mode_exception) {
        CHECK(mtval == (uint32_t)(test_locations + 0),
              "Unexpected M Mode Load Access Fault:"
              " mpec = 0x%08x, mtval = 0x%08x",
              mpec, mtval);
      } else {
        CHECK(mtval == (uint32_t)(test_locations + 1) |
                  mtval == (uint32_t)(test_locations + 3),
              "Unexpected U Mode Load Access Fault:"
              " mpec = 0x%08x, mtval = 0x%08x",
              mpec, mtval);
      };
      break;
    case kIbexExcStoreAccessFault:
      LOG_INFO("Store Fault");
      if (m_mode_exception) {
        CHECK(mtval == (uint32_t)(test_locations + 0) |
                  mtval == (uint32_t)(test_locations + 3),
              "Unexpected M Mode Store Access Fault:"
              " mpec = 0x%08x, mtval = 0x%08x",
              mpec, mtval);
      } else {
        CHECK(mtval == (uint32_t)(test_locations + 0) |
                  mtval == (uint32_t)(test_locations + 1) |
                  mtval == (uint32_t)(test_locations + 2) |
                  mtval == (uint32_t)(test_locations + 3),
              "Unexpected U Mode Store Access Fault:"
              " mpec = 0x%08x, mtval = 0x%08x",
              mpec, mtval);
      };
      break;
    case kIbexExcInstrAccessFault:
      LOG_INFO("Instruction Fault");
      CHECK(mtval == (uint32_t)(test_locations + 0) |
                mtval == (uint32_t)(test_locations + 1) |
                mtval == (uint32_t)(test_locations + 2),
            "Unexpected Instruction Access Fault:"
            " mpec = 0x%08x, mtval = 0x%08x",
            mpec, mtval);
      *mepc_stack_addr = m_mode_exception
                             ? (uintptr_t)kExpMInstrAccFaultRet
                             : (uintptr_t)exp_u_instr_acc_fault_ret;
      break;
    case kIbexExcUserECall:
      if (mpec <= (uint32_t)sys_print) {
        LOG_INFO("Checkpoint");
        break;
      }
      test_status_set(kTestStatusPassed);
      finish_test();
      OT_UNREACHABLE();
    default:
      CHECK(false,
            "Unexpected Exception:"
            " mcause = 0x%x, mpec 0x%x, mtval = 0x%x",
            mcause, mpec, mtval);
      OT_UNREACHABLE();
  }
}

inline uint32_t tor_address(uint32_t addr) { return addr >> 2; }

inline uint32_t region_pmpcfg(uint32_t region) {
  switch (region / 4) {
    case 0:
      return CSR_REG_PMPCFG0;
    case 1:
      return CSR_REG_PMPCFG1;
    case 2:
      return CSR_REG_PMPCFG2;
    case 3:
      return CSR_REG_PMPCFG3;
    default:
      OT_UNREACHABLE();
  };
}

inline uint32_t region_offset(uint32_t region) { return region % 4 * 8; }

static void pmp_setup_machine_area(void) {
  const uint32_t rodata_end = (uint32_t)__rodata_end;
  const uint32_t sram_end = TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_BASE_ADDR +
                            TOP_EARLGREY_SRAM_CTRL_MAIN_RAM_SIZE_BYTES;

  CSR_WRITE(CSR_REG_PMPADDR8, tor_address(rodata_end));
  CSR_WRITE(CSR_REG_PMPADDR9, tor_address(sram_end));

  const uint32_t pmp9cfg = EPMP_CFG_A_TOR | EPMP_CFG_LRW;
  CSR_SET_BITS(region_pmpcfg(9), pmp9cfg << region_offset(1));

  // clear the execution permissions on region 11
  CSR_CLEAR_BITS(region_pmpcfg(11), EPMP_CFG_X << region_offset(11));
  uint32_t csr;
  CSR_READ(region_pmpcfg(11), &csr);
  CHECK(!((csr >> region_offset(11)) & EPMP_CFG_X),
        "Couldn't remove execute access to PMP region 11.");

  // clear the write permissions on regions 13 and 15
  CSR_CLEAR_BITS(region_pmpcfg(13), EPMP_CFG_W << region_offset(13));
  CSR_CLEAR_BITS(region_pmpcfg(15), EPMP_CFG_W << region_offset(15));
  CSR_READ(region_pmpcfg(13), &csr);
  CHECK(!((csr >> region_offset(13)) & EPMP_CFG_W),
        "Couldn't remove write access from PMP region 15.");
  CHECK(!((csr >> region_offset(15)) & EPMP_CFG_W),
        "Couldn't remove write access from PMP region 15.");
}

static void pmp_setup_user_area(void) {
  const uintptr_t start = (uintptr_t)i_am_become_user;
  const uintptr_t end = (uintptr_t)i_am_become_user_end;

  CSR_WRITE(CSR_REG_PMPADDR0, tor_address(start));
  CSR_WRITE(CSR_REG_PMPADDR1, tor_address(end));

  const uint32_t pmp1cfg = (EPMP_CFG_A_TOR | EPMP_CFG_LRWX) ^ EPMP_CFG_R;
  CSR_SET_BITS(region_pmpcfg(1), pmp1cfg << region_offset(1));
}

/*
 * | Location | L | R | W | X | U Mode | M Mode |
 * |----------|---|---|---|---|--------|--------|
 * |     0    | 0 | 1 | 0 | 0 |  R     |        |
 * |     1    | 1 | 1 | 1 | 0 |        |  RW    |
 * |     2    | 0 | 0 | 1 | 0 |  R     |  RW    |
 * |     3    | 1 | 0 | 1 | 1 |    X   |  R X   |
 */
static void pmp_setup_test_locations(void) {
  CSR_WRITE(CSR_REG_PMPADDR3, tor_address((uintptr_t)(test_locations + 0)));
  CSR_WRITE(CSR_REG_PMPADDR4, tor_address((uintptr_t)(test_locations + 1)));
  CSR_WRITE(CSR_REG_PMPADDR5, tor_address((uintptr_t)(test_locations + 2)));
  CSR_WRITE(CSR_REG_PMPADDR6, tor_address((uintptr_t)(test_locations + 3)));
  CSR_WRITE(CSR_REG_PMPADDR7, tor_address((uintptr_t)(test_locations + 4)));

  uint32_t cfg = EPMP_CFG_A_TOR | EPMP_CFG_R;
  CSR_SET_BITS(region_pmpcfg(4), cfg << region_offset(4));
  cfg = EPMP_CFG_A_TOR | EPMP_CFG_LRW;
  CSR_SET_BITS(region_pmpcfg(5), cfg << region_offset(5));
  cfg = EPMP_CFG_A_TOR | EPMP_CFG_W;
  CSR_SET_BITS(region_pmpcfg(6), cfg << region_offset(6));
  cfg = EPMP_CFG_A_TOR | EPMP_CFG_L | EPMP_CFG_X | EPMP_CFG_W;
  CSR_SET_BITS(region_pmpcfg(7), cfg << region_offset(7));
}

static void setup_uart(void) {
  // Initialise DIF handles
  CHECK_DIF_OK(dif_pinmux_init(
      mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR), &pinmux));
  CHECK_DIF_OK(dif_uart_init(
      mmio_region_from_addr(TOP_EARLGREY_UART0_BASE_ADDR), &uart0));

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
  setup_uart();
  pmp_setup_machine_area();

  LOG_INFO("Enable Machine Mode Lockdown");
  CSR_SET_BITS(CSR_REG_MSECCFG, EPMP_MSECCFG_MML);

  pmp_setup_user_area();
  pmp_setup_test_locations();

  LOG_INFO("The PMP Config:");
  dbg_print_epmp();

  uint32_t load;

  LOG_INFO("M Mode Tests");
  for (int loc = 0; loc < NUM_LOCATIONS; ++loc) {
    test_locations[loc] = 42;
    load = test_locations[loc];
    ((void (*)(void))(test_locations + loc))();
    OT_ADDRESSABLE_LABEL(kExpMInstrAccFaultRet);
  };

  // Pretending to use load
  (void)load;

  LOG_INFO("U Mode Tests");
  asm volatile(
      "la t0, i_am_become_user\n"
      "csrw mepc, t0\n"
      "mret\n"
      :  // The clobber doesn't really matter,
      :  // we're not comming back.
      : "t0");
  OT_UNREACHABLE();
}
