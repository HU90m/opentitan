# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

load("//rules:doc.bzl", "mdbook_build")

mdbook_build(
    name = "getting_started_guide",
    dir = "//doc/guides:getting_started",
    theme = "//site:book-theme",
    preprocessors = [
        "//util:mdbook_readme2index",
        #"//util:mdbook_toolversion",
    ],
    srcs = [
        ":all_files",
        "//site/book-theme:all_files",
    ],
)

filegroup(
    name = "all_files",
    srcs = glob(["**"]),
)
