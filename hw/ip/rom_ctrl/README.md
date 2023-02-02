# ROM Controller Technical Specification

This document describes the ROM controller (`rom_ctrl`).
This module attaches as a peripheral to the system bus, and thus follows the [Comportability Specification]({{< relref "doc/rm/comportability_specification" >}}).

The ROM controller interfaces between the system bus and the ROM.
This ROM has scrambled contents (scrambled with a fixed key, derived from a global constant).
The controller is responsible for descrambling these contents on memory fetches.

Unlike the [SRAM controller]({{< relref "hw/ip/sram_ctrl/doc" >}}), which performs the equivalent task for SRAM, the ROM controller also contains a *ROM checker* block.
This ROM checker is used to compute a cryptographic hash of the ROM contents just after boot, detecting any malicious changes that have been made to the ROM when the system was at rest.

## Features

- Logic for memory and address descrambling
- Post-boot ROM integrity check
- Alert trigger and status CSRs for ROM integrity errors or FSM glitches.
