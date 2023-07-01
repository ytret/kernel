#!/usr/bin/env python3

import subprocess
import sys
from pathlib import Path


def print_help():
    print(f"""\
Usage: {sys.argv[0]} [-h | --help] [-v | --verbose]

  -h, --help     show this help and exit
  -v, --verbose  print called commands""")


def parse_args():
    verbose = False

    for arg in sys.argv[1:]:
        if arg == "-h" or arg == "--help":
            print_help()
            exit(0)

        elif arg == "-v" or arg == "--verbose":
            verbose = True

        else:
            print(f"Unknown argument: '{arg}'.  Aborting.")
            print_help()
            exit(64)

    return verbose


def run_cmd(cmd):
    if verbose:
        print(cmd)

    try:
        if verbose:
            subprocess.run(cmd.split(" "), shell=False, check=True)
        else:
            subprocess.run(
                cmd.split(" "),
                shell=False,
                check=True,
                stderr=subprocess.STDOUT,
                stdout=subprocess.PIPE,
            )
    except subprocess.CalledProcessError as e:
        print(f"Command '{cmd}' failed.  Aborting.")
        print(f"Exit code: {e.returncode}")
        print("Command output:")
        if e.output is not None:
            print(e.output.strip().decode("utf-8"))
        exit(70)


def parallelize_cmd(cmd, paths):
    cmd_parallel = f"parallel --tag --verbose {cmd} :::"
    for path in paths:
        cmd_parallel += f" {path}"
    run_cmd(cmd_parallel)


def compile_asm(paths, as_flags):
    for path in paths:
        if path.suffix != ".s":
            raise ValueError(f"path '{path}' is not a .s file")

    cmd_as = "i686-elf-as {} -o build/{.}.o"
    for flag in as_flags:
        cmd_as += f" {flag}"

    parallelize_cmd(cmd_as, paths)

    objects = []
    for path in paths:
        objects.append("build" / path.parent / (path.stem + ".o"))
    return objects


def compile_c(paths, gcc_flags):
    for path in paths:
        if path.suffix != ".c":
            raise ValueError(f"path '{path}' is not a .c file")

    cmd_gcc = (
        "i686-elf-gcc -ffreestanding -Wall -Wextra -Werror -std=c99"
        " -c {} -o build/{.}.o -Isrc"
    )
    for flag in gcc_flags:
        cmd_gcc += f" {flag}"

    parallelize_cmd(cmd_gcc, paths)

    objects = []
    for path in paths:
        objects.append("build" / path.parent / (path.stem + ".o"))
    return objects


def compile(paths):
    for path in paths:
        assert not path.is_absolute()

    asm_files = [x for x in paths if x.suffix == ".s"]
    c_files = [x for x in paths if x.suffix == ".c"]
    other_files = [x for x in paths if x not in asm_files + c_files]

    if len(other_files) != 0:
        print("Unknown file extension: ")
        for file in other_files:
            print(f"  '{file}'")
        print("Aborting.")
        exit(70)

    # Create subdirectories in build/.
    for path in paths:
        Path("build" / path.parent).mkdir(parents=True, exist_ok=True)

    objects = []
    objects += compile_asm(asm_files, [])
    objects += compile_c(c_files, [])
    return objects


def link(paths, out_file):
    if len(paths) == 0:
        print("There are no object files to link together.  Aborting.")
        exit(70)

    cmd_link = f"i686-elf-ld -T src/link.ld -o {out_file}"
    for path in paths:
        cmd_link += f" {path}"
    run_cmd(cmd_link)


def build():
    out_file = "./build/kernel.elf"

    if verbose:
        print(f"*** 1. Compile {out_file} ***")
    sources = [
        "src/entry.s",
        "src/main.c",
    ]
    src_paths = [Path(src) for src in sources]
    objects = compile(src_paths)

    if verbose:
        print(f"\n*** 2. Link {out_file} ***")
    link(objects, out_file)

    return out_file


verbose = parse_args()
build()
