# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Documentation rules for OpenTitan.
"""

def _mdbook_build_impl(ctx):
    output_dir = ctx.actions.declare_directory("book")
    output = ctx.actions.declare_file("book.tar.gz")

    ENV_PP_KEY = "MDBOOK_PREPROCESSOR__{}__COMMAND"

    env = {}
    for (idx, pp) in enumerate(ctx.attr.preprocessors):
        env[ENV_PP_KEY.format(idx)] = pp.files_to_run.executable.path

    env["RUNFILES_DIR"] = "util/"

    MAKE_ABS = 'abs() {\necho "$(cd "$(dirname "$1")" && pwd -P)/$(basename "$1")";\n};\n'
    cmd = MAKE_ABS

    #print(ctx.attr.theme)
    if ctx.attr.theme:
        #print(ctx.files.theme[0].path)
        #env["MDBOOK_OUTPUT__HTML__THEME"] = ctx.files.theme[0].path
        cmd += "MDBOOK_OUTPUT__HTML__THEME=$(abs {});\n".format(ctx.files.theme[0].path)

    #A = "MDBOOK_OUTPUT__HTML__ADDITIONAL_CSS"
    #B = "MDBOOK_OUTPUT__HTML__ADDITIONAL_JS"
    #
    #additional-css = ["book-theme/pagetoc.css",
    #                  "book-theme/recursive.css"]
    #additional-js  = ["book-theme/pagetoc.js"]

    cmd += "{} build -d $(abs {});".format(
        ctx.executable._mdbook.path,
        output_dir.path,
    )

    print(cmd)


    ctx.actions.run_shell(
        mnemonic = "mdBook",
        progress_message = "Generating mdBook: {}".format(ctx.label),
        outputs = [output_dir],
        inputs = ctx.files.srcs + [ctx.executable._mdbook] + ctx.files.preprocessors,
        command = cmd,
        #"{} build {} && mv {}/book/* {}".format(
        #    ctx.executable._mdbook.path,
        #    book_root,
        #    book_root,
        #    output_dir.path,
        #),
        env = env,
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
        "srcs": attr.label_list(doc = "Input files", mandatory=True),
        "preprocessors": attr.label_list(
            doc = "mdBook preprocessors",
            #providers=["FilesToRunProvider"],
        ),
        "theme": attr.label(allow_single_file=True),
        "_mdbook": attr.label(default=Label("@crate_index//:mdbook__mdbook"), allow_files=True, executable=True, cfg="host"),
        "_tar": attr.label(default=Label("//rules/scripts:tar"), allow_files=True, executable=True, cfg="host"),
    },
)
