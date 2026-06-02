#!/usr/bin/env python3
"""Resolve kernel panic stacktrace addresses to function names."""

import bisect
import os
import re
import shutil
import subprocess
import sys

# ---------------------------------------------------------------------------
# 1.  Symbol table loading via i686-elf-nm
# ---------------------------------------------------------------------------

SYM_TYPES = frozenset({"T", "t", "W", "w"})


def _parse_nm_with_size(line: str):
    """Try to parse a line with size: ``addr size type name``."""
    m = re.match(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([tTwWvV])\s+(.+)$", line)
    if not m:
        return None
    if m.group(3) not in SYM_TYPES:
        return None
    return int(m.group(1), 16), int(m.group(2), 16), m.group(4)


def _parse_nm_no_size(line: str):
    """Try to parse a line without size: ``addr type name``."""
    m = re.match(r"^([0-9a-fA-F]+)\s+([tTwWvV])\s+(.+)$", line)
    if not m:
        return None
    if m.group(2) not in SYM_TYPES:
        return None
    return int(m.group(1), 16), 0, m.group(3)


def _load_nm_output(nm_stdout: str) -> list[tuple[int, int, str]]:
    """Parse nm output into sorted list of ``(addr, size, name)``.

    Tries to parse with size first; falls back to no-size for any line
    that doesn't match the size format.
    """
    syms = []
    for line in nm_stdout.splitlines():
        parsed = _parse_nm_with_size(line)
        if parsed is not None:
            syms.append(parsed)
            continue
        parsed = _parse_nm_no_size(line)
        if parsed is not None:
            syms.append(parsed)
    syms.sort(key=lambda x: x[0])
    return syms


def _resolve_nm(nm_args: list[str], kernel_path: str) -> tuple[str, bool]:
    """Run i686-elf-nm and return ``(stdout, has_size)``.

    *has_size* is ``True`` if ``-S`` was successfully used.
    """
    # Try with -S first.
    try:
        r = subprocess.run(
            nm_args + ["-S", kernel_path],
            capture_output=True,
            text=True,
            check=True,
        )
        return r.stdout, True
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # Fall back without -S.
    try:
        r = subprocess.run(
            nm_args + [kernel_path],
            capture_output=True,
            text=True,
            check=True,
        )
        return r.stdout, False
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        print(
            f'error: cannot run {" ".join(nm_args)} on {kernel_path}: {exc}',
            file=sys.stderr,
        )
        sys.exit(1)


def _default_kernel() -> str:
    """Return the default kernel path relative to the project root.

    Resolves symlinks so the script can be invoked from anywhere
    (e.g. a symlink inside ``build/``).
    """
    script = os.path.realpath(__file__)
    # Script lives at <project_root>/tools/resolvestack.py
    project_root = os.path.dirname(os.path.dirname(script))
    candidate = os.path.join(project_root, "build", "kernel")
    if os.path.isfile(candidate):
        return candidate
    return "build/kernel"


def _find_project_root(kernel_path: str) -> str | None:
    """Walk up from *kernel_path* looking for ``flake.nix``."""
    path = os.path.abspath(kernel_path)
    if not os.path.isdir(path):
        path = os.path.dirname(path)
    while True:
        if os.path.isfile(os.path.join(path, "flake.nix")):
            return path
        parent = os.path.dirname(path)
        if parent == path:
            return None
        path = parent


def load_symbols(kernel_path: str) -> tuple[list[tuple[int, int, str]], bool]:
    """Return ``(symbol_list, all_have_sizes)``.

    *symbol_list* is ``[(address, size, name), ...]`` sorted by address.

    The function tries ``i686-elf-nm`` directly first, then wraps it with
    ``nix develop`` if the bare tool is not found.
    """
    # --- strategy 1: bare tool ---
    bare_nm = shutil.which("i686-elf-nm")
    if bare_nm is not None:
        nm_stdout, has_size = _resolve_nm([bare_nm], kernel_path)
        return _load_nm_output(nm_stdout), has_size

    # --- strategy 2: nix develop ---
    project_root = _find_project_root(kernel_path)
    if project_root is not None:
        nix_nm = ["nix", "develop", project_root, "--command", "i686-elf-nm"]
        nm_stdout, has_size = _resolve_nm(nix_nm, kernel_path)
        return _load_nm_output(nm_stdout), has_size

    print(
        "error: i686-elf-nm not found on PATH and no flake.nix project root " "could be located",
        file=sys.stderr,
    )
    sys.exit(1)


# ---------------------------------------------------------------------------
# 2.  Address lookup
# ---------------------------------------------------------------------------


def lookup(addr: int, syms: list[tuple[int, int, str]]) -> str | None:
    """Return the function name containing *addr*, or ``None``.

    Uses binary search to find the nearest symbol whose start address is
    ≤ *addr*, then checks whether *addr* falls inside
    ``[start, start + size)``.
    """
    if not syms:
        return None

    i = bisect.bisect_right(syms, addr, key=lambda x: x[0]) - 1
    if i < 0:
        return None

    start, size, name = syms[i]
    if size == 0:
        # No size info available — it may or may not be the right symbol.
        return f"{name}  <no size info>"
    if addr < start + size:
        return name
    return None


# ---------------------------------------------------------------------------
# 3.  Input parsing & output
# ---------------------------------------------------------------------------

# Matches e.g. `` 1. 0xc001eeb4`` anywhere on a line (including after
# log-prefix garbage like ``E    prv_panic_log_stacktrace:0200``).
_STACKFRAME_RE = re.compile(r"\s(\d+)\.\s+0x([0-9a-fA-F]+)\s*$")

# Matches the "stacktrace:" header (any amount of leading cruft).
_STACKTRACE_HEADER_RE = re.compile(r"^\s*stacktrace\s*:\s*$")


def process_stream(
    infile,
    syms: list[tuple[int, int, str]],
    has_size: bool,
) -> None:
    """Read *infile*, resolve stack frames and write annotated output."""
    for line in infile:
        m = _STACKFRAME_RE.search(line)
        if m:
            addr = int(m.group(2), 16)
            label = lookup(addr, syms)
            if label is None:
                label = "<unknown>"
            print(f" {m.group(1)}. 0x{m.group(2).lower()}  {label}")
            continue

        # Pass the header through (optionally clean it up).
        if _STACKTRACE_HEADER_RE.search(line):
            print("stacktrace:")
            continue

        # Everything else: pass through unchanged.
        print(line)


# ---------------------------------------------------------------------------
# 4.  Entry point
# ---------------------------------------------------------------------------


def _print_help() -> None:
    print(
        "usage: resolvestack.py [<kernel>] [< stacktrace.log]\n"
        "\n"
        "The default kernel path is ``<script_dir>/../build/kernel``.\n"
        "\n"
        "Annotate a kernel panic stacktrace with function names.\n"
        "\n"
        "Given a stacktrace like:\n"
        "   1. 0xc001eeb4\n"
        "   2. 0xc001eaf5\n"
        "\n"
        "produces:\n"
        "   1. 0xc001eeb4  foo\n"
        "   2. 0xc001eaf5  bar  <no size info>\n"
        "   3. 0xdeadc0de  <unknown>\n"
        "\n"
        "Addresses are looked up via ``i686-elf-nm -S`` on <kernel>.\n"
        "If the address falls within a function's known range the\n"
        "function name is printed.  A symbol with no size info is\n"
        "flagged as ``<no size info>``.  Addresses not belonging\n"
        "to any known function show ``<unknown>``.\n"
        "\n"
        "If no input is piped, the script reads lines interactively.",
        file=sys.stderr,
    )


def _read_input():
    """Read from stdin, prompting interactively if stdin is a terminal."""
    if sys.stdin.isatty():
        print("Paste stacktrace (Ctrl+D or empty line to finish):", file=sys.stderr)
        lines = []
        while True:
            try:
                line = input()
            except EOFError:
                break
            if not line:
                break
            lines.append(line)
        return lines
    # Strip trailing newlines to keep lines consistent.
    return [line.rstrip("\n") for line in sys.stdin]


def main() -> None:
    if len(sys.argv) > 2:
        _print_help()
        sys.exit(0)
    if len(sys.argv) == 2 and sys.argv[1] in {"-h", "--help"}:
        _print_help()
        sys.exit(0)

    kernel_path = sys.argv[1] if len(sys.argv) == 2 else _default_kernel()
    if not os.path.isfile(kernel_path):
        print(f"error: {kernel_path}: no such file", file=sys.stderr)
        sys.exit(1)

    lines = _read_input()
    if not lines:
        sys.exit(0)

    syms, has_size = load_symbols(kernel_path)
    process_stream(lines, syms, has_size)


if __name__ == "__main__":
    main()
