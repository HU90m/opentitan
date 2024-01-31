# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

load("//rules:doc.bzl", "mdbook_build", "compile_site")

exports_files(["book-theme"])

mdbook_build(
    name = "opentitan_book",
    theme = ":book-theme",
    preprocessors = [
        "//util:mdbook_readme2index",
        "//util:mdbook_toolversion",
    ],
    srcs = [
        "//:mdbook_config",
        "//:doc_files",
        "//hw:doc_files",
        "//hw/top_earlgrey:doc_files",
        "//hw/top_earlgrey/dv:doc_files",
        "//sw/host/hsmtool:doc_files",
        "//util:doc_files",
        "//site/book-theme:all_files",
        "//util:mdbook",
    ],
)


compile_site(
  name = "opentitan_site",
  mapping = {
    ":opentitan_book" : "book",
    "//sw/host/opentitanlib:opentitanlib_doc" : "gen/rustdoc/opentitanlib",
    "//sw/host/hsmtool:hsmlib_doc" : "gen/rustdoc/hsmtool",
    "//sw/host/ot_certs:ot_certs_doc" : "gen/rustdoc/ot_certs",
  },
)
