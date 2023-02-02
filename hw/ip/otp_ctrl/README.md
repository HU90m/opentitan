# OTP Controller Technical Specification

This document specifies the functionality of the one time programmable (OTP) memory controller.
The OTP controller is a module that is a peripheral on the chip interconnect bus, and thus follows the [Comportability Specification]({{< relref "doc/rm/comportability_specification" >}}).

The OTP is a module that provides a device with one-time-programming functionality.
The result of this programming is non-volatile, and unlike flash, cannot be reversed.
The OTP functionality is constructed through an open-source OTP controller and a proprietary OTP IP.

The OTP controller provides:
- An open-source abstraction interface that software can use to interact with a proprietary OTP block underneath.
- An open-source abstraction interface that hardware components (for example [life cycle controller]({{< relref "hw/ip/lc_ctrl/doc" >}}) and [key manager]({{< relref "hw/ip/keymgr/doc" >}})) can use to interact with a proprietary OTP block underneath.
- High level logical security protection, such as integrity checks and scrambling of sensitive content.
- Software isolation for when OTP contents are readable and programmable.

The proprietary OTP IP provides:
- Reliable, non-volatile storage.
- Technology-specific redundancy or error correction mechanisms.
- Physical defensive features such as SCA and FI resistance.
- Visual and electrical probing resistance.

Together, the OTP controller and IP provide secure one-time-programming functionality that is used throughout the life cycle (LC) of a device.

## Features

- Multiple logical partitions of the underlying OTP IP
  - Each partition is lockable and integrity checked
  - Integrity digests are stored alongside each logical bank
- Periodic / persistent checks of OTP values
  - Periodic checks of shadowed content vs digests
  - Periodic checks of OTP stored content and shadowed content
  - Persistent checks for immediate errors
- Separate life cycle partition and interface to life cycle controller
  - Supports life cycle functions, but cannot be integrity locked
- Lightweight scrambling of secret OTP partition using a global netlist constant
- Lightweight ephemeral key derivation function for RAM scrambling mechanisms
- Lightweight key derivation function for FLASH scrambling mechanism

## OTP Controller Overview

The functionality of OTP is split into an open-source and a closed-source part, with a clearly defined boundary in between, as illustrated in the simplified high-level block diagram below.

![OTP Controller Overview](doc/otp_ctrl_overview.svg)

It is the task of the open-source controller to provide a common, non-technology specific interface to OTP users with a common register interface and a clearly defined I/O interface to hardware.
The open-source controller implements logical isolation and partitioning of OTP storage that enables users to separate different functions of the OTP into "partitions" with different properties.
Finally, the open-source controller provides a high level of security for specific partitions by provisioning integrity digests for each partition, and scrambling of partitions where required.

The proprietary IP on the other hand translates a common access interface to the technology-specific OTP interface, both for functional and debug accesses (for example register accesses to the macro-internal control structure).

This split implies that every proprietary OTP IP must implement a translation layer from a standardized OpenTitan interface to the module underneath.
It also implies that no matter how the OTP storage or word size may change underneath, the open-source controller must present a consistent and coherent software and hardware interface.
This standardized interface is defined further below, and the wrapper leverages the same [technology primitive mechanism]({{< relref "hw/ip/prim/doc" >}}) that is employed in other parts of OpenTitan in order to wrap and abstract technology-specific macros (such as memories and clocking cells) that are potentially closed-source.

In order to enable simulation and FPGA emulation of the OTP controller even without access to the proprietary OTP IP, a generalized and synthesizable model of the OTP IP is provided in the form of a [generic technology primitive](https://github.com/lowRISC/opentitan/blob/master/hw/ip/prim_generic/rtl/prim_generic_otp.sv).

## OTP IP Assumptions

It is assumed the OTP IP employed in production has reasonable physical defense characteristics.
Specifically which defensive features will likely be use case dependent, but at a minimum they should have the properties below.
Note some properties are worded with "SHALL" and others with "SHOULD".
"SHALL" refers to features that must be present, while "SHOULD" refers to features that are ideal, but optional.

- The contents shall not be observable via optical microscopy (for example anti-fuse technology).
- The IP lifetime shall not be limited by the amount of read cycles performed.
- If the IP contains field programmability (internal charge pumps and LDOs), there shall be mechanisms in place to selectively disable this function based on device context.
- If the IP contains redundant columns, rows, pages or banks for yield improvement, it shall provide a mechanism to lock down arbitrary manipulation of page / bank swapping during run-time.
- The IP shall be clear on what bits must be manipulated by the user, what bits are automatically manipulated by hardware (for example ECC or redundancy) and what areas the user can influence.
- The IP shall be compatible, through the use of a proprietary wrapper or shim, with an open-source friendly IO interface.
- The IP should functionally support the programming of already programmed bits without information leakage.
- The IP should offer SCA resistance:
  - For example, the content may be stored differentially.
  - For example, the sensing exhibits similar power signatures no matter if the stored bit is 0 or 1.
- The IP interface shall be memory-like if beyond a certain size.
- When a particular location is read, a fixed width output is returned; similar when a particular location is programmed, a fixed width input is supplied.
- The IP does not output all stored bits in parallel.
- The contents should be electrically hidden. For example, it should be difficult for an attacker to energize the fuse array and observe how the charge leaks.
- The IP should route critical nets at lower metal levels to avoid probing.
- The IP should contain native detectors for fault injection attacks.
- The IP should contain mechanisms to guard against interrupted programming - either through malicious intent or unexpected power loss and glitched address lines.
- The IP should contain mechanisms for error corrections (single bit errors).
  - For example ECC or redundant bits voting / or-ing.
  - As error correction mechanisms are technology dependent, that information should not be exposed to the open-source controller, instead the controller should simply receive information on whether a read / program was successful.
- The IP should have self-test functionality to assess the health of the storage and analog structures.
- The IP may contain native PUF-like functionality.
