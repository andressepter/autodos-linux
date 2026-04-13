#!/usr/bin/env python3
"""Build a games.json fragment for an upstream merge request.

By default includes only keys that exist in --local but not in upstream (new entries).
Use --include-overrides to also emit keys where local JSON differs from upstream.
"""
from __future__ import annotations

import argparse
import json
import sys
import urllib.request


def load_games(path: str) -> dict:
    if not path:
        return {}
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f).get("games") or {}
    except OSError:
        return {}
    except json.JSONDecodeError as e:
        print(f"export: invalid JSON in {path}: {e}", file=sys.stderr)
        sys.exit(2)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--upstream-url",
        default="https://raw.githubusercontent.com/makuka97/autoDos/main/src/games.json",
        help="Raw games.json URL (default: makuka97 main)",
    )
    ap.add_argument("--local", required=True, help="Your games.local.json (or overlay file)")
    ap.add_argument("--out", required=True, help="Output JSON path")
    ap.add_argument(
        "--include-overrides",
        action="store_true",
        help="Also include keys that exist upstream but differ in local",
    )
    args = ap.parse_args()

    try:
        with urllib.request.urlopen(args.upstream_url, timeout=60) as r:
            upstream = json.load(r)
    except Exception as e:
        print(f"export: failed to fetch upstream: {e}", file=sys.stderr)
        sys.exit(1)

    up_games = upstream.get("games") or {}
    loc_games = load_games(args.local)

    contrib: dict = {}
    for k, v in loc_games.items():
        if k not in up_games:
            contrib[k] = v
            continue
        if args.include_overrides and json.dumps(up_games[k], sort_keys=True) != json.dumps(
            v, sort_keys=True
        ):
            contrib[k] = v

    out = {
        "_meta": {
            "description": "PR fragment — merge `games` into upstream games.json",
            "upstream_url": args.upstream_url,
            "entries": len(contrib),
        },
        "games": contrib,
    }

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
        f.write("\n")

    print(f"Wrote {args.out} with {len(contrib)} game entries", file=sys.stderr)


if __name__ == "__main__":
    main()
