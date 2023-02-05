# Theory of Operations

The AES unit supports both encryption and decryption for AES-128/192/256 in ECB, CBC, CFB, OFB and CTR modes using a single, shared data path.
That is, it can either do encryption or decryption but not both at the same time.

The AES unit features a key expanding mechanism to generate the required round keys on-the-fly from a single initial key provided through the register interface.
This means the processor needs to provide just the initial encryption key to the AES unit via register interface.
The AES unit then uses this key to generate all round keys as they are needed in parallel to the actual encryption/decryption.
The benefits of this design compared to passing all round keys via register interface include:

- Reduced storage requirements and smaller circuit area: Instead of storing 15 128-bit round keys, only 3 256-bit key registers are required for AES-256:
  - one set of registers to which the processor writes the initial key, i.e., the start key for encryption,
  - one set of registers to hold the current full key, and
  - one set of registers to hold the full key of the last encryption round, i.e., the start key for decryption.
- Faster re-configuration and key switching: The core just needs to perform 8 write operations instead of 60 write operations for AES-256.

On-the-fly round-key generation comes however at the price of an initial delay whenever the key is changed by the processor before the AES unit can perform ECB/CBC **decryption** using this new key.
During this phase, the key expanding mechanism iteratively computes the start key for the decryption.
The duration of this delay phase corresponds to the latency required for encrypting one 16B block (i.e., 12/14/16 cycles for AES-128/192/256).
Once the start key for decryption has been computed, it is stored in a dedicated internal register for later use.
The AES unit can then switch between decryption and encryption without additional overhead.

For encryption or if the mode is set to CFB, OFB or CTR, there is no such initial delay upon changing the key.
If the next operation after a key switch is ECB or CBC **decryption**, the AES unit automatically initiates a key expansion using the key schedule first (to generate the start key for decryption, the actual data path remains idle during that phase).

The AES unit uses a status register to indicate to the processor when ready to receive the next input data block via the register interface.
While the AES unit is performing encryption/decryption of a data block, it is safe for the processor to provide the next input data block.
The AES unit automatically starts the encryption/decryption of the next data block once the previous encryption/decryption is finished and new input data is available.
The order in which the input registers are written does not matter.
Every input register must be written at least once for the AES unit to automatically start encryption/decryption.
This is the default behavior.
It can be disabled by setting the MANUAL_OPERATION bit in {{< regref "CTRL_SHADOWED" >}} to `1`.
In this case, the AES unit only starts the encryption/decryption once the START bit in {{< regref "TRIGGER" >}} is set to `1` (automatically cleared to `0` once the next encryption/decryption is started).

Similarly, the AES unit indicates via a status register when having new output data available to be read by the processor.
Also, there is a back-pressure mechanism for the output data.
If the AES unit wants to finish the encryption/decryption of a data block but the previous output data has not yet been read by the processor, the AES unit is stalled.
It hangs and does not drop data.
It only continues once the previous output data has been read and the corresponding registers can be safely overwritten.
The order in which the output registers are read does not matter.
Every output register must be read at least once for the AES unit to continue.
This is the default behavior.
It can be disabled by setting the MANUAL_OPERATION bit in {{< regref "CTRL_SHADOWED" >}} to `1`.
In this case, the AES unit never stalls and just overwrites previous output data, independent of whether it has been read or not.


## Block Diagram

This AES unit targets medium performance (\~1 cycle per round for the unmasked implementation).
High-speed, single-cycle operation for high-bandwidth data streaming is not required.

Therefore, the AES unit uses an iterative cipher core architecture with a 128-bit wide data path as shown in the figure below.
Note that for the sake of simplicity, the figure shows the unmasked implementation.
For details on the masked implementation of the cipher core refer to [Security Hardening below]({{< relref "#security-hardening" >}})).
Using an iterative architecture allows for a smaller circuit area at the cost of throughput.
Employing a 128-bit wide data path allows to achieve the latency requirements of 12/14/16 clock cycles per 16B data block in AES-128/192/256 mode in the unmasked implementation, respectively.

![AES unit block diagram (unmasked implementation) with shared data paths for encryption and decryption (using the Equivalent Inverse Cipher).](./aes_block_diagram.svg)

Inside the cipher core, both the data paths for the actual cipher (left) and the round key generation (right) are shared between encryption and decryption.
Consequently, the blocks shown in the diagram always implement the forward and backward (inverse) version of the corresponding operation.
For example, SubBytes implements both SubBytes and InvSubBytes.

Besides the actual AES cipher core, the AES unit features a set of control and status registers (CSRs) accessible by the processor via TL-UL bus interface, and a counter module (used in CTR mode only).
This counter module implements the Standard Incrementing Function according to [Recommendation for Block Cipher Modes of Operation (Appendix B.1)](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf) with a fixed parameter m = 128.
Note that for AES, parameter b = 128 and the counter increment is big-endian.
CFB mode is supported with a fixed parameter s = 128 (CFB-128).
Support for data segment sizes other than 128 bits would require a substantial amount of additional muxing resources and is thus not provided.
The initialization vector (IV) register and the register to hold the previous input data are used in CBC, CFB, OFB and CTR modes only.


## Hardware Interfaces

{{< incGenFromIpDesc "../data/aes.hjson" "hwcfg" >}}

The table below lists other signals of the AES unit.

Signal             | Direction        | Type                   | Description
-------------------|------------------|------------------------|---------------
`idle_o`           | `output`         | `logic`                | Idle indication signal for clock manager.
`lc_escalate_en_i` | `input`          | `lc_ctrl_pkg::lc_tx_t` | Life cycle escalation enable coming from [life cycle controller]({{< relref "hw/ip/lc_ctrl/doc" >}}). This signal moves the main controller FSM within the AES unit into the terminal error state. The AES unit needs to be reset.
`edn_o`            | `output`         | `edn_pkg::edn_req_t`   | Entropy request to [entropy distribution network (EDN)]({{< relref "hw/ip/edn/doc" >}}) for reseeding internal pseudo-random number generators (PRNGs) used for register clearing and masking.
`edn_i`            | `input`          | `edn_pkg::edn_rsp_t`   | [EDN]({{< relref "hw/ip/edn/doc" >}}) acknowledgment and entropy input for reseeding internal PRNGs.
`keymgr_key_i`     | `input`          | `keymgr_pgk::hw_key_req_t` | Key sideload request coming from [key manager]({{< relref "hw/ip/keymgr/doc" >}}).

Note that the `edn_o` and `edn_i` signals used to interface [EDN]({{< relref "hw/ip/edn/doc" >}}) follow a REQ/ACK protocol.
The entropy distributed by EDN is obtained from the [cryptographically secure random number generator (CSRNG)]({{< relref "hw/ip/csrng/doc" >}}).

## Design Details

This section discusses different design details of the AES module.


### Datapath Architecture and Operation

The AES unit implements the Equivalent Inverse Cipher described in the [AES specification](https://csrc.nist.gov/csrc/media/publications/fips/197/final/documents/fips-197.pdf).
This allows for more efficient cipher data path sharing between encryption/decryption as the operations are applied in the same order (less muxes, simpler control), but requires the round key during decryption to be transformed using an inverse MixColumns in all rounds except for the first and the last one.

This architectural choice targets at efficient cipher data path sharing and low area footprint.
Depending on the application scenario, other architectures might offer a more suitable area/performance tradeoff.
For example if only CFB, OFB or CTR modes are ever used, the inverse cipher is not used at all.
Moreover, if the key is changed extremely rarely (as for example in the case of bulk decryption), it may pay off to store all round keys instead of generating them on the fly.
Future versions of the AES unit might offer compile-time parameters to selectively instantiate the forward/inverse cipher part only to allow for dedicated encryption/decryption-only units.

All submodules in the data path are purely combinational.
The only sequential logic in the cipher and round key generation are the State, Full Key and Decryption Key registers.

The following description explains how the AES unit operates, i.e., how the operation of the AES cipher is mapped to the datapath architecture of the AES unit.
Phrases in italics apply to peculiarities of different block cipher modes.
For a general introduction into these cipher modes, refer to [Recommendation for Block Cipher Modes of Operation](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf).

1. The configuration and initial key is provided to the AES unit via a set of control and status registers (CSRs) accessible by the processor via TL-UL bus interface.
   The processor must first provide the configuration to the {{< regref "CTRL_SHADOWED" >}} register.
   Then follows the initial key.
   Each key register must be written at least once.
   The order in which the registers are written does not matter.
1. _The processor provides the initialization vector (IV) or initial counter value to the four IV registers via TL-UL bus interface in CBC, CFB and OFB modes, or CTR mode, respectively.
   Each IV register must be written at least once.
   The order in which the registers are written does not matter.
   Note that while operating, the AES unit automatically updates the IV registers after having consumed the current IV value.
   Whenever a new message is started, the processor must provide the corresponding IV value via TL-UL bus interface.
   In ECB mode, no IV needs to be provided.
   The content of the IV registers is ignored in ECB mode._
1. The input data is provided to the AES unit via four CSRs.
   Each input register must be written at least once.
   The order in which the registers are written does not matter.
1. If new input data is available, the AES unit automatically starts encryption/decryption by performing the following actions.
    1. The AES unit loads initial state into the State register inside the cipher core.

       _Depending on the cipher mode, the initial state is a combination of input data as well as IV._
       _Note, if CBC decryption is performed, or if running in CFB, OFB or CTR mode, the input data is also registered (Data In Prev in the block diagram)._
    2. The initial key is loaded into the Full Key register inside the cipher core.

       _Note, if the ECB/CBC decryption is performed, the Full Key register is loaded with the value stored in the Decryption Key register._

    _Note, for the AES unit to automatically start in CBC, CFB, OFB or CTR mode, also the IV must be ready.
    The IV is ready if -- since the last IV update (either done by the processor or the AES unit itself) -- all IV registers have been written at least once or none of them.
    The AES unit will not automatically start the next encryption/decryption with a partially updated IV._

    By setting the MANUAL_OPERATION bit in {{< regref "CTRL_SHADOWED" >}} to `1`, the AES unit can be operated in manual mode.
    In manual mode, the AES unit starts encryption/decryption whenever the START bit in {{< regref "TRIGGER" >}} is set to `1`, irrespective of the status of the IV and input data registers.

1. Once the State and Full Key registers have been loaded, the AES cipher core starts the encryption/decryption by adding the first round key to the initial state (all blocks in both data paths are bypassed).
   The result is stored back in the State register.
1. Then, the AES cipher core performs 9/11/13 rounds of encryption/decryption when using a 128/192/256-bit key, respectively.
   In every round, the cipher data path performs the four following transformations.
   For more details, refer to the [AES specification](https://csrc.nist.gov/csrc/media/publications/fips/197/final/documents/fips-197.pdf).
    1. SubBytes Transformation: A non-linear byte substitution that operates independently on each byte of the state using a substitution table (S-Box).
    2. ShiftRows Transformation: The bytes of the last three rows of the state are cyclically shifted over different offsets.
    3. MixColumns Transformation: Each of the four columns of the state are considered as polynomials over GF(2^8) and individually multiplied with another fixed polynomial.
    4. AddRoundKey Transformation: The round key is XORed with the output of the MixColumns operation and stored back into the State register.
       The 128-bit round key itself is extracted from the current value in the Full Key register.

    In parallel, the full key used for the next round is computed on the fly using the key expand module.

    _If running in CTR mode, the counter module iteratively updates the IV in parallel to the cipher core performing encryption/decryption.
    Internally, the counter module uses one 16-bit counter, meaning it requires 8 clock cycles to increment the 128-bit counter value stored in the IV register.
    Since the counter value is used in the first round only, and since the encryption/decryption of a single block takes 12/14/16 cycles, the iterative counter implementation does not affect the throughput of the AES unit._
1. Finally, the AES cipher core performs the final encryption/decryption round in which the MixColumns operation is skipped.
   The output is forwarded to the output register in the CSRs but not stored back into the State register.
   The internal State register is cleared with pseudo-random data.

   _Depending on the cipher mode, the output of the final round is potentially XORed with either the value in the IV registers (CBC decryption) or the value stored in the previous input data register (CFB, OFB, CTR modes), before being forwarded to the output register in the CSRs.
   If running in CBC mode, the IV registers are updated with the output data (encryption) or the value stored in the previous input data register (decryption).
   If running in CFB or OFB mode, the IV registers are updated with the output data or the output of the final cipher round (before XORing with the previous input data), respectively._

Having separate registers for input, output and internal state prevents the extraction of intermediate state via TL-UL bus interface and allows to overlap reconfiguration with operation.
While the AES unit is performing encryption/decryption, the processor can safely write the next input data block into the CSRs or read the previous output data block from the CSRs.
The State register is internal to the AES unit and not exposed via the TL-UL bus interface.
If the AES unit wants to finish the encryption/decryption of an output data block but the previous one has not yet been read by the processor, the AES unit is stalled.
It hangs and does not drop data.
It only continues once the previous output data has been read and the corresponding registers can be safely overwritten.
The order in which the output registers are read does not matter.
Every output register must be read at least once for the AES unit to continue.
In contrast, the initial key, and control register can only be updated if the AES unit is idle, which eases design verification (DV).
Similarly, the initialization vector (IV) register can only be updated by the processor if the AES unit is idle.
If the AES unit is busy and running in CBC or CTR mode, the AES unit itself updates the IV register.

The cipher core architecture of the AES unit is derived from the architecture proposed by Satoh et al.: ["A compact Rijndael Hardware Architecture with S-Box Optimization"](https://link.springer.com/chapter/10.1007%2F3-540-45682-1_15).
The expected circuit area in a 110nm CMOS technology is in the order of 12 - 22 kGE (unmasked implementation, AES-128 only).
The expected circuit area of the entire AES unit with masking enabled is around 110 kGE.

For a description of the various sub modules, see the following sections.


### SubBytes / S-Box

The SubBytes operation is a non-linear byte substitution that operates independently on each byte of the state using a substitution table (S-Box).
It is both used for the cipher data path and the key expand data path.
In total, the AES unit instantiates 20 S-Boxes in parallel (16 for SubBytes, 4 for KeyExpand), each having 8-bit input and output.
In combination with the 128-bit wide data path, this allows to perform one AES round per iteration.

The design of this S-Box and its inverse can have a big impact on circuit area, timing critical path, robustness and power leakage, and is itself its own research topic.

The S-Boxes are decoupled from the rest of the AES unit with a handshake protocol, allowing them to be easily replaced by different implementations if required.
The AES unit comes with the following S-Box implementations that can be selected by a compile-time Verilog parameter:
- Domain-oriented masking (DOM) S-Box: default, see [Gross et al.: "Domain-Oriented Masking: Compact Masked Hardware Implementations with Arbitrary Protection Order"](https://eprint.iacr.org/2016/486.pdf)
- Masked Canright S-Box: provided for reference, usage discouraged, a version w/ and w/o mask re-use is provided, see [Canright and Batina: "A very compact "perfectly masked" S-Box for AES (corrected)"](https://eprint.iacr.org/2009/011.pdf)
- Canright S-Box: only use when disabling masking, recommended when targeting ASIC implementation, see [Canright: "A very compact Rijndael S-Box"](https://hdl.handle.net/10945/25608)
- LUT-based S-Box: only use when disabling masking, recommended when targeting FPGA implementation

The DOM S-Box has a latency of 5 clock cycles.
All other implementations are fully combinational (one S-Box evaluation every clock cycle).
See also [Security Hardening below.]({{< relref "#1st-order-masking-of-the-cipher-core" >}})

### ShiftRows

The ShiftRows operation simply performs a cyclic shift of Rows 1, 2 and 3 of the state matrix.
Consequently, it can be implemented using 3\*4 32-bit 2-input muxes (encryption/decryption).


### MixColumns

Each of the four columns of the state are considered as polynomials over GF(2^8) and individually multiplied with another fixed polynomial.
The whole operation can be implemented using 36 2-input XORs and 16 4-input XORs (all 8-bit), 8 2-input muxes (8-bit), as well as 78 2-input and 24 3-input XOR gates.


### KeyExpand

The key expand module (KEM) integrated in the AES unit is responsible for generating the various round keys from the initial key for both encryption and decryption.
The KEM generates the next 128/192/256-bit full key in parallel to the actual encryption/decryption based on the current full key or the initial key (for the first encryption round).
The actual 128-bit round key is then extracted from this full key.

Generating the keys on-the-fly allows for lower storage requirements and smaller circuit area but comes at the price of an initial delay before doing ECB/CBC **decryption** whenever the key is changed.
During this phase, the KEM cycles through all full keys to obtain the start key for decryption (equals the key for final round of encryption).
The duration of this delay phase corresponds to the latency required for encrypting one 16B block.
During this initial phase, the cipher data path is kept idle.

The timing diagram below visualizes this process.

```wavejson
{
  signal: [
    {    name: 'clk',       wave: 'p........|.......'},
    ['TL-UL IF',
      {  name: 'write',     wave: '01...0...|.......'},
      {  name: 'addr',      wave: 'x2345xxxx|xxxxxxx', data: 'K0 K1 K2 K3'},
      {  name: 'wdata',     wave: 'x2345xxxx|xxxxxxx', data: 'K0 K1 K2 K3'},
    ],
    {},
    ['AES Unit',
      {  name: 'Config op', wave: 'x4...............', data: 'DECRYPT'},
      {  name: 'AES op',    wave: '2........|.4.....', data: 'IDLE DECRYPT'},
      {  name: 'KEM op',    wave: '2....3...|.4.....', data: 'IDLE ENCRYPT DECRYPT'},
      {  name: 'round',     wave: 'xxxxx2.22|22.2222', data: '0 1 2 9 0 1 2 3 4'},
      {  name: 'key_init',  wave: 'xxxx5....|.......', data: 'K0-3'},
      {  name: 'key_full',  wave: 'xxxxx5222|4.22222', data: 'K0-3 f(K) f(K) f(K) K0-3\' f(K) f(K) f(K) f(K) f(K)'},
      {  name: 'key_dec',   wave: 'xxxxxxxxx|4......', data: 'K0-3\''},
    ]
  ]
}
```

The AES unit is configured to do decryption (`Config op` = DECRYPT).
Once the new key has been provided via the control and status registers (top), this new key is loaded into the Full Key register (`key_full` = K0-3) and the KEM starts performing encryption (`KEM op`=ENCRYPT).
The cipher data path remains idle (`AES op`=IDLE).
In every round, the value in `key_full` is updated.
After 10 encryption rounds, the value in `key_full` equals the start key for decryption.
This value is stored into the Decryption Key register (`key_dec` = K0-3' at the very bottom).
Now the AES unit can switch between encryption/decryption without overhead as both the start key for encryption (`key_init`) and decryption (`key_dec`) can be loaded into `full_key`.

For details on the KeyExpand operation refer to the [AES specification, Section 5.2](https://csrc.nist.gov/csrc/media/publications/fips/197/final/documents/fips-197.pdf).

Key expanding is the only operation in the AES unit for which the functionality depends on the selected key length.
Having a KEM that supports 128-bit key expansion, support for the 256-bit mode can be added at low overhead.
In contrast, the 192-bit mode requires much larger muxes.
Support for this mode is thus optional and can be enabled/disabled via a design-time parameter.

Once we have cost estimates in terms of gate count increase for 192-bit mode, we can decide on whether or not to use it in OpenTitan.
Typically, systems requiring security above AES-128 go directly for AES-256.

### System Key-Manager Interface

By default, the AES unit is controlled entirely by the processor.
The processor writes both input data as well as the initial key to dedicated registers via the system bus interconnect.

Alternatively, the processor can configure the AES unit to use an initial key provided by the [key manager]({{< relref "hw/ip/keymgr/doc" >}}) via key sideload interface without exposing the key to the processor or other hosts attached to the system bus interconnect.
To this end, the processor has to set the SIDELOAD bit in {{< regref "CTRL_SHADOWED" >}} to `1`.
Any write operations of the processor to the Initial Key registers {{< regref "KEY_SHARE0_0" >}} - {{< regref "KEY_SHARE1_7" >}} are then ignored.
In normal/automatic mode, the AES unit only starts encryption/decryption if the sideload key is marked as valid.
To update the sideload key, the processor has to 1) wait for the AES unit to become idle, 2) wait for the key manager to update the sideload key and assert the valid signal, and 3) write to the {{< regref "CTRL_SHADOWED" >}} register to start a new message.
After using a sideload key, the processor has to trigger the clearing of all key registers inside the AES unit (see [De-Initialization]({{< relref "#de-initialization" >}}) below).


