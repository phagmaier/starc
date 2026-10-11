#!/usr/bin/env python3
"""Extract a clangd database from GNU make's dry run of a completed qmake build.

Usage: tools/compdb-from-make.py .build/debug [--output PATH]

Run only after the real build completes. GNU make executes recursive-make
recipe lines even with -n; existing child Makefiles prevent qmake's conditional
configuration recipes from running. --old-file=Makefile suppresses refresh of
the generated Makefiles. This tool never requests a real compilation or clean.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


DIRECTORY = re.compile(r"^make(?:\[\d+\])?: (Entering|Leaving) directory ['`](.*)'$")
COMPILER = re.compile(r"(?:^|-)(?:g\+\+|gcc|clang\+\+|clang|c\+\+|cc)(?:-[\d.]+)?$")
SOURCE_SUFFIXES = {".cpp", ".cc", ".c", ".cxx", ".C"}
WRAPPERS = {"ccache", "sccache", "distcc"}


def compiler_index(words):
    """Recognize compiler commands, optionally preceded by a compiler wrapper."""
    if not words:
        return None
    index = 0
    if Path(words[0]).name in WRAPPERS:
        index = 1
    if index < len(words) and COMPILER.search(Path(words[index]).name):
        return index
    return None


def parse_commands(output, build_dir):
    directories = []
    entries = {}
    continued = ""
    for number, raw_line in enumerate(output.splitlines(), 1):
        line = continued + raw_line
        if line.endswith("\\"):
            continued = line[:-1] + " "
            continue
        continued = ""
        match = DIRECTORY.match(line)
        if match:
            operation, name = match.groups()
            directory = Path(name).resolve()
            if operation == "Entering":
                directories.append(directory)
            else:
                if not directories or directories[-1] != directory:
                    raise ValueError(f"line {number}: inconsistent make directory nesting")
                directories.pop()
            continue
        stripped = line.lstrip()
        # Other output includes linker commands, diagnostics, and echoed recipes.
        first_word = stripped.split(maxsplit=1)[0] if stripped else ""
        if not (COMPILER.search(Path(first_word).name) or Path(first_word).name in WRAPPERS):
            continue
        try:
            words = shlex.split(stripped, posix=True)
        except ValueError as error:
            raise ValueError(f"line {number}: cannot parse compiler command: {error}") from error
        if compiler_index(words) is None or "-c" not in words:
            continue
        if any(word in {"&&", "||", ";", "|", ">", ">>"} for word in words):
            raise ValueError(f"line {number}: compiler shell expressions are unsupported")
        sources = [word for word in words if not word.startswith("-") and Path(word).suffix in SOURCE_SUFFIXES]
        if not sources:
            raise ValueError(f"line {number}: -c command has no recognized source file")
        directory = directories[-1] if directories else build_dir
        source = (directory / sources[-1]).resolve()
        if not source.is_file():
            raise ValueError(f"line {number}: source does not exist: {source}")
        entries[str(source)] = {
            "directory": str(directory),
            "file": str(source),
            "arguments": words,
        }
    if continued:
        raise ValueError("dry-run output ends in an unfinished command")
    if directories:
        raise ValueError("dry-run output ends inside a make directory")
    if not entries:
        raise ValueError("dry run produced no compiler commands; existing database preserved")
    return [entries[name] for name in sorted(entries)]


def write_database(destination, entries):
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=destination.parent,
                                         prefix=".compile_commands.", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(entries, stream, indent=2)
            stream.write("\n")
        os.replace(temporary, destination)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("build_dir", type=Path, help="completed qmake shadow build directory")
    parser.add_argument("--output", type=Path, help="default: BUILD_DIR/compile_commands.json")
    options = parser.parse_args()
    build_dir = options.build_dir.resolve()
    destination = (options.output or build_dir / "compile_commands.json").resolve()
    try:
        if not (build_dir / "Makefile").is_file():
            raise ValueError(f"no generated Makefile: {build_dir}; complete a real build first")
        environment = os.environ.copy()
        environment.update(LC_ALL="C")
        environment.pop("MAKEFLAGS", None)
        environment.pop("MFLAGS", None)
        result = subprocess.run(
            ["make", "-n", "-B", "-j1", "--print-directory", "--old-file=Makefile"],
            cwd=build_dir, env=environment, text=True, capture_output=True, check=False,
        )
        if result.returncode:
            raise ValueError(f"make dry run failed ({result.returncode}):\n{result.stderr.strip()}")
        entries = parse_commands(result.stdout, build_dir)
        write_database(destination, entries)
        print(f"Wrote {len(entries)} compiler commands to {destination}")
        if result.stderr.strip():
            print(result.stderr.strip(), file=sys.stderr)
    except (OSError, ValueError) as error:
        print(f"Error: {error}\nExisting compilation database was preserved.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
