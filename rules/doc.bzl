# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

"""Documentation rules for OpenTitan.
"""

def doc_files(
  include = ["**/*.md", "**/*.svg"],
  exclude = [],
):
  native.filegroup(
    name = "doc_files",
    srcs = native.glob(
      include = include,
      exclude = exclude,
    ),
    visibility = ["//site:__pkg__"],
  )

def _mdbook_build_impl(ctx):
    output_dir = ctx.actions.declare_directory("book")
    #output = ctx.actions.declare_file("book.tar.gz")

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
    cmd += "rm -r {}/bazel-out\n".format(output_dir.path)

    ctx.actions.run_shell(
        mnemonic = "mdBook",
        progress_message = "Generating mdBook: {}".format(ctx.label),
        outputs = [output_dir],
        inputs = ctx.files.srcs + [ctx.executable._mdbook],
        tools = [preproc[DefaultInfo].files_to_run for preproc in ctx.attr.preprocessors],
        command = cmd,
        env = env,
    )

    #ctx.actions.run(
    #    mnemonic = "mdBookTar",
    #    progress_message = "Collecting mdBook: {}".format(ctx.label),
    #    outputs = [output],
    #    executable = ctx.executable._tar,
    #    inputs = [output_dir],
    #    arguments = [
    #        output_dir.path,
    #        output.path,
    #        "-e",
    #        "bazel-out",
    #    ],
    #)

    return [DefaultInfo(files = depset([output_dir]))]


mdbook_build = rule(
    implementation = _mdbook_build_impl,
    attrs = {
        "srcs": attr.label_list(doc = "Input files", mandatory=True),
        "preprocessors": attr.label_list(
            doc = "mdBook preprocessors",
        ),
        "theme": attr.label(allow_single_file=True),
        "_mdbook": attr.label(default=Label("@crate_index//:mdbook__mdbook"), allow_files=True, executable=True, cfg="host"),
        "_tar": attr.label(default=Label("//rules/scripts:tar"), allow_files=True, executable=True, cfg="host"),
    },
)

def _compile_site_impl(ctx):
    output_dir = ctx.actions.declare_directory("site")

    print(ctx.attr.mapping)

    cmd = ""
    for (target, destination) in ctx.attr.mapping.items():
      dest = "{}/{}".format(output_dir.path, destination)
      cmd += "mkdir -p {}\n".format(dest)
      cmd += "cp -rL {}/* {}\n".format(target.files.to_list()[0].path, dest)

    ctx.actions.run_shell(
        mnemonic = "Site",
        progress_message = "Collating Site: {}".format(ctx.label),
        outputs = [output_dir],
        inputs = ctx.files.mapping,
        command = cmd,
    )

    output = ctx.actions.declare_file("site.tar.gz")
    ctx.actions.run(
        mnemonic = "SiteTar",
        progress_message = "Collecting Site: {}".format(ctx.label),
        outputs = [output],
        executable = ctx.executable._tar,
        inputs = [output_dir],
        arguments = [
            output_dir.path,
            output.path,
        ],
    )

    return [
      DefaultInfo(files = depset([output_dir])),
      OutputGroupInfo(site_tar = depset([output])),
    ]

compile_site = rule(
    implementation = _compile_site_impl,
    attrs = {
      "mapping": attr.label_keyed_string_dict(
        allow_empty=False,
      ),
      "_tar": attr.label(default=Label("//rules/scripts:tar"), allow_files=True, executable=True, cfg="host"),
    },
)
