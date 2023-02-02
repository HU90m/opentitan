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
cargo install mdbook mdbook-variables
```

## To build pages individually
This may be easier for rapid development as using `hugo/mdbook serve` gives you a live site that will update itself automatically with your latest changes.

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

## To build all pages together
Building this way will give you a live local site which you can click between the different books and also the top-level static site pages. This is useful to test out links between books. However, in this mode the pages do not update themselves automatically, you will need to Ctrl-C to kill the command and restart it.

```sh
./util/build-docs.sh serve
```

## Linking between books when writing Markdown
Becuase we use a number of seperately-generated mdbook "Books", there are three strategies you will need when constructing links to other pages.

1) Links to external sites
Just use standard URLs
```markdown
[google.com](https://google.com)
```

2) Linking to pages within the same book
Here you want to use the relative path between the markdown document you are current in, and the path of the document you want to link to. However, this is the relative path according to the structure of the book's chapters and pages, which you can see in the `SUMMARY.md` file, of which there is one for each book.
E.g. in this repository, the hardware book root is at `hw/`. If you were in the markdown file `hw/ip/clkmgr/README.md`, and you wanted to create a link to the CSRNG IP page, you would use the link format as follows:
```markdown
# In the file hw/SUMMARY.md, you can find the lines...
#   - [Clock Manager](./ip/clkmgr/README.md)
#   - [CSRNG](./ip/csrng/README.md)

# Therefore, within hw/ip/README.md, you would use the link...
[CSRNG HWIP](./../csrng/)
# (if the path is to a directory, that implicitly links to the README.md in that directory)
```

3) Linking to pages in a different book
This requires a special syntax, as the final link URL is going to be dependent on where the books are located relative to each other on the website. You will need to look at the `SUMMARY.md` file in the book you want to link to, as well as the first path element identifying the book. You can use the following syntax...
```markdown
# Constructing a link
# from : sw/README.md (the homepage of the Software Book)
# to   : doc/security/src/threat_model/README.md (a page in the Security Book)

# First look at the SUMMARY.md file for the security book (doc/security/SUMMARY.md)
# You will find the line..
# - [Lightweight Threat Model](threat_model/README.md)

# The link syntax is then constructed as :
#  {{URL_ROOT}}   : this is always the starting element
#  /security      : this is the book you are linking into
#  /threat_model/ : this is the page in the book you are linking to
[OpenTitan Threat Model]({{URL_ROOT}}/security/threat_model/)
# (if the path is to a directory, that implicitly links to the README.md in that directory)
```


# About the project

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
