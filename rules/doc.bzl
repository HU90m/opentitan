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
    output_dir = ctx.actions.declare_directory("book")

    #process_wrapper_flags.add("--subst", "pwd=${pwd}")

    ctx.actions.run(
        mnemonic = "mdBook",
        progress_message = "Generating mdBook for {}".format(ctx.label),
        outputs = [output_dir],
        executable = ctx.executable._mdbook,
        inputs = ctx.files.srcs,
        arguments = ["build", ctx.files.dir[0].path, "-d", "book"],
    )

    return [
        DefaultInfo(files = depset([output_dir])),
        OutputGroupInfo(
            output_dir = depset([output_dir]),
        ),
    ]



mdbook_build = rule(
    implementation = _mdbook_build_impl,
    attrs = {
        #"out": attr.output(mandatory = True),
        "dir": attr.label(doc = "Book root", allow_single_file=True, mandatory=True),
        "srcs": attr.label_list(doc = "Input files", mandatory=True),
        "_mdbook": attr.label(default=Label("@crate_index//:mdbook__mdbook"),
                             allow_files=True, executable=True, cfg="host")
    },
)
