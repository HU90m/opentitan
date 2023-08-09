#!/bin/env python3

import argparse
import tarfile
import subprocess
import sys
import os

def main():
    parser = argparse.ArgumentParser(prog="run_and_collect")
    parser.add_argument("directory", help="Directory to collect")
    parser.add_argument("output")
    parser.add_argument("cmd", help="Command to run")
    args = parser.parse_args()

    dir = sys.argv[1]
    output = sys.argv[2]
    cmd = sys.argv[3:]

    #print(os.getcwd())
    os.makedirs(dir)
    with open(dir + "/hello_there", "w") as f:
        f.write("hell")
    res = subprocess.run(cmd, capture_output=True)
    print(f"{res}")
    if res.stdout:
        print(res.stdout.decode())
        #if res.returncode != 0 or True:
        #exit(res.stderr.decode())

    print(output)
    with tarfile.open(output, "w:gz") as tar:
        print(f"before {tar}")
        tar.add(dir, recursive=True)
        print(tar)

def main2():
    parser = argparse.ArgumentParser(prog="run_and_collect")
    parser.add_argument("directory", help="Directory to collect")
    parser.add_argument("output", help="tar file")
    args = parser.parse_args()


    with tarfile.open(args.output, "w:gz", dereference=True) as tar:
        for file in os.listdir(args.directory):
            print(file)
            tar.add(file, recursive=True)

if __name__ == "__main__":
    main2()
