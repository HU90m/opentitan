# User Reference

The process in which the power manager is used is highly dependent on the system's topology.
The following proposes one method for how this can be done.

Assume first the system has the power states described [above](#supported-low-power-modes).

## Programmer Sequence for Entering Low Power

1. Disable interrupts
2. Enable desired wakeup and reset sources in {{< regref "WAKEUP_EN" >}} and {{< regref "RESET_EN" >}}.
3. Perform any system-specific low power entry steps, e.g.
   - Interrupt checks (if something became pending prior to disable)
4. Configure low power mode in {{< regref "CONTROL" >}}.
5. Set low power hint in {{< regref "LOW_POWER_HINT" >}}.
6. Set and poll {{< regref "CFG_CDC_SYNC" >}} to ensure above settings propagate across clock domains.
7. Execute wait-for-interrupt instruction on the processing host.

### Possible Exits

Once low power is initiated, the system may exit due to several reasons.
1. Graceful low power exit - This exit occurs when some source in the system gracefully wakes up the power manager.
2. System reset request - This exit occurs when either software or a peripheral requests the pwrmgr to reset the system.
3. [Fall through exit](#fall-through-handling) - This exit occurs when an interrupt manages to break the wait-for-interrupt loop.
4. [Aborted entry](#abort-handling) - This exit occurs when low power entry is attempted with an ongoing non-volatile transaction.

In both fall through exit and aborted entry, the power manager does not actually enter low power.
Instead the low power entry is interrupted and the system restored to active state.

## Programmer Sequence for Exiting Low Power

There are two separate cases for low power exit.
One is exiting from deep sleep, and the other is exiting from normal sleep.

### Exiting from Deep Sleep

When exiting from deep sleep, the system begins execution in ROM.

1. Complete normal preparation steps.
2. Check reset cause in [rstmgr]({{< relref "hw/ip/rstmgr/doc" >}})
3. Re-enable modules that have powered down.
4. Disable wakeup recording through {{< regref "WAKE_INFO_CAPTURE_DIS" >}}.
5. Check which source woke up the system through {{< regref "WAKE_INFO" >}}.
6. Take appropriate steps to handle the wake and resume normal operation.
7. Once wake is handled, clear the wake indication in {{< regref "WAKE_INFO" >}}.

### Exiting from Normal Sleep

The handling for fall-through and abort are similar to normal sleep exit.
Since in these scenarios the system was not reset, software continues executing the instruction after the wait-for-interrupt invocation.

1. Check exit condition to determine appropriate steps.
2. Clear low power hints and configuration in {{< regref "CONTROL" >}}.
3. Set and poll {{< regref "CFG_CDC_SYNC" >}} to ensure setting changes have propagated across clock boundaries.
4. Disable wakeup sources and stop recording.
5. Re-enable interrupts for normal operation and wakeup handling.
6. Once wake is handled, clear the wake indication in {{< regref "WAKE_INFO" >}}.

For an in-depth discussion, please see [power management programmers model](https://docs.google.com/document/d/1w86rmvylJgZVmmQ6Q1YBcCp2VFctkQT3zJ408SJMLPE/edit?usp=sharing) for additional details.

## Device Interface Functions (DIFs)

{{< dif_listing "sw/device/lib/dif/dif_pwrmgr.h" >}}

## Register Table

{{< incGenFromIpDesc "/hw/top_earlgrey/ip/pwrmgr/data/autogen/pwrmgr.hjson" "registers" >}}
