# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load(
    "//rules:opentitan.bzl",
    "RSA_ONLY_KEY_STRUCTS",
)
load(
    "//rules/opentitan:defs.bzl",
    "cw310_jtag_params",
    "opentitan_binary",
    "opentitan_test",
    "rsa_key_for_lc_state",
)
load("//rules:const.bzl", "CONST", "get_lc_items")
load("//rules:lc.bzl", "lc_raw_unlock_token")
load("//rules:otp.bzl", "otp_image")
load("//rules:splice.bzl", "bitstream_splice")

opentitan_test(
    name = "rv_core_ibex_isa_smoke_test_functest",
    srcs = ["//sw/device/silicon_creator/manuf/tests:idle_functest.c"],
    cw310 = cw310_jtag_params(
        binaries = {
            ":rv_core_ibex_isa_smoke_test": "sram_program",
        },
        bitstream = \
            "//sw/device/silicon_creator/manuf/tests:bitstream_rom_exec_disabled_test_unlocked0",
        tags = ["manuf"],
        test_cmd = """
            --elf={sram_program}
        """,
        test_harness = "//sw/host/tests/sival/rv_core_ibex_isa_smoke_harness",
    ),
    exec_env = {
        "//hw/top_earlgrey:fpga_cw310_rom_with_fake_keys": None,
    },
    deps = [
        "//sw/device/lib/runtime:log",
        "//sw/device/lib/testing:otp_ctrl_testutils",
        "//sw/device/lib/testing/test_framework:check",
        "//sw/device/lib/testing/test_framework:ottf_main",
    ],
)

opentitan_binary(
    name = "rv_core_ibex_isa_smoke_test",
    testonly = True,
    srcs = ["rv_core_ibex_isa_smoke_test.S", "rv_core_ibex_isa_smoke_test.c"],
    exec_env = {
        "//hw/top_earlgrey:fpga_cw310_rom_with_fake_keys": None,
    },
    kind = "ram",
    linker_script = "//sw/device/silicon_creator/manuf/lib:sram_program_linker_script",
    deps = [
        "//sw/device/lib/testing/test_framework:ottf_test_config",
        "//sw/device/silicon_creator/manuf/lib:sram_start",
    ],
)
