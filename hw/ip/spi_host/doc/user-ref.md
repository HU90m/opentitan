# User Reference

The operation of the SPI_HOST IP proceeds in seven general steps.

To initialize the IP:
1. Program the {{< regref "CONFIGOPTS" >}} multi-register with the appropriate timing and polarity settings for each `csb` line.
2. Set the desired interrupt parameters
3. Enable the IP

Then for each command:

4. Load the data to be transmitted into the FIFO using the {{< regref "TXDATA" >}} memory window.
5. Specify the target device by programming the {{< regref "CSID" >}}
6. Specify the structure of the command by writing each segment into the {{< regref "COMMAND" >}} register
   - For multi-segment transactions, be sure to assert {{< regref "COMMAND.CSAAT" >}} for all but the last command segment
7. For transactions which expect to receive a reply, the data can then be read back from the {{< regref "RXDATA" >}} window.

These latter four steps are then repeated for each command.
Each step is covered in detail in the following sections.

For concreteness, this Programmer's Guide uses examples from one of our primary target devices, the [W25Q01JV flash from Winbond](https://www.winbond.com/resource-files/W25Q01JV%20SPI%20RevB%2011132019.pdf).
The SPI_HOST IP is however suitable for interacting with any number of SPI devices, and the same mode of operation can be used for any SPI device.

## Initializing the IP

### Per-target Configuration

The {{< regref "CONFIGOPTS" >}} multi-register must be programmed to reflect the requirements of the attached target devices.
As such these registers can be programmed once at initialization, or whenever a new device is connected (e.g., via changes in the external pin connections, or changes in the pinmux configuration).
The proper settings for the {{< regref "CONFIGOPTS" >}} fields (e.g., CPOL and CPHA, clock divider, ratios, and other timing or sampling requirements) will all depend on the specific device attached as well as the board level delays.

### Interrupt configuration

The next step is to configuration the interrupts for the SPI_HOST.
This should also be done at initialization using the following register fields:
- The {{< regref "ERROR_ENABLE" >}} register should be configured to indicate what types of error conditions (if any) should be ignored to not trigger an interrupt.
At reset, these fields are all set indicating that all error classes trigger an interrupt.

- For interrupt driven I/O the {{< regref "EVENT_ENABLE" >}} register must be configured to select the desired event interrupts to signal the desired conditions (e.g. "FIFO empty", "FIFO at the watermark level", or "ready for next command segment").
By default, this register is all zeros, meaning all event interrupts are disabled, and thus all transactions must be managed by polling the status register.
   - When using the FIFO watermarks to send interrupts, the watermark levels must be set via the {{< regref "CONTROL.RX_WATERMARK" >}} and {{< regref "CONTROL.TX_WATERMARK" >}} fields.

- The event and error interrupts must finally be enabled using the {{< regref "INTR_ENABLE" >}} register.

### Enabling the SPI_HOST

The IP must be enabled before sending the first command by asserting the {{< regref "CONTROL.SPIEN" >}} bit.

## Issuing Transactions

As mentioned above, each command is typically specified in three phases: loading the TX data, specifying the command segments/format, and reading the RX data.
In principle, the first two steps can be performed in either order.
If the SPI_HOST does not find any data to transmit it will simply stall until data is inserted.
Meanwhile, the RX data is only available after the command format has been specified and processed by the state machine.

For longer transactions, with data larger than the capacity of the FIFOs, the command sequence may become more complex.
For instance, to send 1024 bytes of data in a single transaction, the TX data may need to be loaded several times if using a 256-byte FIFO.
In this instance, the programming sequence will consist of at least four iterations of entering TX data and waiting for the TX FIFO to drain.

### Loading TX data

SPI transactions expect each command to start with some command sequence from the host, and so usually data will be transmitted at least in the first command segment.
The {{< regref "TXDATA" >}} window provides a simple interface to the TX FIFO.
Data can be written to the window using 8-, 16- or 32-bit instructions.

Some attention, however, should be paid to byte-ordering and segmenting conventions.

#### Byte-ordering Conventions

For SPI flash applications, it is generally assumed that most of the *payload* data will be directly copied from embedded SRAM to the flash device.

If this data is to copied to the {{< regref "TXDATA" >}} window using 32-bit instructions, the SPI_HOST should be parameterized such that the `ByteOrder` parameter matches the byte order of the embedded CPU (i.e., for Ibex, `ByteOrder` should be left set to `1` to indicate a Little-Endian CPU).
This will ensure that data is transmitted to the flash (and thus also stored in flash) in address-ascending order.
For example, consider the transfer of four bytes, `D[3:0][7:0]`, to SPI via the {{< regref "TXDATA" >}} window.
- It is assumed for this example that all four bytes are contiguously stored in SRAM at a word-aligned address, with `D[0]` at the lowest byte-address.
- When these bytes are loaded into the Ibex CPU they are arranged as the 32-bit word: `W[31:0] = {D[3][7:0], D[2][7:0], D[1][7:0], D[0][7:0]}`.
- After this word are loaded into the {{< regref "TXDATA" >}} window, the LSB (i.e., `W[7:0] = D[0][7:0]`) is transmitted first, by virtue of the `ByteOrder == 1` configuration.

In this way, configuring `ByteOrder` to match the CPU ensures that data is transmitted in memory-address order.

The value of the `ByteOrder` parameter can be confirmed by firmware by reading the {{< regref "STATUS.BYTEORDER" >}} register field.

Not all data to the SPI device will come from memory however.
In many cases the transaction command codes or headers will be constructed or packed on the fly in CPU registers.
The order these register bytes are transmitted on the bus will depend on the value of the `ByteOrder` parameter, as discussed in the Theory of Operation section, and for multi-bit values, such as flash addresses), some byte-swapping may be required to ensure that data is transmitted in the proper order expected by the target device.

For example, SPI flash devices generally expect flash addresses (or any other multi-byte values) to be transmitted MSB-first.
This is illustrated in the following figure, which depicts a Fast Quad Read I/O command.
Assuming that `ByteOrder` is set to `1` for Little-Endian devices such as Ibex, byte-swapping will be required for these addresses, otherwise the device will receive the addresses LSB first.

{{< wavejson >}}
{ signal: [
  {name:"csb", wave:"10........................."},
  {name:"sck", wave:"lnn........................"},
  {name:"sd[0]", wave:"x1..0101.22222222z.22334455",
   data:["a[23]", "a[19]", "a[15]", "a[11]", "a[7]", "a[3]", "1", "1"]},
  {name:"sd[1]", wave:"xz.......22222222z.22334455",
   data:["a[22]", "a[18]", "a[14]", "a[10]", "a[6]", "a[2]", "1", "1"]},
  {name:"sd[2]", wave:"xz.......22222222zz22334455",
   data:["a[21]", "a[17]", "a[15]", "a[11]", "a[7]", "a[3]", "1", "1"]},
  {name:"sd[3]", wave:"xz.......22222222zz22334455",
   data:["a[20]", "a[16]", "a[12]", "a[8]", "a[4]", "a[0]", "1", "1"]},
  {node: ".A.......B.C.D.E.F.G.H.I.J.K"},
  {node: ".........L.....M...N........O"}
],
  edge: ['A<->B Command 0xEB ("Fast Read Quad I/O")',  'B<->C MSB(addr)', 'D<->E LSB(addr)',
         'G<->H addr[0]', 'H<->I addr[1]', 'I<->J addr[2]', 'J<->K addr[3]',
         'L<->M Address', 'N<->O Data'],

 foot: {text: "Addresses are transmitted MSB first, and data is returned in order of increasing peripheral byte address."}}
{{< /wavejson >}}

Byte ordering on the bus can also be managed by writing {{< regref "TXDATA" >}} as a sequence of discrete bytes using 8-bit transactions, since partially-filled data-words are always sent in the order they are received.

A few examples related to using SPI flash devices on a Little-Endian platform:
- A 4-byte address can be loaded into the TX FIFO as four individual bytes using 8-bit I/O instructions.
- The above read command (with 4-byte address) can be loaded into the FIFO by first loading the command code into {{< regref "TXDATA" >}} as a single byte, and the address can be loaded into {{< regref "TXDATA" >}} using 32-bit instructions, provided the byte order is swapped before loading.
- Flash transactions with 3-byte addressing require some care, as there are no 24-bit I/O instructions, though there are a several options:
    - After the 8-bit command code is sent, the address can either be sent in several I/O operations (e.g., the MSB is sent as an 8-bit command, and the remaining 16-bits can be sent after swapping)
    - If bandwidth efficiency is a priority, the address, `A[23:0]`, and command code, `C[7:0]`, can all be packed together into a single 4-byte quantity `W[31:0] = {A[7:0], A[15:8], A[23:16], C[7:0]}`, which when loaded into {{< regref "TXDATA" >}} will ensure that the command code is sent first, followed by the address in MSB-first order.

#### Segmenting Considerations

Data words are *not* shared across segments.
If at the end of each TX (or bidirectional) segment there is a partially transmitted data word then any unsent bytes will be discarded as the SPI_HOST IP closes the segment.
For the next TX segment, the transmitted data will start with the following *word* from the TX FIFO.

#### Refilling the TX FIFO

For extremely long transactions, the TX FIFO may not have enough capacity to hold all the data being transmitted.
In this case software can either poll the {{< regref "STATUS.TXQD" >}} register to determine the number of elements in the TX FIFO, or enable the SPI_HOST IP to send an interrupt when the FIFO drains to a certain level.
If {{< regref "INTR_ENABLE.spi_event" >}} and {{< regref "EVENT_ENABLE.TXWM" >}} are both asserted, the IP will send an interrupt whenever the number of elements in the TX FIFO falls below {{< regref "CONTROL.TX_WATERMARK" >}}.

### Specifying the Segments

Each write to the {{< regref "COMMAND" >}} register corresponds to a single command segment.
The length, CSAAT flag, direction and speed settings for that segment should all be packed into a single 32-bit register and written simultaneously to {{< regref "COMMAND" >}}.

The {{< regref "COMMAND" >}} should only be written when {{< regref "STATUS.READY" >}} is asserted.

While each command segment is being processed, the SPI_HOST has room to queue up exactly one additional segment descriptor in the Command Clock Domain Crossing.
Once a second command segment descriptor has been submitted, software must wait for the state machine to finish processing the current segment before submitting more.
Software can poll the {{< regref "STATUS.READY" >}} field to determine when it is safe to insert another segment descriptor.
Otherwise the {{< regref "EVENT_ENABLE.IDLE" >}} bit can be enabled (along with {{< regref "INTR_ENABLE.spi_event" >}}) to trigger an event interrupt whenever {{< regref "STATUS.READY" >}} is asserted.

### Reading Back the Device Response

Once an RX segment descriptor has been submitted to the SPI_HOST, the received data will be available in the RX FIFO after the first word has been received.

The number of words in the FIFO can be polled by reading the {{< regref "STATUS.RXQD" >}} field.
The SPI_HOST IP can also configured to generate watermark event interrupts whenever the number of words received reaches (or exceeds) {{< regref "CONTROL.RX_WATERMARK" >}}.
To enable interrupts when ever the RX FIFO reaches the watermark, assert {{< regref "EVENT_ENABLE.RXWM" >}} along with {{< regref "INTR_ENABLE.spi_event" >}}.

## Exception Handling

The SPI_HOST will assert one of the {{< regref "ERROR_STATUS" >}} bits in the event of a firmware programming error, and will become unresponsive until firmware acknowledges the error by clearing the corresponding error bit.

The SPI_HOST interrupt handler should clear any bits in {{< regref "ERROR_STATUS" >}} bit before clearing {{< regref "INTR_STATE.error" >}}.

In addition to clearing the {{< regref "ERROR_STATUS" >}} register, firmware can also trigger a complete software reset via the {{< regref "CONTROL.SW_RST" >}} bit, as described in the next section.

Other system-level errors may arise due to improper programming of the target device (e.g., due to violations in the device programming model, or improper configuration of the SPI_HOST timing registers).
Given that the SPI protocol provides no mechanism for the target device to stall the bus, the SPI_HOST will continue to function even if the remote device becomes unresponsive.
In case of an unresponsive device, the RX FIFO will still accumulate data from the bus during RX segments, though the data values will be undefined.

## Software Reset Procedure

In the event of an error the SPI_HOST IP can be reset under software control using the following procedure:

1. Set {{< regref "CONTROL.SW_RST" >}}.
2. Poll IP status registers for confirmation of successful state machine reset:
   - Wait for {{< regref "STATUS.ACTIVE" >}} to clear.
   - Wait for both FIFOs to completely drain by polling {{< regref "STATUS.TXQD" >}} and {{< regref "STATUS.RXQD" >}} until they reach zero.
3. Clear {{ < regref "CONTROL.SW_RST" >}}.

## Device Interface Functions (DIFs)

{{< dif_listing "sw/device/lib/dif/dif_spi_host.h" >}}

## Register Table

{{< incGenFromIpDesc "../data/spi_host.hjson" "registers" >}}
