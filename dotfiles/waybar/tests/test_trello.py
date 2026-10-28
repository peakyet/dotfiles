"""Contract checks: python -m unittest discover -s dotfiles/waybar/tests."""

import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path
from unittest.mock import patch
from urllib.request import Request

spec = importlib.util.spec_from_file_location("waybar_trello", Path(__file__).parents[1] / "scripts/trello.py")
trello = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trello)


class Clock(datetime):
    current = datetime(2026, 10, 28, 9, 0, tzinfo=trello.TIMEZONE)

    @classmethod
    def now(cls, tz=None):
        return cls.current.astimezone(tz)


class TrelloClockTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        for name, value in (("CREDENTIALS", self.root / "credentials.json"), ("CACHE", self.root / "cards.json"), ("datetime", Clock)):
            patcher = patch.object(trello, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        Clock.current = datetime(2026, 10, 28, 9, 0, tzinfo=trello.TIMEZONE)

    def output(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            trello.main()
        result = json.loads(output.getvalue())
        self.assertEqual(result["text"], Clock.current.strftime("%a %d %b  %H:%M"))
        return result["tooltip"]

    def response(self, request, **kwargs):
        list_id = request.full_url.split("/lists/")[1].split("/")[0]
        cards = []
        if list_id == trello.LISTS[0][1]:
            cards = [
                {"name": "No deadline <&>", "pos": 20, "due": None},
                {"name": "Due next", "pos": 10, "due": "2026-10-28T02:30:00Z"},
                {"name": "Finished but still Doing", "pos": 30, "dueComplete": True},
                {"name": "Archived", "pos": 1, "closed": True},
                {"name": "Moved away", "pos": 2, "idList": "another-list"},
            ]
        return io.BytesIO(json.dumps([{"closed": False, "idList": list_id, **card} for card in cards]).encode())

    def test_doing_cards_and_persistent_offline_cache(self):
        self.assertIn("Setup needed", self.output())
        trello.write_json(trello.CREDENTIALS, {"key": "test-key", "token": "test-token"})
        with patch.object(trello, "urlopen", self.response):
            tooltip = self.output()
        self.assertIn("3 cards", tooltip)
        self.assertIn("10:30", tooltip)  # UTC → Shanghai, not UTC/local machine time.
        self.assertIn("No due date", tooltip)
        self.assertIn("No deadline &lt;&amp;&gt;", tooltip)
        self.assertIn("✓ Complete", tooltip)
        self.assertNotIn("Archived", tooltip)
        self.assertNotIn("Moved away", tooltip)
        self.assertLess(tooltip.index("Due next"), tooltip.index("No deadline"))
        self.assertIn("No cards in Doing", tooltip)
        self.assertEqual(trello.CACHE.stat().st_mode & 0o777, 0o600)
        saved = trello.CACHE.read_bytes()
        with patch.object(trello, "urlopen", side_effect=OSError("offline test-token")):
            self.assertIn("Synced", self.output())  # Fresh cache works offline.
            Clock.current += timedelta(hours=2)
            tooltip = self.output()
        self.assertIn("Sync failed · cached", tooltip)
        self.assertIn("Overdue", tooltip)  # Due status advances even offline.
        self.assertIn("No deadline", tooltip)
        self.assertNotIn("test-token", tooltip)
        self.assertEqual(trello.CACHE.read_bytes(), saved)

    def test_partial_fetch_never_replaces_complete_snapshot(self):
        trello.write_json(trello.CREDENTIALS, {"key": "test-key", "token": "test-token"})
        with patch.object(trello, "urlopen", self.response):
            self.output()
        saved = trello.CACHE.read_bytes()
        Clock.current += timedelta(minutes=6)
        first = Request("https://api.trello.com/1/lists/" + trello.LISTS[0][1] + "/cards")
        responses = [self.response(first), OSError("second board unavailable")]
        with patch.object(trello, "urlopen", side_effect=responses):
            self.assertIn("Sync failed · cached", self.output())
        self.assertEqual(trello.CACHE.read_bytes(), saved)
        trello.CREDENTIALS.unlink()
        self.assertIn("Setup needed", self.output())
        self.assertIn("No deadline", self.output())

    def test_setup_saves_private_credentials_without_echoing_token(self):
        with patch.object(trello, "getpass", side_effect=["test-key", "test-token"]), contextlib.redirect_stdout(io.StringIO()) as output:
            trello.setup()
        self.assertIn("scope=read", output.getvalue())
        self.assertNotIn("test-token", output.getvalue())
        self.assertEqual(json.loads(trello.CREDENTIALS.read_text()), {"key": "test-key", "token": "test-token"})
        self.assertEqual(trello.CREDENTIALS.stat().st_mode & 0o777, 0o600)


if __name__ == "__main__":
    unittest.main()
