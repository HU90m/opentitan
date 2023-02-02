# User Reference

Software will mostly interact with the ROM controller by fetching code or loading data from ROM.
For this, the block looks like a block of memory, accessible through a TL-UL window.
However, there are a few registers that are accessible.
Other than the standard {{< regref "ALERT_TEST" >}} register, all are read-only.

The {{< regref "FATAL_ALERT_CAUSE" >}} register might change value during operations (if an alert is signalled), but the other registers will all have fixed values by the time any software runs.

To get the computed ROM digest, software can read {{< regref "DIGEST_0" >}} through {{< regref "DIGEST_7" >}}.
The ROM also contains an expected ROM digest.
Unlike the rest of the contents of ROM, this isn't scrambled.
As such, software can't read it through the standard ROM interface (which would try to unscramble it again, resulting in rubbish data that would cause a failed ECC check).
In case software needs access to this value, it can be read at {{< regref "EXP_DIGEST_0" >}} through {{< regref "EXP_DIGEST_7" >}}.

## Device Interface Functions (DIFs)

{{< dif_listing "sw/device/lib/dif/dif_rom_ctrl.h" >}}

## Register Table

{{< incGenFromIpDesc "../data/rom_ctrl.hjson" "registers" >}}
