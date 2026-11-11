import subprocess
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
VISUALIZER = PROJECT_ROOT / "python" / "visualizer.py"


class CommunicationIntegrationTests(unittest.TestCase):
    def test_round_trip_traverses_all_seven_layers(self):
        result = subprocess.run(
            [
                sys.executable,
                str(VISUALIZER),
                "--port",
                "0",
                "integration test",
            ],
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=30,
        )

        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        for label in (
            "Client -> server: encapsulation",
            "Client -> server: decapsulation",
            "Server -> client: encapsulation",
            "Server -> client: decapsulation",
        ):
            self.assertIn(label, result.stdout)
        for progress in ("Progress 2/7:", "Progress 4/7:", "Progress 6/7:", "Progress 7/7:"):
            self.assertEqual(result.stdout.count(progress), 4)
        self.assertIn("Message delivered and acknowledged", result.stdout)


if __name__ == "__main__":
    unittest.main()
