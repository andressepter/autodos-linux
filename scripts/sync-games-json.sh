#!/usr/bin/env bash
# See usage() or run with --help.

set -euo pipefail

usage() {
  cat <<'EOF'
Sync only games.json from upstream AutoDOS (makuka97) — no git remote needed.

Usage (from repo root):
  ./scripts/sync-games-json.sh              replace src/games.json (backup first)
  ./scripts/sync-games-json.sh --merge      upstream wins on same keys; keep local-only keys

Env:
  GAMES_JSON_URL   default: raw main on makuka97/autoDos
  DEST             default: src/games.json
EOF
}

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

URL="${GAMES_JSON_URL:-https://raw.githubusercontent.com/makuka97/autoDos/main/src/games.json}"
DEST="${DEST:-src/games.json}"
TMP="${TMPDIR:-/tmp}/autodos-games-json.$$"
MERGE=0

die() { echo "sync-games-json: $*" >&2; exit 1; }

case "${1:-}" in
  "" ) ;;
  --merge) MERGE=1 ;;
  -h|--help) usage; exit 0 ;;
  *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
esac
[[ -z "${2:-}" ]] || die "extra arguments (use only: --merge or --help)"

command -v curl >/dev/null || die "curl required"
command -v python3 >/dev/null || die "python3 required (for JSON check and optional merge)"

curl -fsSL "$URL" -o "$TMP" || die "download failed: $URL"

python3 - "$TMP" <<'PY' || die "downloaded file is not valid JSON"
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    json.load(f)
PY

if [[ "$MERGE" -eq 1 ]]; then
  [[ -f "$DEST" ]] || die "cannot --merge: missing $DEST"
  OUT="$TMP.merged"
  python3 - "$DEST" "$TMP" "$OUT" <<'PY'
import json, sys
local_path, upstream_path, out_path = sys.argv[1:4]
with open(local_path, encoding="utf-8") as f:
    loc = json.load(f)
with open(upstream_path, encoding="utf-8") as f:
    up = json.load(f)
lg = loc.get("games") or {}
ug = up.get("games") or {}
merged_games = dict(lg)
merged_games.update(ug)
out = dict(up)
out["games"] = merged_games
meta = dict(up.get("_meta") or {})
meta["games"] = len(merged_games)
up_note = (meta.get("note") or "").strip()
meta["note"] = (up_note + " [+local keys preserved]") if up_note else "[+local keys preserved]"
out["_meta"] = meta
with open(out_path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2)
    f.write("\n")
PY
  mv "$OUT" "$TMP"
fi

if [[ -f "$DEST" ]]; then
  BAK="${DEST}.bak.$(date +%Y%m%d%H%M%S)"
  cp -a "$DEST" "$BAK"
  echo "Backed up: $BAK"
fi

mkdir -p "$(dirname "$DEST")"
mv "$TMP" "$DEST"
echo "Wrote: $DEST (from $URL)"
if [[ "$MERGE" -eq 1 ]]; then
  echo "Mode: merge (upstream overwrote matching keys; local-only game keys kept)"
else
  echo "Mode: replace (full file from upstream)"
fi
