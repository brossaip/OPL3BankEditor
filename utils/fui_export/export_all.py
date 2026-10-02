#!/usr/bin/env python3
"""Convert every file under a directory tree to Furnace .fui using fui-export.

Each input file gets its own output folder that mirrors the input layout:
    Bank_Examples/sub/foo.wopl  ->  PassatsFui/sub/foo/*.fui
so instrument names from different banks never collide.
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-i", "--input", type=Path, default=ROOT / "Bank_Examples")
    ap.add_argument("-o", "--output", type=Path, default=ROOT / "PassatsFui")
    ap.add_argument("-t", "--tool", type=Path, default=ROOT / "build" / "fui-export")
    args = ap.parse_args()

    if not args.tool.is_file():
        sys.exit(f"fui-export not found: {args.tool}")
    if not args.input.is_dir():
        sys.exit(f"Input directory not found: {args.input}")

    files = sorted(p for p in args.input.rglob("*") if p.is_file())
    ok, failed = 0, []

    for f in files:
        rel = f.relative_to(args.input)
        out_dir = args.output / rel.parent / f.stem
        # Same stem with different extensions (e.g. a.ibk / a.op2) must not clash
        if out_dir.exists():
            out_dir = args.output / rel.parent / f"{f.stem}_{f.suffix.lstrip('.')}"
        out_dir.parent.mkdir(parents=True, exist_ok=True)

        res = subprocess.run([str(args.tool), str(f), str(out_dir)],
                             capture_output=True, text=True)
        if res.returncode == 0:
            ok += 1
            print(f"OK    {rel}")
        else:
            failed.append(rel)
            msg = (res.stderr or res.stdout).strip().splitlines()
            print(f"FAIL  {rel}  {msg[-1] if msg else ''}")

    print(f"\n{ok}/{len(files)} files converted, {len(failed)} failed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
