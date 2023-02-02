# Bus Specification

This document specifies the bus functionality within a Comportable top level
system. This includes the bus protocol and all hardware IP that supports
creating the network on chip within that framework.

## Features

- Support for multiple bus hosts and bus devices<sup>1</sup>
- Support for multiple clock domains
- Support for multiple outstanding requests
- Extendability for 32b or 64b data widths
- Extendability for flexible address widths
- Extendability for security features
- Low pin-count / area overhead
- Support for transaction sizes up to bus width (byte, 2B, 4B); no
  support for bursts
- Suite of bus primitives to aid in fast fabric generation

<sup>1</sup>lowRISC is avoiding the fraught terms master/slave and defaulting
to host/device where applicable.

## Description

For chip-level interconnect, Comportable devices will be using
[TileLink](https://static.dev.sifive.com/docs/tilelink/tilelink-spec-1.7-draft.pdf)
as its bus fabric protocol. For the purposes of our performance
requirement needs, the Uncached Lightweight (TL-UL) variant will
suffice. There is one minor modification to add the user extensions. This
is highlighted below, but otherwise all functionality follows the official
specification. The main signal names are kept the same as TL-UL and the
user extension signal groups follow the same timing and naming conventions
used in the TL-UL specification. Existing TL-UL IP blocks may be used
directly in devices that do not need the additional sideband signals,
or can be straightforwardly adapted to use the added features.

TL-UL is a lightweight bus that combines the point-to-point
split-transaction features of the powerful TileLink (or AMBA AXI)
5-channel bus without the high pin-count overhead. It is intended to be
about on par of pincount with APB but with the transaction performance of
AXI-4, modulo the following assumptions.

- Only one request (read or write) per cycle
- Only one response (read or write) per cycle
- No burst transactions

Bus primitives are provided in the lowRISC IP library. These are
described later in this document. These primitives can be combined to form
a flexible crossbar of any M hosts to any N devices. As of this writing,
these crossbars are generated programmatically through usage of configuration files.
See the [tlgen reference manual]({{< relref "doc/rm/crossbar_tool" >}}) for more details.

## Compatibility

With the exception of the user extensions, the bus is
compliant with TileLink-UL. The bus primitives, hosts and peripherals
developed using the extended specification can be used with
blocks using the base specification. As a receiver baseline blocks
ignore the user signals and as a
source will generate a project-specific default value. Alternatively,
the blocks can be easily modified to make use of the user extensions.
