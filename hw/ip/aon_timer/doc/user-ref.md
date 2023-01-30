# User Reference

## Initialization

1. Write the timer values {{<regref"WKUP_COUNT">}} and {{<regref"WDOG_COUNT">}} to zero.
2. Program the desired wakeup pre-scaler value in {{<regref"WKUP_CTRL">}}.
3. Program the desired thresholds in {{<regref"WKUP_THOLD">}}, {{<regref"WDOG_BARK_THOLD">}} and {{<regref"WDOG_BITE_THOLD">}}.
4. Set the enable bit to 1 in the {{<regref"WKUP_CTRL">}} / {{<regref"WDOG_CTRL">}} registers.
5. If desired, lock the watchdog configuration by writing 1 to the `regwen` bit in {{<regref"WDOG_REGWEN">}}.

## Watchdog pet

Pet the watchdog by writing zero to the {{<regref"WDOG_COUNT">}} register.

## Interrupt Handling

If either timer reaches the programmed threshold, interrupts are generated from the AON_TIMER module.
Disable or reinitialize the wakeup timer if required by clearing the enable bit in {{<regref"WKUP_CTRL">}} or clearing the timer value in {{<regref"WKUP_COUNT">}}.
Clear the interrupt by writing 1 into the Interrupt Status Register {{<regref "INTR_STATE">}}.

If the timer has caused a wakeup event ({{<regref"WKUP_CAUSE">}} is set) then clear the wakeup request by writing 0 to {{<regref"WKUP_CAUSE">}}.

If {{<regref"WKUP_COUNT">}} remains above the threshold after clearing the interrupt or wakeup event and the timer remains enabled, the interrupt and wakeup event will trigger again at the next clock tick.

## Device Interface Functions (DIFs)

{{< dif_listing "sw/device/lib/dif/dif_aon_timer.h" >}}

## Register Table

{{< incGenFromIpDesc "../data/aon_timer.hjson" "registers" >}}
