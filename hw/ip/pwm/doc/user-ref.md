# User Reference

To set the PWM Frequency for the entire IP:
1. Clear {{< regref "CFG.CNTR_EN" >}}
2. Select {{< regref "CFG.CLK_DIV" >}}
3. Assert {{< regref "CFG.CNTR_EN" >}}

To configure the fixed PWM duty cycle and for a particular output channel (for example channel 0):

1. Disable blinking by clearing the {{< regref "PWM_PARAM_0.BLINK_EN_0" >}} bit.
2. Set {{< regref "DUTY_CYCLE_0.A_0" >}}
3. Optionally set {{< regref "PWM_PARAM_0.PHASE_DELAY_0" >}} to adjust the pulse phase.
4. Optionally assert {{< regref "INVERT.INVERT_0" >}} to flip the polarity.
5. Set {{< regref "PWM_EN.EN_0" >}} to turn the channel on.

These changes will take place immediately, regardless of whether the `phase_ctr` is currently in the middle of a pulse cycle.

To activate simple blinking for channel 0:

1. Set {{< regref "DUTY_CYCLE_0.A_0" >}} and {{< regref "DUTY_CYCLE_0.B_0" >}} to establish the initial and target duty cycles.
2. Clear the {{< regref "PWM_PARAM_0.BLINK_EN_0" >}} and {{< regref "PWM_PARAM_0.HTBT_EN_0" >}} bits.
This step is necessary for changing the blink timing parameters
3. Set  {{< regref "BLINK_PARAM_0.X_0" >}} and {{< regref "BLINK_PARAM_0.Y_0" >}} to set the number of pulse cycles respectively spent at duty cycle A and duty cycle B.
4. Re-assert {{< regref "PWM_PARAM_0.BLINK_EN_0" >}}.

For synchronous blinking of a group of channels, first disable the desired channels using the {{< regref "PWM_EN" >}} register.
Then after configuring the blink properties of the entire group, re-enable them with a single write to {{< regref "PWM_EN" >}}.

To activate heartbeat blinking for channel 0:
1. Set {{< regref "DUTY_CYCLE_0.A_0" >}} and {{< regref "DUTY_CYCLE_0.B_0" >}} to establish the initial and target duty cycles.
2. Clear the {{< regref "PWM_PARAM_0.BLINK_EN_0" >}} bit.
This step is necessary for changing the blink timing parameters
3. Set {{< regref "BLINK_PARAM_0.X_0" >}} to the number of pulse cycles between duty cycle steps (i.e. increments or decrements).
4. Set {{< regref "BLINK_PARAM_0.Y_0" >}} to set the size of each step.
5. In a single write, assert both {{< regref "PWM_PARAM_0.BLINK_EN_0" >}} and {{< regref "PWM_PARAM_0.HTBT_EN_0" >}}

## Device Interface Functions (DIFs)

{{< dif_listing "sw/device/lib/dif/dif_pwm.h" >}}

## Register Table

{{< incGenFromIpDesc "../data/pwm.hjson" "registers" >}}
