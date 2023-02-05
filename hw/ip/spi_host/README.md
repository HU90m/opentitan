# SPI_HOST HWIP Technical Specification

This document specifies SPI_HOST hardware IP (HWIP) functionality.
This module conforms to the [Comportable guideline for peripheral functionality.]({{< relref "doc/rm/comportability_specification/index.md" >}})
See that document for integration overview within the broader top-level system.

## Features

- Hardware control for remote devices using the Serial Peripheral Interface (SPI)
- Primarily designed for serial NOR flash devices such as the [Winbond W25Q01JV](https://www.winbond.com/resource-files/W25Q01JV%20SPI%20RevB%2011132019.pdf)
- Number of chip select lines controlled by `NumCS` parameter
- Support for Standard SPI, Dual SPI or Quad SPI commands
  - Signals SD[0] through SD[3] are intended to connect to lines IO<sub>0</sub> through IO<sub>3</sub> respectively, on the target device.
  - Signal SD[0] may also be identified as "MOSI" by other SPI Hosts, while SD[1] is commonly referred to as "MISO"
- RX and TX data held in separate FIFOs
   - 288 bytes for TX data, 256 bytes for RX data
   - FIFOs loaded/unloaded via 32-bit TL-UL registers
   - Support for arbitrary byte-count in each transaction
   - Parametrizable support for Big- or Little-Endian systems in ordering I/O bytes within 32-bit words.
- SPI clock rate controlled by separate input clock to core
   - SPI SCK line typically toggles at 1/2 the core clock frequency
   - An additional clock rate divider exists to reduce the frequency if needed
- Support for all SPI polarity and phases (CPOL, CPHA)
   - Additional support for "Full-cycle" SPI transactions, wherein data can be read a full SPI Clock cycle after the active edge (as opposed to one half cycle as is typical for SPI interfaces)
- Single Transfer Rate (STR) only (i.e. data received on multiple lines, but only on one clock edge)
   - *No support* for Dual Transfer Rate (DTR)
- Pass-through mode for coordination with [SPI_DEVICE IP]({{< relref "hw/ip/spi_device/doc/_index.md" >}})
- Automatic control of chip select lines
- Condensed interrupt footprint: Two lines for two distinct interrupt classes: "error" and "spi_event"
   - Fine-grain interrupt masking supplied by secondary enable registers

## Description

The Serial Peripheral Interface (SPI) is a synchronous serial interface quite commonly used for NOR flash devices as well as a number of other off-chip peripherals such as ADC's, DAC's, or temperature sensors.
The interface is a *de facto* standard (not a formal one), and so there is no definitive reference describing the interface, or establishing compliance criteria.

It is therefore important to consult the data sheets for the desired peripheral devices in order to ensure compatibility.
For instance, this OpenTitan SPI Host IP is primarily designed for controlling Quad SPI NOR flash devices, such as the [W25Q01JV Serial NOR flash from Winbond](https://www.winbond.com/resource-files/W25Q01JV%20SPI%20RevB%2011132019.pdf) or this [1 Gb M25QL NOR flash from Micron](https://media-www.micron.com/-/media/client/global/documents/products/data-sheet/nor-flash/serial-nor/mt25q/die-rev-b/mt25q_qlkt_l_01g_bbb_0.pdf?rev=43d124f03bbf4ef0962435e9ec63a185).
Though this IP implementation aims to be general enough to support a variety of devices, the Winbond serial flash device is used as the primary reference for understanding our host requirements.

There are also a number of good references describing legacy host implementations for this protocol, which are useful for understanding some of the general needs for a wider range of target devices.
For instance, the legacy [SPI Block Guide](https://web.archive.org/web/20150413003534/http://www.ee.nmt.edu/~teare/ee308l/datasheets/S12SPIV3.pdf) from Motorola contains a definitive overview of some of the the general requirements for a standard SPI host, notably the definitions of SPI clock phase and polarity (CPOL and CPHA).
In order to potentially support a broad range of devices, this SPI Host IP also supports all four of the standard SPI clock phases.

### SPI Protocol Basics

Broadly speaking, the SPI host accepts commands from the TL-UL interface and, based on these commands, serially transmits and receives data on the external SPI interface pins.
The timing and data-line formatting of the command sequence depend on the external peripheral device and the nature of the specific command issued.

In each standard SPI command a number of instruction-, address- or data-bytes are transmitted on SD[0], and response bytes are received on SD[1].
So in standard-mode commands, SD[0] is always configured as an output, and SD[1] is always an input.
In standard SPI commands the SD[0] and SD[1] lines can be used as a full-duplex communication channel.
Not all devices exploit this capability, opting instead to have clear input and output phases for each command.
This half-duplex signaling behavior is especially common in devices with also support Dual- and Quad-mode commands, which are always half-duplex.
The SPI_HOST IP optionally supports both full-duplex and half-duplex commands in standard mode.

Along with the data lines, the SPI protocol also includes a chip select line, commonly called CS\#.
In this IP we refer to it as CSB.
The SPI bus can be connected to many target peripherals, but each device on the bus gets its own chip select line, and so this active-low signal designates a particular device for each command.

The chip-select line also marks the beginning and end of each command.
No device will accept any command input until CSB has been asserted for that device, and most devices (if not all) do not accept a second command unless CSB has been deasserted to mark the end of the previous command.
Some simple devices, particularly those that support SPI daisy-chaining, do not process command input at all until *after* the CSB line has been deasserted.
In the case of NOR flash devices, read and write commands are indeterminate in length, and the data transfer ends only when the CSB line is deasserted.
So, though the exact details of operation may vary from device to device, the edges of the CSB signal serve as an important markers for delineating the boundaries of each transaction.

The SD and CSB lines are accompanied by a serial clock, SCK.
The host is responsible for generating the serial clock, and typically each side asserts outgoing data on one edge of the clock (e.g. on the rising edge) and samples data in the next edge (e.g. on the falling edge).
When it comes to devices there is no universal convention on clock polarity (CPOL) or clock phase (CPHA).
Some devices expect the clock line to be low when the host is idle, thus the clock should come as a sequence of positive pulses (CPOL = 0).
On the other hand, other devices expect CPOL = 1, meaning that the clock line is inverted: *high* when idle with sequences of *negative* pulses.

Devices also differ in their expectations of clock *phase* (CPHA) relative to the data.
Devices with CPHA = 0, expect that both the host and device will be sampling data on the *leading* edge, and asserting data on the *trailing* edge.
(Because of the option for either polarity, the terms "leading" and "trailing" are preferred to "rising" or "falling").
When CPHA = 0, the first output bit is asserted with the falling edge of CSB.
Meanwhile if CPHA = 1, data is always is asserted on the leading edge of SCK, and data is always sampled on the trailing edge of SCK.

When operating at the fastest-rated clock speeds, some flash devices (i.e. both the Winbond and Micron devices noted above), require setup times which exceed half a clock-cycle.
In order to support these fastest data rates, the SPI_HOST IP offers a modified "Full-cycle" (FULLCYC = 1) timing mode where data can be sampled a *full* cycle after the target device asserts data on the SD bus.
This full cycle mode has no effect on any of the signals transmitted, only on the timing of the sampling of the incoming signals.

```wavejson
{signal: [
  {name: "clk_i", wave: "p.................."},
  {name: "SCK (CPOL=0)", wave: "0.1010101010101010."},
  {name: "SCK (CPOL=1)", wave: "1.0101010101010101."},
  {name: "CSB", wave: "10................1"},
  ["CPHA = 0",
    {name: "SD[0] (output)", wave: "22.2.2.2.2.2.2.2...", data: ["","out7", "out6", "out5", "out4", "out3", "out2", "out1", "out0" ]},
    {name: "SD[1] (input)", wave: "22.2.2.2.2.2.2.2.2.", data: ["","in7", "in6", "in5", "in4", "in3", "in2", "in1", "in0",""]},
    {name: "Sampling event (FULLCYC=0)", wave: "1.H.H.H.H.H.H.H.H.."},
    {name: "Sampling event (FULLCYC=1)", wave: "1..H.H.H.H.H.H.H.H."},
  ],
  ["CPHA = 1",
    {name: "SD[0] (output)", wave: "2.2.2.2.2.2.2.2.2..", data: ["","out7", "out6", "out5", "out4", "out3", "out2", "out1", "out0" ]},
    {name: "SD[1] (input)", wave: "2.2.2.2.2.2.2.2.2.2", data: ["","in7", "in6", "in5", "in4", "in3", "in2", "in1", "in0",""]},
    {name: "Sampling event (FULLCYC=0)", wave: "1..H.H.H.H.H.H.H.H."},
    {name: "Sampling event (FULLCYC=1)", wave: "1...H.H.H.H.H.H.H.H"},
  ],
],
  head: {
    text: "Standard SPI transaction (1 byte), illustrating of the impact of the CPOL, CPHA, and FULLCYC settings"
  },
  foot: {
  }
}
```

As mentioned earlier, the SD[0] and SD[1] lines are unidirectional in Standard SPI mode.
On the other hand in the faster Dual- or Quad-modes, all data lines are bidirectional, and in Quad mode the number of data lines increases to four.
For the purposes of this IP, Dual or Quad-mode commands can be thought of as consisting of up to four command *segments* in which the host:
1. Transmits instructions or data at single-line rate,
2. Transmits instructions address or data on 2 or 4 data lines,
3. Holds the bus in a high-impedance state for some number of "dummy" clock cycles (neither side transmits), or
4. Receives information from the target device.

Each of these segments have a different directionality or speed (i.e., SD bus width).
As indicated in the example figure below, input data need only be sampled during the last segment.
Likewise, software-provided data is only transmitted in the first two segments.
The SPI_HOST command interface allows the user to specify any number of command segments to build larger, more complex transactions.

```wavejson
{signal: [
  {name: "clk_i",        wave: "p................................"},
  {name: "SCK (CPOL=0)", wave: "0.101010101010101010101010101010."},
  {name: "CSB", wave: "10..............................1"},
  {name: "SD[0]", wave: "22.2.2.2.2.2.2.2.2.2.z.....2.2.x.", data: ["","cmd7", "cmd6", "cmd5", "cmd4", "cmd3", "cmd2", "cmd1", "cmd0",
                                                             "out4", "out0", "in4", "in0" ]},
  {name: "SD[1]", wave: "z................2.2.z.....2.2.x.", data: ["out5", "out1", "in5", "in1"]},
  {name: "SD[2]", wave: "z................2.2.z.....2.2.x.", data: ["out6", "out2", "in6", "in2"]},
  {name: "SD[3]", wave: "z................2.2.z.....2.2.x.", data: ["out7", "out3", "in7", "in3"]},
  {name: "Segment number", wave: "x2...............3...4.....5...x.", data: ['1', '2', '3','4'] },
  {name: "Segment speed", wave: "x2...............3...4.....5...x.", data: ['Standard', 'Quad', 'X', 'Quad'] },
  {name: "Segment direction", wave: "x2...............3...4.....5...x.", data: ['TX', 'TX', 'None', 'RX'] },
  {name: "Sampling event (FULLCYC=0)", wave: "1...........................H.H.."},
  {name: "Sampling event (FULLCYC=1)", wave: "1............................H.H."},
],
  head: {
  text: "Example Quad SPI transaction: 1 byte TX (Single), 1 byte (Quad), 3 dummy cycles and 1 RX byte with CPHA=0"
  },
}
```

For even faster transfer rates, some flash chips support double transfer rate (DTR) variations to the SPI protocol wherein the device receives and transmits fresh data on *both* the leading and trailing edge.
This IP only supports single transfer rate (STR), *not* DTR.
A preliminary investigation of DTR transfer mode suggests that proper support for setup and hold times in this mode may require a level of sub-cycle timing control which is not currently planned for this IP.

## Analysis of Transient Datapath Stalls

Even if the RX (or TX) FIFOs have free-space (or data) available, stall events can still happen due to momentary backlogs or bubbles in the data pipeline.
For instance, the Byte Merge and Byte Select blocks occasionally need some extra cycles to clean out the internal `prim_packer_fifo`.
These delays are likely to cause transient stalls particularly, when constructing transactions with many short (less than 4-byte) segments.
Transient stalls could lead to false diagnostics when trying to optimize SPI_HOST throughput.
Thus it is useful to analyze the shift register's tolerance to bubble events, particularly for the highest bandwidth Quad SPI mode.

### Transient Stalls in TX direction.

The transient analysis stall analysis is simpler for the TX direction.
There is no buffering on the shift register TX data inputs because it would complicate the `byte_flush` operation on the Byte Select block.

In Quad mode,the shift register will demand one new byte as often as once every four clock cycles.
(This rate is slowed down if for a non-trivial clock-divide ratio).
Meanwhile, the Byte Select Block pauses once for every disabled byte, and once more at the end of each word.
Thus if the Byte Select block is loaded with three-consecutive bytes-disables (either in the same word or across two separate words), this will create a pause of 4-clock cycles between two bytes causing a transient stall event.

Assuming that each TX Word has at least one byte enabled, the longest possible transient delay between two Byte Select outputs is 7 clock cycles (with three byte-disables in two adjacent words, respectively aligned for maximal delay and assuming no delays in the TX FIFOs themselves).
Dual- and Standard-mode segments can tolerate inter-byte delays of 7 or 15 clocks respectively, and thus transient stalls should not be a problem after Dual- or Standard-mode segments.

### Transient Stalls in the RX direction

Similar to the Byte Select, the Byte Merge block must pause for at least one cycle between each word.
Also when at the end of a segment the Byte Merge packs less than four bytes into the last word, there is also an additional cycle of delay for each unused byte.
Thus if the last word in a given segment has only one valid byte, the total delay will be four clock cycles.

Such stalls however are a much smaller concern in the RX direction due to the buffering of the Shift Register outputs.
As shown in the following waveform, even in Quad-mode, this buffer means the shift register can tolerate as many as six clock cycles of temporary back-pressure before creating a stall.

```wavejson
{signal: [
  [ "Shift Register Ports",
  {name: "clk_core_i",                  wave: "p..........................."},
  {name: "wr_en_i",                     wave: "010..10..10..10..10..10..1.0"},
  {name: "shift_en_i",                  wave: "0..10..10..10..10..10..10..."},
  {name: "rd_en_i",                     wave: "0....10..10..10..10..10..1.0"},
  {name: "rx_valid_o (to Byte Merge)",   wave: "0.....10..1....0..10..1....."},
  {name: "rx_ready_i (from Byte Merge)", wave: "1......0.....1.....0......1.",
                                        node: ".......A.....B.....C......D"},
  {name: "rd_ready_o (to FSM)",         wave: "1.........0..1........0...1."}],
  ["FSM Port", {name: "rx_stall_o",     wave: "0........................10."}],
  {name: ""}
],
  edge: ["A<->B 6 clocks: No Stall", "C<->D 7 clocks will stall FSM"],
  head: {text: "SPI_HOST Shift Register: Tolerance to Gaps in rx_ready_i", tick:1}
}
```

Even though such long delays are tolerable, it takes some time for shift register to catch up completely and clear the backlog.
For example, if after a 6-clock delay the shift-register encounters another 4-clock backlog this can also introduce a stall condition, as shown in the waveform below.

```wavejson
{signal: [
  ["Shift Register Ports",
  {name: "clk_core_i", wave: "p........................"},
  {name: "wr_en_i",    wave: "010..10..10..10..1.0..10."},
  {name: "shift_en_i", wave: "0..10..10..10..10...10..1"},
  {name: "rd_en_i",    wave: "0....10..10..10..1.0..10."},
  {name: "rx_valid_o", wave: "0.....10..1...........010"},
  {name: "rx_ready_i (from Byte Merge)", wave: "1......0.....10...10.1...",
                      node: ".......A.....BC...D"},
  {name: "rd_ready_o (to FSM)", wave: "1.........0..10...10.1..."}],
  ["FSM Port", {name: "rx_stall_o", wave: "0................10......"}],
  {name: "", wave: ""},
],
  edge: ["A<->B 1st Gap: 6 clocks", "C<->D 2nd Gap: 4 clocks"],
  head: {text: "SPI_HOST Shift Register: Back-to-back gaps in rx_ready_i", tick:1}
}
```

Delays of 3-clocks or less do not create any internal backlog in the system.
However, the Byte Merge block can create a 4-clock delay each time it processes a single-byte segment.
In practice, this is unlikely to cause a problem, as no Quad-SPI Flash transactions require even two back-to-back RX segments.
However with enough (at least six) consecutive one-byte segments, the accumulated delay can eventually create a stall event on the RX path as well, as seen below.

```wavejson
{signal: [
 [ "Shift Register Ports",
  {name: "clk_core_i", wave: "p..........................."},
  {name: "wr_en_i", wave: "010..10..10..10..10..10..1.0"},
  {name: "shift_en_i", wave: "0..10..10..10..10..10..10..."},
  {name: "rd_en_i", wave: "0....10..10..10..10..10..1.0"},
  {name: "rx_valid_o", wave: "0.....10..1.0.1..01........."},
  {name: "rx_ready_i (from Byte Merge)", wave: "1......0...10...10...10...10",
                      node: ".......A...BC...D"},
  {name: "rd_ready_o (to FSM)", wave: "1.........01..0.1.0..10...10"}],
  [ "FSM Port",
  {name: "rx_stall_o", wave: "0........................10."}],
  {name: ""}
],
  edge: ["A<->B 4 clocks", "C<->D 4 clocks"],
  head: {text: "SPI_HOST Shift Register: Hypothetical RX Congestion Scenario", tick:1},
 foot: {text: "Six back-to-back quad reads 1-byte each, same CSID, CSAAT enabled"}
}
```
