# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

load("@rules_pkg//:pkg.bzl", "pkg_tar")
#load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

load("//rules:doc.bzl", "ls", "mdbook_build")

ls(
    name = "bin",
    srcs = [":all_files"],
)

ls(
    name = "ls_guide",
    srcs = [":getting_started_guide"],
)

ls(
    name = "ls_rust_doc",
    srcs = ["//sw/host/opentitanlib:opentitanlib_doc"],
)

mdbook_build(
    name = "getting_started_guide",
    dir = "//doc/guides:getting_started",
    #out = "getting_started_build",
    srcs = [":all_files"],
)

filegroup(
    name = "all_files",
    srcs = glob(["**"]),
)

pkg_tar(
    name = "getting_started_tar",
    srcs = [":getting_started_guide"],
)
