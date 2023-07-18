#!/bin/env python3

import argparse
import tarfile
import subprocess
import sys
import os

def main():
    parser = argparse.ArgumentParser(prog="tar")
    parser.add_argument("directory", help="Directory to collect into a tar file.")
    parser.add_argument("output", help="The name of the output tar file.")
    args = parser.parse_args()

    with tarfile.open(args.output, "w:gz", dereference=True) as tar:
        for file in os.listdir(args.directory):
            tar.add(args.directory + "/" + file, arcname=file, recursive=True)

if __name__ == "__main__":
    main()
