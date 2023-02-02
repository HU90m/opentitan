# Hardware

OpenTitan is the first of its kind in making a production-ready secure open-source root-of-trust SoC (System on Chip).
This book is a reference for hardware engineers working on the OpenTitan project.
The book has four main chapters:
* [Top Earlgrey](./top_earlgrey/index.html)
* [Cores](./doc/cores.md)
* [Hardware IP Blocks](./ip/index.html)
* [Common SystemVerilog and UVM Components](./dv/sv/index.html)

The Earlgrey top embodies an instantiation of OpenTitan in a discrete chip.
The cores are hardware IP blocks that can run programs and are therefore different from the other hardware IP blocks in OpenTitan.

Each hardware IP block has its own overview, theory of operations and user reference.
The overview gives you a quick summary of what the block does, while the theory of operations goes into more details of how the internal hardware works.
If you only want to know how to use a block, please have a look at the user guide.

For hardware, testing and utilities that are used in multiple blocks, please have a look at the common components chapter.

## Overview of page

We start off by providing links to the [results of various tool-flows](#results-of-toolflows) run on all of our [Comportable]({{< relref "doc/rm/comportability_specification" >}}) IPs.
This includes DV simulations, FPV and lint, all of which are run with the `dvsim` tool which serves as the common frontend.

The [Comportable IPs](#comportable-ips) following it provides links to their design specifications and DV documents, and tracks their current stage of development.
See the [Hardware Development Stages]({{< relref "/doc/project/development_stages.md" >}}) for description of the hardware stages and how they are determined.

Next, we focus on all available [processor cores](#processor-cores) and provide links to their design specifications, DV documents and the DV simulation results.

Finally, we provide the same set of information for all available [top level designs](#top-level-designs).

## Results of tool-flows

* [DV simulation summary results, with coverage (nightly)](https://reports.opentitan.org/hw/top_earlgrey/dv/summary/latest/report.html)
* [FPV sec_cm results (weekly)](https://reports.opentitan.org/hw/top_earlgrey/formal/sec_cm/summary/latest/report.html)
* [FPV ip results (weekly)](https://reports.opentitan.org/hw/top_earlgrey/formal/ip/summary/latest/report.html)
* [FPV prim results (weekly)](https://reports.opentitan.org/hw/top_earlgrey/formal/prim/summary/latest/report.html)
* [AscentLint summary results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/lint/ascentlint/summary/latest/report.html)
* [Verilator lint summary results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/lint/verilator/summary/latest/report.html)
* [Style lint summary results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/lint/veriblelint/summary/latest/report.html)
* [DV Style lint summary results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/dv/lint/veriblelint/summary/latest/report.html)
* [FPV Style lint summary results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/fpv/lint/veriblelint/summary/latest/report.html)

## Comportable IPs

{{< dashboard "comportable" >}}

## Processor cores

* `core_ibex`
  * [User manual](https://ibex-core.readthedocs.io/en/latest)
  * [DV document](https://ibex-core.readthedocs.io/en/latest/03_reference/verification.html)
  * DV simulation results, with coverage (nightly) (TBD)

## Earl Grey chip-level results

* [Datasheet](top_earlgrey/doc/index.html)
* [Specification](top_earlgrey/doc/design/index.html)
* [DV Document](top_earlgrey/doc/dv/index.html)
* [DV simulation results, with coverage (nightly)](https://reports.opentitan.org/hw/top_earlgrey/dv/latest/report.html)
* [Connectivity results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/conn/jaspergold/latest/report.html)
* [AscentLint results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/lint/ascentlint/latest/report.html)
* [Verilator lint results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/lint/verilator/latest/report.html)
* [Style lint results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/lint/veriblelint/latest/report.html)
* [DV Style lint results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/dv/lint/veriblelint/latest/report.html)
* [CDC results (nightly)](https://reports.opentitan.org/hw/top_earlgrey/cdc/latest/report.html)

### Earl Grey-specific comportable IPs

{{< dashboard "top_earlgrey" >}}

## Hardware documentation overview

{{% sectionContent %}}
