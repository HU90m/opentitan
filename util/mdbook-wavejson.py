#!/usr/bin/env python3
# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
import json
import sys
import re
from typing import List, Any, Dict, Generator

WAVEJSON_PATTERN = re.compile("```wavejson\n(.+?)\n```", re.DOTALL)


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
        chapter["content"] = \
            WAVEJSON_PATTERN.sub(r'<script type="WaveDrom">\1</script>', chapter["content"])

    # dump the book into stdout
    print(json.dumps(book))


if __name__ == "__main__":
    main()
