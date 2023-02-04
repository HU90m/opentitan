#!/usr/bin/env python3
# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
import json
import sys
import re
from typing import List, Any, Dict, Generator
from io import StringIO

from reggen.ip_block import IpBlock
import reggen.gen_cfg_html as gen_cfg_html
import reggen.gen_html as gen_html

BLOCK_CFG_PATTERN = re.compile(r"ip/.+/data/.+\.hjson")


def chapters(items: List[dict[str, Any]]) -> Generator[Dict[str, Any], None, None]:
    """Recursively yields all chapters"""
    for chapter in (item.get("Chapter") for item in items):
        if not chapter:
            continue

        for cfg in chapters(chapter["sub_items"]):
            yield cfg

        yield chapter


def main() -> None:
    if len(sys.argv) > 2:
        if (sys.argv[1], sys.argv[2]) == ("supports", "html"):
            sys.exit(0)
        else:
            sys.exit(1)

    # load both the context and the book from stdin
    context, book = json.load(sys.stdin)

    chapters_gen = chapters(book["sections"])
    for chapter in chapters_gen:
        if not BLOCK_CFG_PATTERN.match(chapter["source_path"]):
            continue

        regs = IpBlock.from_text(
            chapter["content"],
            [],
            "file at {}/{}".format(context["root"], chapter["source_path"])
        )
        buffer = StringIO()
        buffer.write("## Hardware Interface\n")
        gen_cfg_html.gen_cfg_html(regs, buffer)
        buffer.write("\n## Registers\n")
        gen_html.gen_html(regs, buffer)
        chapter["content"] = buffer.getvalue()

    # dump the book into stdout
    print(json.dumps(book))


if __name__ == "__main__":
    main()
