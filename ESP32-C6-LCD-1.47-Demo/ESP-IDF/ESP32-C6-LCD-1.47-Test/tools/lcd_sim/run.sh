#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

if [[ ! -d .venv ]]; then
  python3.12 -m venv .venv 2>/dev/null || python3 -m venv .venv
  .venv/bin/pip install -r requirements.txt -q
fi

exec .venv/bin/python main.py
