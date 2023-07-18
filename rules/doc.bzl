# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Documentation rules for OpenTitan.
"""

def _mdbook_build_impl(ctx):
    output_dir = ctx.actions.declare_directory("book")
    output = ctx.actions.declare_file("book.tar.gz")

    ctx.actions.run_shell(
        mnemonic = "mdBook",
        progress_message = "Generating mdBook: {}".format(ctx.label),
        outputs = [output_dir],
        inputs = ctx.files.srcs + [ctx.executable._mdbook],
        command = "pushd {} && ~1/{} build && mv book/* ~1/{}".format(
            ctx.files.dir[0].path,
            ctx.executable._mdbook.path,
            output_dir.path,
        )
    )

    ctx.actions.run(
        mnemonic = "mdBookTar",
        progress_message = "Collecting mdBook: {}".format(ctx.label),
        outputs = [output],
        executable = ctx.executable._tar,
        inputs = [output_dir],
        arguments = [
            output_dir.path,
            output.path,
        ],
    )

    return [DefaultInfo(files = depset([output]))]

mdbook_build = rule(
    implementation = _mdbook_build_impl,
    attrs = {
        "dir": attr.label(doc = "Book root", allow_single_file=True, mandatory=True),
        "srcs": attr.label_list(doc = "Input files", mandatory=True),
        "_mdbook": attr.label(default=Label("@crate_index//:mdbook__mdbook"), allow_files=True, executable=True, cfg="host"),
        "_tar": attr.label(default=Label("//rules/scripts:tar"), allow_files=True, executable=True, cfg="host"),
    },
)
