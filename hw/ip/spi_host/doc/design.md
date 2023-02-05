# Design Details

## Component Overview

Transaction data words flow through the SPI_HOST IP in a path which starts with the TX FIFOs, shown in the block diagram above.
At the output of the TX FIFOs each data word is separated into individual bytes by the Byte Select block, which is also responsible for parsing the byte-enable mask and discarding unwanted bytes.
Selected bytes are then passed into the shift register, where they are played out at Standard, Dual, or Quad speed.
For receive segments, outputs from the shift register are passed into the Byte Merge block to be packed into 32-bit words.
Finally the repacked words are inserted into the RX FIFO to be read by firmware.

All of the blocks in the data path use ready-valid handshakes for flow control.
In addition, the Byte Select block expects a `flush` pulse from the shift register to signify when no further data is needed for the current segment, and so any remaining data in the current word can be discarded.
Likewise, the Byte Merge block receives a `last` signal from the shift register to identify the end of a command segment so that any partial words can be passed into the RX FIFO (regardless of whether the last byte forms a complete 32-bit word).
The shift register is then responsible for driving and receiving data on the `cio_sd` lines.
It coordinates all of the data flow to and from the Byte Select and Byte Merge blocks.

The SPI_HOST FSM parses the software command segments and orchestrates the proper transmission of data through its control of the shift register.
The FSM directly drives the `cio_sck` and `cio_csb` signals at the commanded speed.
It also controls the shift register: dictating the correct timing for sending out each beat of data, loading bytes from the Byte Select, and sending bytes on to the Byte Merge block.

## RX and TX FIFOs

The RX and TX FIFOs store the transmitted and received data, which are stored in synchronous FIFOs.
The RX FIFO is 32 bits wide, matching the width of the TLUL register bus.
The TX FIFO on the other hand is 36 bits wide, with 32 bits of SPI data (again to match the TLUL bus width) plus 4 byte enable-bits, which are passed into the core to allow the processing of unaligned writes.

The depth of these FIFOs is controlled by two independent parameters for the RX and TX queues.

## Byte Select

The Byte Select unit is responsible for loading words from the FIFO and feeding individual bytes into the shift register.
This unit takes two data inputs: a data word, `word_i[31:0]`, and a byte enable signal, `word_be_i[3:0]`.
There is a single output, `byte_o[7:0]`, which feeds the following shift register.
There are ready/valid signals for managing flow control on all inputs and outputs.
The shift register asserts ready to request new bytes, based on control inputs from the SPI_HOST FSM.

When the SPI_HOST FSM indicates the final byte for a segment, the shift register asserts the `flush_i` signal with `byte_ready_i` as it requests the last byte from the Byte Select.
This instructs the Byte Select block to send one more byte from current word, and then discard any remaining unused bytes, before immediately loading the next available word from the TX FIFO.

It is assumed that the input data-words and byte enables have already been byte-swapped at the IP top level, as needed.
The bytes are transmitted to the shift register in decreasing significance, starting with `word_i[31:24]`, followed by `word_i[23:16]`, `word_i[15:8]`, and finally `word_i[7:0]`.

Some bytes may be skipped however if the corresponding value of `word_be_i[3:0]` is zero.
For example if `word_be_i[3:0]` equals `4'b0011`, then the first two input bytes will be skipped, and only `word_i[15:8]` and `word_i[7:0]` will be forwarded, in that order.

The following waveform illustrates the operation of the Byte Select module, highlighting the effect of the `flush_i` signal (in the first input word), as well as the effect of the byte enable signal (shown in the second word).

```wavejson
{signal: [
  {name: "clk_i", wave:           "p............."},
  {name: "word_i[31:0]", wave:    "x2..x2...x....", data: ["32'hBEADCAFE", "32'hDAD5F00D"]},
  {name: "word_be_i[31:0]", wave: "x2..x2...x....", data: ["4'b1111", "4'b0011"]},
  {name: "word_valid_i", wave:    "0..101...0...."},
  {name: "word_ready_o",wave:     "1...0...10...."},
  {name: "byte_o[7:0]", wave:     "x...2222.2222x", data: ["BE", "AD", "CA", "0", "DA", "D5", "F0", "0D"]},
  {name: "byte_valid_o", wave:    "0...1..0...1.0"},
  {name: "byte_ready_i", wave:    "1............."},
  {name: "byte_flush_i", wave:    "0.....10......"},
  ],
  head: {
  text: "Byte Select Operation"
  }
}
```

## Byte Merge

The Byte Merge block is responsible for accumulating bytes from the shift register and packing them into words.
Like the Byte Select block, it is based on the `prim_packer_fifo` primitive.

The Byte Merge block has a data byte input, and a data word output, which are both controlled by their corresponding ready/valid signals.
There are no byte-enable outputs for the byte merge, as it is assumed that software can infer the relevant bytes based on the length of the relevant read command segment.

There is `byte_last_i` signal, to indicate the final byte in a word.
If `byte_last_i` is asserted whenever a byte is loaded, the new byte will be added to the output word, and any remaining bytes will be set to zero, before the word is be loaded into the RX FIFO.

Input bytes are packed into the output word in decreasing significance.
The first byte in each segment is loaded into `word_o[31:24]`.
The following bytes are packed into `word_o[23:16]`, `word_o[15:8]`, and then `word_o[7:0]`.
For partially filled words, the zero padding goes into the least significant byte positions.

Any ByteOrder swapping is performed at the other end of the RX FIFO.

```wavejson
{signal: [
  {name: "clk_i",        wave: "p.............."},
  {name: "byte_i[7:0]",  wave: "x22222.2....22x", data: ["01", "02", "03", "04", "05", "06", "07", "08"]},
  {name: "byte_valid_i", wave: "01............."},
  {name: "byte_last_i",  wave: "0....1.0......."},
  {name: "byte_ready_o", wave: "1....010...1..."},
  {name: "word_o[31:0]", wave: "2.2222222222222", data: ["0", "01","0102","010203", "01020304", "0", "05", "0500", "05000", "050000", "0", "06", "0607", "060708"]},
  {name: "word_valid_o", wave: "0....10...10..."},
  {name: "word_ready_i", wave: "1.............."}
  ],
 config: {hscale:2},
  head: {
  text: "Byte Merge Operation"
  }
}
```

## Shift Register

The SPI_HOST shift register serially transmits and receives all bytes to the `sd_o[3:0]` and `sd_i[3:0]` signals, based on the following timing-control signals from the FSM:
- `speed_i`: Controls the speed of the current data segment, ranging from `Standard` or `Dual` to `Quad`
- `wr_en_i`: Writes a new byte from the Byte Select into the 8-bit shift register
This is usually the first signal issued to the shift register in command segments with data to transmit (i.e., TX only, or bidirectional segments)
   - There is also a `wr_ready_o` output to tell the FSM that there is no data currently available.
     If `wr_ready_o` is deasserted when the FSM asserts `wr_en_i`, the FSM will stall.
- `last_write_i`: When asserted at the same time as `wr_en_i`, this indicates that the current byte is the last of its command segment, and thus the `tx_flush_o` signal should be asserted when requesting this byte from the Byte Select block.
- `shift_en_i`: Advances the shift register by 1, 2, or 4 bits, depending on the value of `speed_i`
- `full_cyc_i`: Indicates full-cycle operation (i.e., input data are sampled from `sd_i` whenever new data is shifted out to `sd_o`)
- `sample_en_i`: Samples `sd_i[3:0]` into a temporary register, `sd_i_q[3:0]` so it can be loaded into the shift register with the next assertion of `shift_en_i`
Explicit sampling is particularly required for Standard SPI bidirectional segments, where new input data arrives before the first output shift operation.
For consistency in timing, the `sd_i_q` buffer is used in all other modes as well, unless `full_cyc_i` is asserted.
The `sample_en_i` signal is ignored during full-cycle operation, in which case data is copied directly into the shift register during shift operations.
- `rd_en_i`: Indicates that the current byte from the shift register should be transferred on to the Byte Merge block
   - The `rd_ready_o` output informs the FSM whenever all data storage (the RX FIFO plus any intervening buffers) is full and no further data can be acquired.
- `last_read_i`: When asserted at the same time as `rd_en_i`, this indicates that the current byte is the last of its command segment, and thus the `rx_last_o` signal should be asserted when passing this byte to the Byte Merge block.

```wavejson
{signal: [
  {name: "clk_i",                   wave: "p.........................."},
 [ "External signals",
  {name: "TX DATA[31:0] (TX FIFO)", wave: "2..........................", data:"0x123456XX"},
  {name: "cio_sck_o (FSM)",         wave: "0...1010101010101010101010."},
 ],
  {name: "cio_csb_o[0] (FSM)",      wave: "1..0......................."},
  {name: "tx_data_i[7:0]",          wave: "2..2...............2.......", data:["0x12", "0x34", "0x56"]},
  {name: "tx_valid_i",              wave: "1.........................."},
  {name: "tx_ready_o/wr_en_i",      wave: "0.10..............10......."},
  {name: "sample_en_i",             wave: "0..101010101010101010101010"},
  {name: "shift_en_i",              wave: "0...10101010101010..1010101"},
  {name: "speed_i[1:0]",            wave: "2..........................", data: ["0 (Standard SPI)"]},
  {name: "sd_i[1]",                 wave: "x..1.1.0.0.1.1.1.1.0.1.0.1."},
  {name: "sd_i_q[1]",               wave: "x...1.1.0.0.1.1.1.1.0.1.0.1"},
  {name: "sr_q[0]",                 wave: "x..0.1.1.0.0.1.1.1.0.1.0.1."},
  {name: "sr_q[1]",                 wave: "x..1.0.1.1.0.0.1.1.0.0.1.0."},
  {name: "sr_q[2]",                 wave: "x..0.1.0.1.1.0.0.1.1.0.0.1."},
  {name: "sr_q[3]",                 wave: "x..0.0.1.0.1.1.0.0.0.1.0.0."},
  {name: "sr_q[4]",                 wave: "x..1.0.0.1.0.1.1.0.1.0.1.0."},
  {name: "sr_q[5]",                 wave: "x..0.1.0.0.1.0.1.1.1.1.0.1."},
  {name: "sr_q[6]",                 wave: "x..0.0.1.0.0.1.0.1.0.1.1.0."},
  {name: "sr_q[7]",                 wave: "x..0.0.0.1.0.0.1.0.0.0.1.1."},
  {name: "sr_q[7:0] (hex)",         wave: "x..4.2.2.2.2.2.2.2.4.2.2.2.",
   data: ["0x12", "0x25", "0x4B", "0x96", "0x2c", "0x59", "0xB3", "0x67", "0x34", "0x69", "0xD2", "0xA5"]},
  {name: "Load Input Data Event",   wave: "1..H...............H......."},
  {name: "rx_data_o[7:0]", wave: "x..................2.......", data: ["0xcf"]},
  {name: "rx_valid_o[7:0]/rd_en_i", wave: "0.................10......."},
  {name: "sd_o[0] (sr_q[7])", wave: "x..0.0.0.1.0.0.1.0.0.0.1.1."},
],
head: {
  text: "Shift Register During Standard SPI Transaction: Simultaneous Receipt and Transmission of Data."
},
}
```

The connection from the shift register to the `sd` bus depends on the speed of the current segment.
- In Standard-mode, only the most significant shift register bit, `sr_q[7]` is connected to the outputs using `sd_o[0]`.
In this mode, each `shift_en_i` pulse is induces a shift of only one bit.
- In Dual-mode, the two most significant bits, `sr_q[7:6]`, are connected to `sd_o[1:0]` and the shift register shifts by two bits with every `shift_en_i` pulse.
- In Quad-mode, the four most significant bits, `sr_q[7:4]`, are connected to `sd_o[3:0]` and the shift register shifts four bits with every pulse.

The connections to the shift register inputs are similar.
Depending on the speed, the `sd_i` inputs are routed to the the 1, 2, or 4 least significant inputs of the shift register.
In full-cycle mode, the shift register LSB's are updated directly from the `sd_i` inputs.
Otherwise the data first passes through an input sampling register, `sd_i_q[3:0]`, which allows the input sampling events to be staggered from the output shift events.

### Bubbles in the Data Pipeline

Temporary delays in the transmission or receipt data are a performance issue.
Stall events, which temporarily halt operation of the SPI_HOST IP, often indicate that software is not keeping up with data in the TX and RX FIFOs.
For this reason the SPI_HOST IP can create interrupts to help monitor the frequency of these stall events, in order to identify correctable performance delays.

There is also the possibility of encountering bubble events, which cause transient stalls in the data pipeline.
Transient stalls only occur for Quad-mode segments, and only when transmitting or receiving words with only one valid byte.

When transmitting at full clock speed, Quad-mode segments need to process one byte every four clock cycles.
If a particular Quad TX segment pulls only one byte from a particular data word (for reasons related either to the segment length or odd data alignment), the `prim_packer_fifo` used in the Byte Select block can generate delays of up to four clocks before releasing the next byte.
This can cause temporary stall conditions either during the Quad segment, or--if there is another TX segment immediately following--just before the following segment.

Similar delays exist when receiving Quad-mode data, which are similarly worst when packing words with just one byte (i.e., when receiving segments of length 4n+1).
The RX pipeline is however much more robust to such delays, thanks to buffering in the shift register outputs.
There is some sensitivity to *repeated* 4 clock delays, but it takes at least six of them to cause a temporary stall.
So transient RX stalls only occur when receiving more than six consecutive one-byte segments.
As this is an unlikely use case, transient stalls are considered an unlikely occurrence in the RX path.

Dual- and Standard-mode segments can tolerate byte-to-byte delays of 7 or 15 clocks, so there are no known mechanism for transient stalls at these speeds.

Please refer to the [the Appendix]({{< relref "#analysis-of-transient-datapath-stalls" >}}) for a detailed analysis of transient stall events.

## SPI_HOST Finite State Machine (FSM)

The SPI_HOST FSM is responsible for parsing the input command segments and configuration settings, which it uses to control the timing of the `sck` and `csb` signals.
It also controls the timing of shift register operations, coordinating I/O on the `sd` bus with the other SPI signals.

This section describes the SPI_HOST FSM and its control of the `sck` and `csb` lines as well as its interactions with the Shift Register and the Command FIFO.

### Clock Divider

The SPI_HOST FSM is driven by the rising edge of the input clock, however the FSM state registers are not *enabled* during every cycle.
There is an internal clock counter `clk_cntr_q` which repeatedly counts down from {{< regref "CONFIGOPTS.CLKDIV" >}} to 0, and the FSM is only enabled when `clk_cntr_q == 0`.

The exception is when the FSM is one of the two possible Idle states (`Idle` or `IdleCSBActive`), in which case `clk_cntr_q` is constantly held at zero, making it possible to immediately transition out of the idle state as soon as a new command appears.
Once the FSM transitions out of the idle state, `clk_cntr_q` resets to {{< regref "CONFIGOPTS.CLKDIV" >}}, and FSM transitions are only enabled at the divided clock rate.

As shown in the waveform below, this has the effect of limiting the FSM transitions to only occur at discrete *timeslices* of duration:

$$T_\textrm{timeslice} = \frac{T_{\textrm{clk},\textrm{clk}}}{\texttt{clkdiv}+1}.$$

```wavejson
{signal: [
  {name: 'clk',        wave: 'p......................'},
  {name: 'clkdiv',     wave: '2......................', data: "3"},
  {name: 'clk_cntr_q', wave: '222222222222......22222', data: "3 2 1 0 3 2 1 0 3 2 1 0 3 2 1 0 3"},
  {name: 'FSM state',  wave: '2...2.......2.....2...2', data: "WaitTrail WaitIdle Idle WaitLead Hi"              },
  {name: 'fsm_en',     wave: '0..10......1......0..10'              },
  {name: 'Timeslice Boundary', wave: "1...H...H...H.....H...H"}
],
  edge: ["A<->B min. 9 cycles", "C<->D min. 4 cycles"],
 head: {text: "Use of FSM Enable Pulses to Realize Multi-Clock Timeslices", tock: 1},
 foot: { text: "The fsm_en signal is always high in idle states, to allow exit transitions at any time"}
}
```

#### Other Internal Counters

In addition to the FSM state register, the SPI_HOST FSM block also has a number of internal registers to track the progress of a given command segment.

- `wait_cntr_q`: This counter is used the hold the FSM in a particular state for several timeslices, in order to implement the `CSNIDLE`, `CSNLEAD` or `CSNTRAIL` delays required for a particular device.

- `byte_cntr_q`, `bit_cntr_q`: These counters respectively track the number of bytes left to transmit for the current segment and the number of bits left to transmit in the current byte.

- Finally, there are registers corresponding to each configuration field (`csid_q`, `cpol_q`, `cpha_`, `csnidle_q`, `csnlead_q`, `csntrail_q`, and `full_cyc_q`) and to each command segment field (`csaat`, `cmd_rd_en`, `cmd_wr_en`, and `cmd_speed`).
This registers are sampled whenever a new command comes in, allowing the command inputs to change.

### Basic Operation

The state machine itself is easiest understood by first considering a simple case, with CSAAT set to zero.
For this initial discussion it is assumed that every command consists of one single segment.
Multi-segment commands are considered in the following sections.
In this case the state machine can be simplified to the following figure.

![](./spi_host_fsm_simplified.svg)

The operation of the state machine is the same regardless of the polarity (CPOL) or phase (CPHA) of the current command.
Commands with `CPOL==0` or `CPOL==1` are processed nearly identically, since the only difference is in the polarity of the `sck` output.
The state machine drives an internal `sck` clock signal, which is low except when the FSM is in the `InternalClockHigh` state.
If `CPOL==0` this clock is registered as is to the external `sck` ports.
If `CPOL==1` the internal clock is *inverted* before the final `sck` output register.

In the following description of the individual states, it is assumed that `CPOL==0`.
To understand the IP's behavior for transactions with `CPOL==1`, simply invert the value of `sck`.

1. Idle state: In this initial reset state, The `sck` signal is low, and all `csb` lines are high (i.e., inactive).
An input command is registered whenever `command_valid_i` and `command_ready_o` are both high (i.e., when the signal `new_command = command_valid_i & command_ready_o` is high), in which case the state machine transitions to the `WaitLead` state.

2. WaitLead state: In this state, `sck` remains low, and the `csb` line corresponding to `csid` is asserted-low.
The purpose of this state is to hold `sck` low for at least `csnlead` + 1 timeslices, before the first rising edge of `sck`.
For his reason, the FSM uses the `wait_cntr` to track the number of timeslices spent in this state, and only exits when `wait_cntr` counts down to zero, at which point the FSM transitions to the `InternalClkHigh` state.

3. InternalClkHigh state: Entering this state drives `sck` high.
This state repeats many times per segment, and usually transitions to the `InternalClkLow` state.
The FSM transitions to the `WaitTrail` state only when the entire segment has been transmitted/received (as indicated by the signals last_bit and last_byte).
This state machine usually only lasts stays in this state for one timeslice, except when the FSM is disabled or stalled.

4. InternalClkLow state: This state serves to drive `sck` low between visits to the `InternalClkHigh` state.
This state always returns back to the `InternalClkHigh` state in the next timeslice.

5. WaitTrail state: Similar to the WaitLead, this state serves to control the timing of the `csb` line.
The FSM uses the `wait_cntr` register to ensure that it remains in this state for `csntrail+1` timeslices, during which time the active `csb` is still held low.
The `wait_cntr` register resets to {{< regref "CONFIGOPTS.CSNTRAIL" >}} upon entering this state, and is decremented once per timeslice.
This state transitions to `WaitIdle` when `wait_cntr` is zero.

6. WaitIdle state: In this timing control state, the FSM uses the `wait_cntr` register to ensure that all `csb` lines are held high for at least `csnidle+1` timeslices.
The `wait_cntr` register resets to {{< regref "CONFIGOPTS.CSNIDLE" >}} upon entering this state, and is decremented once per timeslice.
This state transitions to `Idle` when `wait_cntr` reaches zero.

```wavejson
{signal: [
  {name: 'clk', wave: 'p...............'},
  {name: 'rst_n', wave: '01..............'},
  {name: 'state', wave: 'x22.42424242.2.2', data: ['Idle', 'WaitLead', 'IntClkHigh', 'IntClkLow', 'IntClkHigh', 'IntClkLow', 'IntClkHigh', 'IntClkLow','IntClkHigh', 'WaitTrail', 'WaitIdle', 'Idle']},
  {name: 'csb (active device)', wave: 'x10..........1..'},
  {name: 'csb (all others)', wave: '1...............'},
  {name: 'sck', wave: '0...10101010....'}
],
 config: {hscale: 2}
}
```

### Milestone Signals, Serial Data Lines & Shift Register Control

The FSM manages I/O on the `sd` bus by controlling the timing of the shift register control signals: `shift_en_o`, `sample_en_o`, `rd_en_o`, `last_read_o`, `wr_en_o`, and `last_write_o`.

The shift register control signals are managed through the use of three intermediate signals:
- `byte_starting`: This signal indicates the start of a new byte on the `sd` bus in the *following* clock cycle.
For Bidirectional or TX segments this signal would indicate that it is time to load a new byte into the shift register.
This signal corresponds to the FSM's `wr_en_o` port, though that output is suppressed during RX or dummy segments.
- `byte_ending`: This signal indicates the end of the current `sd` byte in the *current* clock cycle (i.e., the next clock cycle either marks the beginning new byte or the end of the current segment).
As illustrated in the following waveform, the `byte_starting` and `byte_ending` signals are often asserted at the same time, though there is an extra `byte_starting` pulse at the beginning of each command and an extra `byte_ending` pulse at the end.
For RX and bidirectional command segments, a `byte_ending` pulse generates a `rd_en_o` pulse to the shift register, which transfers the 8-bit contents of the shift register into the RX FIFO via the Byte Merge block.
- `bit_shifting`: This signal drives the `shift_en_o` control line to the shift register, ejecting the most-significant bits, and updating the `sd` outputs.

These *milestone signals* mark the progress of each command segment.

The coordination of the milestone signals and the shift register controls are shown in the following waveform.
Since the milestone signal pulses coincide with *entering* particular FSM states, they are derived from the state register *inputs* (i.e., `state_d`), as opposed to the state register outputs (`state_q`).

```wavejson
{signal: [
  {name: 'clk', wave: 'p........................'},
  {name: 'rst_n', wave: '01.......................'},
  {name: 'state_q',
   wave: 'x2.2.42424242424242424242', data: "Idle WL Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo",
   node: '...W..V.............U'},
  {name: 'csb', wave: 'x1.0.....................'},
  {name: 'sck', wave: '0....10101010101010101010'},
  {name: 'state_d',
   wave: 'x22.42424242424242424242', data: "Idle WL Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo",
   node: '..Z..Y.............X'},
  {name: 'byte_starting / wr_en_o',
   wave: 'x010...............10....',
   node: '..A................E'},
  {name: 'byte_ending / rd_en_o',
   wave: 'x0.................10....',
   node: '...................F'},
  {name: 'bit_shifting / shift_en_o',
   wave: 'x0...10101010101010..1010',
   node: '.....C'},
  {name: 'sample_en_o',
   wave: 'x0.10.1010101010101010101',
   node: '...B..D'},
  {name: 'sample_event',
   wave: '1...H..H.H.H.H.H.H.H.H.H.'},
  {name:'sd_o',
   wave:'x..2..2.2.2.2.2.2.2.2.2.2',
   node:'',
   data: "A[7] A[6] A[5] A[4] A[3] A[2] A[1] A[0] B[7] B[6]"},
  {name: 'bit_cntr_q', wave: 'x2.2..2.2.2.2.2.2.2.2.2.2', data: "0 7 6 5 4 3 2 1 0 7 6 5"},
  {name: 'byte_cntr_q', wave: 'x2.2................2....', data: "0 N N-1"},

],
edge: ['A-~>B', 'C-~>D', 'Z-~>A', 'Y-~>C', 'X-~>E', 'X-~>F', 'Z-~>W', 'Y-~>V', 'X-~>U'],
config: {hscale: 1},
head: {text: "Timing Relationship between FSM states, Milestone Signals, and Shift Register controls (with CPHA=0)"},
foot: {text: "Key: WL=\"WaitLead\", Hi=\"InternalClkHigh\", Lo=\"InternalClkLow\" "}
}
```

When working from a CPHA=0 configuration, the milestone signals are directly controlled by transitions in the FSM state register, as described in the following table.

<table>
<thead><tr>
<th>Milestone Signal</th><th>FSM Triggers</th>
</tr></thead>
<tbody>
<tr><td rowspan=2><tt>byte_starting</tt></td><td>Entering <tt>WaitLead</tt></td></tr>
<tr><td>Entering <tt>InternalClkLow</tt> and <tt>bit_cntr == 0 </tt> </td></tr>
<tr><td><tt>bit_shifting</tt></td><td>Entering <tt>InternalClkLow</tt> and <tt>bit_cntr != 0</tt></td></tr>
<tr><td><tt>byte_ending</tt></td><td>Exiting <tt>InternalClkHigh</tt> and <tt>bit_cntr == 0</tt></td></tr>
</tbody>
</table>

When working from a CPHA=1 configuration, the milestone signals exploit the fact that there is usually a unique correspondence between `csb`/`sck` events and FSM transitions.
There are some exceptions to this pattern since, as discussed below, CSAAT- and multi-csb-support requires the creation of multiple flavors of idle states.
However, there are no milestone signal pulses in any of the transitions between these various idle states.
Thus in CPHA=1 mode, the milestone signals are delayed by one-state transition.
For example, in a CPHA=0 configuration the first data burst should be transmitted as the `csb` line is asserted low, that is, when the FSM enters the WaitLead state.
Thus a `byte_starting` pulse is generated at this transition.
On the other hand, in CPHA=1 configuration the first data burst should be transmitted after the first edge of `sck`, which happens on the next state transition as illustrated in the following waveform.

That said, there are two copies of each milestone signal:
- the original FSM-driven copy, for use when operating with CPHA=0, and
- a delayed copy, for use in CPHA=1 operation.

```wavejson
{signal: [
  {name: 'clk', wave: 'p......................'},
  {name: 'rst_n', wave: '01.....................'},
  {name: 'state_q',
   wave: 'x2.2.4242424242424242.2', data: "Idle WL Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi WT WI",
   node: '...W..V.....U..........'},
  {name: 'state_d',
   wave: 'x22.4242424242424242.2', data: "Idle WL Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi Lo Hi WT WI",
   node: '..Z..Y.....X..........'},
  {name: 'byte_starting_cpha0',
   wave: 'x010.......10..........',
   node: '..A........C...........'},
  {name: 'byte_starting_cpha1',
   wave: 'x0..10......10.........',
   node: '....B.......D..........'},
  {name: 'byte_ending_cpha0',
   wave: 'x0.........10......10..',
   node: '...........E...........'},
  {name: 'byte_ending_cpha1',
   wave: 'x0..........10......10.',
   node: '............F..........'},
  {name: 'bit_shifting_cpha0',
   wave: 'x0...101010..101010....',
   node: '.....G...I...K.........'},
  {name: 'bit_shifting_cpha1',
   wave: 'x0....101010..101010...',
   node: '......H...J...L'},
  {name: 'csb', wave: 'x1.0..................1'},
  {name: 'sck', wave: '0....1010101010101010..'},
  ["CPHA=0",
   {name: 'byte_starting',
    wave: 'x010.......10..........'},
   {name: 'bit_shifting',
    wave: 'x0...101010..101010....'},
  {name: 'bit_cntr_q', wave: 'x2.2..2.2.2.2.2.2.2....',
   data: "0 6 4 2 0 6 4 2 0"},
  {name: 'byte_cntr_q', wave: 'x2.2........2..........',
   data: "0 1 0"},
  {name:'sd_o',
   wave:'x0.2..2.2.2.2.2.2.2...0',
   node:'',
   data: "A[7:6] A[6:5] A[4:3] A[1:0] B[7:6] B[6:5] B[4:3] B[1:0]"}
  ],
  ["CPHA=1",
   {name: 'byte_starting',
    wave: 'x0..10......10.........'},
   {name: 'bit_shifting',
    wave: 'x0....101010..101010...'},
   {name: 'byte_ending',
    wave: 'x0..........10......10.'},
  {name: 'bit_cntr_q', wave: 'x2...2.2.2.2.2.2.2.2...',
   data: "0 6 4 2 0 6 4 2 0"},
  {name: 'byte_cntr_q',
   wave: 'x2.2.........2.........',
   data: "0 1 0"},
  {name:'sd_o',
   wave:'x0...2.2.2.2.2.2.2.2..0',
   node:'',
   data: "A[7:6] A[6:5] A[4:3] A[1:0] B[7:6] B[6:5] B[4:3] B[1:0]"}
  ],
],
edge: ['Z-~>A','Y-~>G', 'X-~>C', 'X-~>E','A->B', 'C->D', 'E->F', 'G->H', 'I->J', 'K->L', 'Z->W', 'Y->V', 'X->U'],
config: {hscale: 1},
head: {text: "Comparison of Milestone Signals in CPHA=0 vs. CPHA=1 configuration (for a dual speed segment)"},
foot: {text: "Key: WL=\"WaitLead\", Hi=\"InternalClkHigh\", Lo=\"InternalClkLow\", WT=\"WaitTrail\""}
}
```

### Milestone Signals and Control of the the Bit and Byte Counters

The previous waveform also highlights the relationship between the milestone signals and the bit and byte counters.
At the beginning of each byte `bit_cntr_q` is reset to a speed-specific value, to trigger the correct number of shift operations required for each byte:
- 7 for Standard-mode
- 6 for Dual-mode
- 4 for Quad-mode

The reset of the `bit_cntr_q` counter is triggered by the `byte_starting` register.
Meanwhile the `bit_shifting` signal triggers a decrement of the bit-shifting register.
The size of the decrement also depends on the speed of the current segment:
- 1 for Standard-mode
- 2 for Dual-mode
- 4 for Quad-mode

The `byte_cntr_q` register is updated from the {{< regref "COMMAND.LEN" >}} register value, at the beginning of each segment, and decremented after each `byte_ending` pulse until the counter reaches zero.

This relationship between the milestone signals and the bit and byte counters is also illustrated in the previous waveform.

### Implementation of Configuration Change Delays

As described in the [Theory of Operation]({{< relref "#idle-time-delays-when-changing-configurations" >}}), changes in configuration only occur when the SPI_HOST is idle.
The configuration change must be preceded by enough idle time to satisfy the previous configuration, and followed by enough idle time to satisfy the new configuration.

In order to support these idle time requirements, the SPI_HOST FSM has two idle waiting states.
- The `WaitIdle` state manages the idle time requirements of the *preceding* command segment, and usually transitions to the `Idle` state afterwards.
- From the `Idle` state the FSM monitors for changes in configuration, and transitions to the `ConfigSwitch` state if any changes are detected in the next incoming command segment.
This state introduces delays long enough the satisfy the idle time requirements of *following* command segment.
From the `ConfigSwitch` state, the state machine directly enters the `WaitLead` state to start the next command segment.

A complete state diagram, including the `ConfigSwitch` state, is shown in the following section.

The following waveform illustrates how a change in a single {{< regref "CONFIGOPTS" >}}, here {{< regref "CONFIGOPTS.CPOL" >}}, triggers an entry into the `ConfigSwitch` Idle state, and how the new configuration is applied at the transition from `WaitIdle` to `ConfigSwitch` thereby ensuring ample idle time both before and after the configuration update.

```wavejson
{signal: [
  {name: 'clk',                       wave: 'p.................'},
  {name: 'command_i.csid',            wave: '2.................', data: ["0"]},
  {name: 'command_i.configopts.cpol', wave: '1........x........'},
  {name: 'cpol_q',                    wave: '0........1........'},
  {name: 'switch_required',           wave: '1........x........'},
  {name: 'command_valid_i',           wave: '1........0........'},
  {name: 'command_ready_i',           wave: '0.......10........'},
  {name: 'FSM state',                 wave: '2222..2..2..2..222', data: ["Hi", "Lo", "Hi", "WaitTrail", "WaitIdle", "ConfigSwitch", "WaitLead", "Hi", "Lo", "Hi"]},
  {name: 'csb[0]',                    wave: '0.....1.....0.....'},
  {name: 'sck',                       wave: '1010.....1.....010'},
  {name: 'configuration update event', wave: '1........H........'}
],
  edge: ["A<->B min. 9 cycles", "C<->D min. 4 cycles"],
  head: {text: "Extension of CSB Idle Pulse Due to CPOL Configuration Switch", tock: 1},
  foot: { text: "(Note: Due to the presence of a valid command, the FSM transitions directly from WaitIdle to ConfigSwitch)"}
}
```

### CSAAT Support

In addition to omitting the `ConfigSwitch` state, the simplified state machine illustrated above does not take into account commands with multiple segments, where the CSAAT bit is enabled for all but the last segment.

When the CSAAT bit in enabled there is no idle period between the current segment and the next, nor are the two adjoining segments separated by a Trail or Lead period.
Usually the end of each segment is detected in the `InternalClkHigh` state, at which point, if CSAAT is disabled, the FSM transitions to the `WaitTrail` state to close out the transaction.
However, if CSAAT is enabled the `WaitTrail` state is skipped, and the next state depends on whether there is another command segment available for processing (i.e., both `command_ready_o` and `command_valid_i` are both asserted).

In order to support seamless, back-to-back segments the `ConfigSwitch` state can be skipped if a new segment is already available when the previous ends, in which case the FSM transitions directly to the `InternalClkLow` at the end of the previous segment.

If there is no segment available yet, the FSM must pause and idly wait for the next command in the special `IdleCSBActive` state.
This state serves a similar purpose to the `Idle` state since in this state the IP is doing nothing but waiting for new commands.
It is different from the `Idle` state though in that during this state the active `csb` is held low.
When a command segment is received in the `IdleCSBActive` state, it transitions immediately to the `InternalClkLow` state to generate the next `sck` pulse and process the next segment.

```wavejson
{signal: [
  {name: 'clk', wave: 'p...........'},
  {name: 'command_ready_o', wave: '0.1....0....'},
  {name: 'command_valid_i', wave: '0.....10....'},
  {name: 'new_command',     wave: '0.....10....'},
  {name: 'state',           wave: '2222...22222', data: ["Hi", "Lo", "Hi", "IdleCSBActive", "Lo", "Hi", "Lo", "Hi", "Lo"]},
  {name: 'sck (CPOL=0)',    wave: '1010....1010'},
  {name: 'sd (CPHA=0)',     wave: '35.....3.4.5'}
 ],
  edge: ["A<->B min. 9 cycles", "C<->D min. 4 cycles"],
  head: {text: "Idling While CS Active", tock: 1}
}
```

The following figure shows the complete state transition diagram of for the SPI_HOST FSM.

![](./spi_host_fsm_complete.svg)

### Skipped idle states

The `Idle` and `IdleCSBActive` states are unique from the others in that:
1. In order to respond to an incoming command the FSM can exit these idle states at any time, regardless of the current timeslice definition.
(In fact, since different commands may use different values for the `CLKDIV` configuration parameter, the concept of a timeslice is poorly defined when idle).
2. These idle states may be *bypassed* in order to support more efficient transitions from one command segment to the next.
If an incoming command is detected as the FSM is about to enter an idle state, that idle state is skipped, and the FSM immediately transitions to the next logical state, based on the contents of the new incoming command.

These bypassable states, which are highlighted in the previous diagram, represent a number of possible transitions from one *pre-idle* state to a following *post-idle* state.
For clarity such transitions are left implicit in the diagram above.
However they could also be explicitly added to the state diagram.
For example, the implicit transitions around the `Idle` are shown in the following figure.

![](./spi_host_bypassable_state.svg)

### Stall

Whenever the shift register needs to transfer data in (or out) of the RX (TX) FIFOs, but they are full (or empty), the FSM immediately stalls to wait for new data.

During this stall period none of the FSM internal registers are updated.
Normal operation proceeds only when the stall condition has been resolved or the SPI_HOST has been reset.

In the SPI_HOST FSM this is realized by disabling all flop updates whenever a stall is detected.

Furthermore, all control signals out of the FSM are suppressed during a stall condition.

From an implementation standpoint, the presence of a stall condition has two effects on the SPI_HOST FSM:
1. No flops or registers may be updated during a stall condition.
Thus the FSM may not progress while stalled.

2. All handshaking or control signals to other blocks must be suppressed during a stall condition, placing backpressure on the rest the blocks within the IP to also stop operations until the stall is resolved.
