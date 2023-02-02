# Security Hardening

The AES unit employs different means at architectural, micro-architectural and physical levels for security hardening against side-channel analysis and fault injection.

## Side-Channel Analysis

To aggravate side-channel analysis (SCA), the AES unit implements the following countermeasures.

### 1st-order Masking of the Cipher Core

The AES unit employs 1st-order masking of the AES cipher core.
More precisely, both the cipher and the key expand data path use two shares.
As shown in the block diagram below, the width of all registers and data paths basically doubles.

![Block diagram of the masked AES cipher core.](./aes_block_diagram_cipher_core_masked.svg)

The initial key is provided in two shares via the register interface.
The input data is provided in unmasked form and masked outside of the cipher core to obtain the two shares of the initial state.
The pseudo-random data (PRD) required for masking the input data is provided by the pseudo-random number generator (PRNG) of the cipher core.
Similarly, the two shares of the output state are combined outside the cipher core to obtain the output data.

The same PRNG also generates the fresh randomness required by the masked SubBytes (16 masked S-Boxes) and the masked KeyExpand (4 masked S-Boxes).
The masking scheme selected for the S-Box can have a high impact on SCA resistance, circuit area, number of PRD bits consumed per cycle and per S-Box evaluation, and throughput.
The selection of the masked S-Box implementation can be controlled via compile-time Verilog parameter.
By default, the AES unit uses domain-oriented masking (DOM) for the S-Boxes as proposed by [Gross et al.: "Domain-Oriented Masking: Compact Masked Hardware Implementations with Arbitrary Protection Order".](https://eprint.iacr.org/2016/486.pdf)
The provided implementation has a latency of 5 clock cycles per S-Box evaluation.
As a result, the overall latency for processing a 16-byte data block increases from 12/14/16 to 56/66/72 clock cycles in AES-128/192/256 mode, respectively.
The provided implementation further forwards partial, intermediate results among DOM S-Box instances for remasking purposes.
This allows to reduce circuit area related to generating, buffering and applying PRD without impacting SCA resistance.
Alternatively, the two original versions of the masked Canright S-Box can be chosen as proposed by [Canright and Batina: "A very compact "perfectly masked" S-Box for AES (corrected)".](https://eprint.iacr.org/2009/011.pdf)
These are fully combinational (one S-Box evaluation every cycle) and have lower area footprint, but they are significantly less resistant to SCA.
They are mainly included for reference but their usage is discouraged due to potential vulnerabilities to the correlation-enhanced collision attack as described by [Moradi et al.: "Correlation-Enhanced Power Analysis Collision Attack".](https://eprint.iacr.org/2010/297.pdf)

The masking PRNG is reseeded with fresh entropy via [EDN]({{< relref "hw/ip/edn/doc" >}}) automatically 1) whenever a new key is provided (see {{< regref "CTRL_AUX_SHADOWED.KEY_TOUCH_FORCES_RESEED" >}}) and 2) based on a block counter.
The rate at which this block counter initiates automatic reseed operations can be configured via {{< regref "CTRL_SHADOWED.PRNG_RESEED_RATE" >}}.
In addition software can manually initiate a reseed operation via {{< regref "TRIGGER.PRNG_RESEED" >}}.

Note that the masking can be enabled/disabled via compile-time Verilog parameter.
It may be acceptable to disable the masking when using the AES cipher core for random number generation e.g. inside [CSRNG.]({{< relref "hw/ip/csrng/doc" >}})
When disabling the masking, also an unmasked S-Box implementation needs to be selected using the corresponding compile-time Verilog parameter.
When disabling masking, it is recommended to use the unmasked Canright or LUT S-Box implementation for ASIC or FPGA targets, respectively.
Both are fully combinational and allow for one S-Box evaluation every clock cycle.

It's worth noting that since input/output data are provided/retrieved via register interface in unmasked form, the AES unit should not be used to form an identity ladder where the output of one AES operation is used to form the key for the next AES operation in the ladder.
In OpenTitan, the [Keccak Message Authentication Code (KMAC) unit]({{< relref "hw/ip/kmac/doc" >}}) is used for that purpose.

### Fully-Parallel Data Path

Any 1st-order masking scheme primarily protects against 1st-order SCA.
Vulnerabilities against higher-order SCA might still be present.
A common technique to aggravate higher-order attacks is to increase the noise in the system e.g. by leveraging parallel architectures.
To this end, the AES cipher core uses a 128-bit parallel data path with a total of up to 20 S-Boxes (16 inside SubBytes, 4 inside KeyExpand) that are evaluated in parallel.

Besides more noise for increased resistance against higher-order SCA, the fully-parallel architecture also enables for higher performance and flexibility.
It allows users to seamlessly switch out the S-Box implementation in order to experiment with different masking schemes.
To interface the data paths with the S-Boxes, a handshake protocol is used.

### Note on Reset vs. Non-Reset Flip-Flops

The choice of flip-flop type for registering sensitive assets such as keys can have implications on the vulnerability against e.g. combined reset glitch attacks and SCA.
Following the [OpenTitan non-reset vs. reset flops rationale](https://github.com/lowRISC/opentitan/issues/2603), the following observations can be made:
- If masking is enabled, key and state values are stored in two shares inside the AES unit.
  Neither the Hamming weights of the individual shares nor the summed Hamming weight are proportional to the Hamming weight of the secret asset.
- Input/output data and IV values are (currently) not stored in multiple shares but these are less critical as they are used only once.
  Further, they are stored in banks of 32 bits leaving a larger hypothesis space compared to when glitching e.g. an 8-bit register into reset.
  In addition, they could potentially also be extracted when being transferred over the TL-UL bus interface.

For this reason, the AES unit uses reset flops only.
However, all major key and data registers are cleared with pseudo-random data upon reset.

### Clearing Registers with Pseudo-Random Data

Upon reset or if initiated by software, all major key and data registers inside the AES module are cleared with pseudo-random data (PRD).
This helps to reduce SCA leakage when both writing these registers for reconfiguration and when clearing the registers after use.

In addition, the state registers inside the cipher core are cleared with PRD during the last round of every encryption/decryption.
This prevents Hamming distance leakage between the states of the last two rounds as well as between output and input data.

## Fault Injection

Fault injection (FI) attacks can be distinguished based on the FI target.

### Control Path

In cryptographic devices, fault attacks on the control path usually aim to disturb the control flow in a way to facilitate SCA or other attacks.
Example targets for AES include: switch to less secure mode of operation (ECB), keep processing the same input data, reduce the number of rounds/early termination, skip particular rounds, skip individual operations in a round.

To protect against FI attacks on the control path, the AES unit implements the following countermeasures.

- Shadowed Control Register:
  The main control register is implemented as a shadow register.
  This means software has to perform two subsequent write operations to perform an update.
  Internally, a shadow copy is used that is constantly compared with the actual register.
  For further details, refer to the [Register Tool documentation.]({{< relref "doc/rm/register_tool#shadow-registers" >}})

- Sparse encodings of FSM states:
  All FSMs inside the AES unit use sparse state encodings.

- Sparse encodings for mux selector signals:
  All main muxes use sparsely encoded selector signals.

- Sparse encodings for handshake and other important control signals.

- Multi-rail control logic:
  All FSMs inside the AES unit are implemented using multiple independent and redundant logic rails.
  Every rail evaluates and drives exactly one bit of sparsely encoded handshake or other important control signals.
  The outputs of the different rails are constantly compared to detect potential faults.
  The number of logic rails can be scaled up by means of relatively easy RTL modifications.
  By default, three independent logic rails are used.

- Hardened round counter:
  Similar to the cipher core FSM, the internal round counter is protected against FI through a multi-rail implementation.
  The outputs of the different rails are constantly compared to detect potential faults in the round counter.

If any of these countermeasures detects a fault, a fatal alert is triggered, the internal FSMs go into a terminal error state, the AES unit does not release further data and locks up until reset.
Since the AES unit has no ability to reset itself, a system-supplied reset is required before the AES unit can become operational again.
Such a condition is reported in {{< regref "STATUS.ALERT_FATAL_FAULT" >}}.
Details on where the fault has been detected are not provided.

### Data Path

The aim of fault attacks on the data path is typically to extract information on the key by means of statistical analysis.
The current version of the AES unit does not employ countermeasures against such attacks, but future versions most likely will.
