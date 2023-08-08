# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Documentation rules for OpenTitan.
"""

def _ls_impl(ctx):
    out = ctx.actions.declare_file('hello')
    c = "Files:\n"
    for target in ctx.attr.srcs:
        for file in target.files.to_list():
            c += file.path + "\n"

    ctx.actions.write(
        output = out,
        content = c,#"Hello {}!\n".format(ctx.attr.file_name),
    )
    return [DefaultInfo(files = depset([out]))]

ls = rule(
    implementation = _ls_impl,
    attrs = {
        "srcs": attr.label_list(doc = "Input files"),
    }
)


def _mdbook_build_impl(ctx):
    #out_dir_str = "{}.book".format(ctx.label.name)
    #output_dir = ctx.actions.declare_directory("book")
    output = ctx.actions.declare_file("book.tar")

    #process_wrapper_flags.add("--subst", "pwd=${pwd}")

    out_dir = "lkhasdg"
    ctx.actions.run(
        mnemonic = "mdBook",
        progress_message = "Generating mdBook for {}".format(ctx.label),
        outputs = [output],
        executable = ctx.executable._run_and_collect,
        inputs = ctx.files.srcs + ctx.files._mdbook,
        arguments = [
            out_dir,
            output.path,
            ctx.executable._mdbook.path,
            "build",
            ctx.files.dir[0].path,
            "-d",
            out_dir,
        ],
    )

    return [DefaultInfo(files = depset([output]))]



mdbook_build = rule(
    implementation = _mdbook_build_impl,
    attrs = {
        #"out": attr.output(mandatory = True),
        "dir": attr.label(doc = "Book root", allow_single_file=True, mandatory=True),
        "srcs": attr.label_list(doc = "Input files", mandatory=True),
        "_mdbook": attr.label(default=Label("@crate_index//:mdbook__mdbook"), allow_files=True, executable=True, cfg="host"),
        "_run_and_collect": attr.label(default=Label("//rules/scripts:run_and_collect"), allow_files=True, executable=True, cfg="host"),
    },
)
