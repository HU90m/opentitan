# User Reference

There are in general only two software controllable functions in the clock manager.

## Transactional Clock Hints

To enable a transactional clock, set the corresponding hint in {{< regref "CLK_HINTS" >}} to `1`.
To disable a transactional clock, set the corresponding hint in {{< regref "CLK_HINTS" >}} to `0`.
Note, a `0` does not indicate clock is actually disabled, software can thus check {{< regref "CLK_HINTS_STATUS" >}} for the actual state of the clock.

## Peripheral Clock Controls
To control peripheral clocks, directly change the bits in {{< regref "CLK_ENABLES" >}}.

## Device Interface Functions (DIFs)

{{< dif_listing "sw/device/lib/dif/dif_clkmgr.h" >}}

## Register Table

{{< incGenFromIpDesc "/hw/top_earlgrey/ip/clkmgr/data/autogen/clkmgr.hjson" "registers" >}}
