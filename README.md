# OpenTitan

![OpenTitan logo](https://docs.opentitan.org/doc/opentitan-logo.png)

## Documentation Summary

### Install Dependencies
```sh
# Using you package manager of choice...

# Install hugo
sudo apt install hugo

# Install mdbook
# 1) Install Rust
# https://www.rust-lang.org/tools/install
# 2) Then mdbook using cargo
cargo install mdbook
```

### Top-Level Pages
```sh
cd site/landing
cd site/docs
hugo serve # Start an local webserver hosting the build site
```
### Books
```sh
cd hw/ # Hardware books
cd util/ # Tooling book
cd sw/ # SW Book
cd doc/books/project-governance # Project
cd doc/books/security # Security
cd doc/books/use-cases # Use-Cases
mdbook serve  # Start an local webserver hosting the build site
```

## About the project

[OpenTitan](https://opentitan.org) is an open source silicon Root of Trust
(RoT) project.  OpenTitan will make the silicon RoT design and implementation
more transparent, trustworthy, and secure for enterprises, platform providers,
and chip manufacturers.  OpenTitan is administered by [lowRISC
CIC](https://www.lowrisc.org) as a collaborative project to produce high
quality, open IP for instantiation as a full-featured product. See the
[OpenTitan site](https://opentitan.org/) and [OpenTitan
docs](https://docs.opentitan.org) for more information about the project.

## About this repository

This repository contains hardware, software and utilities written as part of the
OpenTitan project. It is structured as monolithic repository, or "monorepo",
where all components live in one repository. It exists to enable collaboration
across partners participating in the OpenTitan project.

## Documentation

The project contains comprehensive documentation of all IPs and tools. You can
access it [online at docs.opentitan.org](https://docs.opentitan.org/).

## How to contribute

Have a look at [CONTRIBUTING](https://github.com/lowRISC/opentitan/blob/master/CONTRIBUTING.md) and our [documentation on
project organization and processes](https://docs.opentitan.org/doc/project/)
for guidelines on how to contribute code to this repository.

## Licensing

Unless otherwise noted, everything in this repository is covered by the Apache
License, Version 2.0 (see [LICENSE](https://github.com/lowRISC/opentitan/blob/master/LICENSE) for full text).
