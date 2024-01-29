# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

load("//rules:doc.bzl", "mdbook_build")

exports_files(["book-theme"])

mdbook_build(
    name = "opentitan_book",
    theme = ":book-theme",
    preprocessors = [
        "//util:mdbook_readme2index",
        #"//util:mdbook_toolversion",
    ],
    srcs = [
        "//:mdbook_config",
        "//site/book-theme:all_files",
        "//util:mdbook",
    ],
)
