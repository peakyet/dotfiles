#!/usr/bin/env python3
"""Waybar Shanghai clock with read-only Trello Doing lists and offline caching."""

from __future__ import annotations

import json
import os
import sys
import tempfile
import textwrap
from datetime import datetime
from getpass import getpass
from html import escape
from pathlib import Path
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen
from zoneinfo import ZoneInfo

TIMEZONE = ZoneInfo("Asia/Shanghai")
STATE = Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local/state")) / "waybar/trello"
CREDENTIALS = STATE / "credentials.json"
CACHE = STATE / "cards.json"
# Each board's 03 · Doing (limit 2), in display order. List IDs are not secrets.
LISTS = (
    ("Control, Optimization & Planning", "68f4a3380f4540cdea27a5eb"),
    ("Programming & Developer Tools", "6adda7dc9c9ec60cd2e3f55c"),
    ("Engineering Methods & Workflows", "6adda7de2ce730f8aaf67e8b"),
    ("Personal & Life Admin", "68de1aa61eb1f1d326058245"),
)


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(dir=path.parent)
    try:
        with os.fdopen(fd, "w") as output:
            json.dump(value, output, ensure_ascii=False)
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def setup() -> None:
    print("Get an API key from https://trello.com/app-key (Power-Up → API key).")
    key = getpass("Trello API key: ").strip()
    url = "https://trello.com/1/authorize?" + urlencode({
        "key": key, "name": "Waybar Doing", "scope": "read",
        "expiration": "never", "response_type": "token",
    })
    print("Open this URL and allow read-only access:\n" + url)
    token = getpass("Trello read-only token: ").strip()
    if not key or not token:
        raise SystemExit("Key and token are required; nothing saved.")
    write_json(CREDENTIALS, {"key": key, "token": token})
    print("Saved privately. The top bar will refresh within a minute.")


def load_cards(now: datetime) -> tuple[dict | None, str]:
    cached = None
    try:
        cached = json.loads(CACHE.read_text())
        if cached["lists"] != [list_id for _, list_id in LISTS]:
            cached = None
        else:
            datetime.fromisoformat(cached["fetched_at"])
    except (OSError, ValueError, KeyError, TypeError):
        cached = None

    if not CREDENTIALS.exists():
        status = "Setup needed · run:\npython ~/.config/waybar/scripts/trello.py --setup"
        if cached is not None:
            status += "\nSnapshot " + datetime.fromisoformat(cached["fetched_at"]).astimezone(TIMEZONE).strftime("%d %b %H:%M")
        return cached, status

    if cached is not None and now.timestamp() - datetime.fromisoformat(cached["fetched_at"]).timestamp() < 300:
        return cached, "Synced " + datetime.fromisoformat(cached["fetched_at"]).astimezone(TIMEZONE).strftime("%d %b %H:%M")

    try:
        credentials = json.loads(CREDENTIALS.read_text())
        # Headers keep the token out of URLs and HTTP error messages.
        authorization = 'OAuth oauth_consumer_key="' + quote(credentials["key"], safe="") + '", oauth_token="' + quote(credentials["token"], safe="") + '"'
        boards = []
        for name, list_id in LISTS:
            url = f"https://api.trello.com/1/lists/{list_id}/cards?" + urlencode({
                "filter": "open", "fields": "name,due,dueComplete,closed,pos,idList",
            })
            with urlopen(Request(url, headers={"Authorization": authorization}), timeout=8) as response:
                cards = json.load(response)
            cards = [card for card in cards if not card["closed"] and card["idList"] == list_id]
            cards.sort(key=lambda card: card["pos"])
            boards.append({"name": name, "cards": cards})
        snapshot = {"lists": [list_id for _, list_id in LISTS], "fetched_at": now.isoformat(), "boards": boards}
        write_json(CACHE, snapshot)
        return snapshot, "Synced " + now.strftime("%d %b %H:%M")
    except Exception:
        # Keep the last complete snapshot; never replace it with partial results.
        if cached is not None:
            timestamp = datetime.fromisoformat(cached["fetched_at"]).astimezone(TIMEZONE).strftime("%d %b %H:%M")
            return cached, "Sync failed · cached " + timestamp + "\nCheck network and Trello credentials."
        return None, "Sync failed · check network and Trello credentials."


def cards_tooltip(snapshot: dict | None, now: datetime, status: str) -> str:
    lines = ["<big>Trello · Doing</big>"]
    if snapshot is not None:
        total = sum(len(board["cards"]) for board in snapshot["boards"])
        lines.append(f"<small>{total} cards · Asia/Shanghai</small>")
        shown = 0
        for board in snapshot["boards"]:
            if shown >= 12:
                break
            lines.extend(["", "<b>" + escape(board["name"]) + "</b>"])
            if not board["cards"]:
                lines.append("<small>No cards in Doing</small>")
            for card in board["cards"]:
                if shown >= 12:
                    break
                title = " ".join(card["name"].split())
                lines.append(escape(textwrap.fill("• " + title, width=64, subsequent_indent="  ")))
                if card.get("dueComplete"):
                    detail = "✓ Complete"
                elif card.get("due"):
                    due = datetime.fromisoformat(card["due"].replace("Z", "+00:00")).astimezone(TIMEZONE)
                    detail = ("Overdue · " if due < now else "Due · ") + due.strftime("%d %b %H:%M")
                else:
                    detail = "No due date"
                lines.append("<small>  " + escape(detail) + "</small>")
                shown += 1
        if total > shown:
            lines.append(f"\n+{total - shown} more · open Trello to view all")
    lines.extend(["", "<small>" + escape(status) + "</small>", "<small>Click to open Trello</small>"])
    return "\n".join(lines)


def main() -> None:
    now = datetime.now(TIMEZONE)
    try:
        snapshot, status = load_cards(now)
        tooltip = cards_tooltip(snapshot, now, status)
    except Exception:
        tooltip = cards_tooltip(None, now, "Trello unavailable · check local settings.")
    print(json.dumps({"text": now.strftime("%a %d %b  %H:%M"), "tooltip": tooltip}, ensure_ascii=False))


if __name__ == "__main__":
    if sys.argv[1:] == ["--setup"]:
        setup()
    else:
        main()
