import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from scripts.ahc044_runner import init_run, private_evaluate, stop_run, submit
from scripts.ars_events import read_events


class Ahc044RunnerTest(unittest.TestCase):
    def test_timeout_is_a_normal_deadline_stop(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "run"
            init_run(
                SimpleNamespace(
                    run_dir=run_dir,
                    run_id="deadline-run",
                    duration=60,
                    suite_id="test",
                    track="single-model",
                    model="test-model",
                    system_manifest_sha256="0" * 64,
                )
            )
            aop_result = root / "aop.json"
            aop_result.write_text(json.dumps({"succeeded": False, "timed_out": True}))
            stop_run(SimpleNamespace(run_dir=run_dir, aop_result=aop_result))

            events = read_events(run_dir / "events.jsonl")
            model_stop = next(
                event
                for event in events
                if event["event_type"] == "model_call.completed"
            )
            run_stop = next(
                event for event in events if event["event_type"] == "run.stopped"
            )
            self.assertEqual(model_stop["payload"]["status"], "cancelled")
            self.assertEqual(run_stop["payload"]["reason"], "deadline")

    def test_end_to_end_event_recording(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "run"
            source = root / "solution.cpp"
            source.write_text("int main() {}\n")
            init_run(
                SimpleNamespace(
                    run_dir=run_dir,
                    run_id="test-run",
                    duration=60,
                    suite_id="test",
                    track="single-model",
                    model="test-model",
                    system_manifest_sha256="0" * 64,
                )
            )
            public = {
                "judge_result": "ACCEPTED",
                "score": 50,
                "wall_seconds": 1.0,
                "cpu_seconds": 0.5,
                "rank": None,
                "performance": None,
                "score_basis": "test public",
            }
            with patch("scripts.ahc044_runner.evaluate", return_value=public):
                submit(
                    SimpleNamespace(
                        run_dir=run_dir, source=source, language="cpp23", workers=1
                    )
                )

            aop_result = root / "aop.json"
            aop_result.write_text(
                json.dumps(
                    {
                        "succeeded": True,
                        "usage": {
                            "input_tokens": 100,
                            "output_tokens": 20,
                            "cached_input_tokens": 30,
                        },
                        "api_equivalent_cost": {"amount_usd": 0.5},
                    }
                )
            )
            stop_run(SimpleNamespace(run_dir=run_dir, aop_result=aop_result))
            private = {
                **public,
                "score": 150,
                "rank": 10,
                "performance": 2000,
                "score_basis": "test private",
            }
            with patch("scripts.ahc044_runner.evaluate", return_value=private):
                private_evaluate(
                    SimpleNamespace(run_dir=run_dir, language="cpp23", workers=1)
                )

            events = read_events(run_dir / "events.jsonl")
            self.assertEqual(
                [event["sequence"] for event in events], list(range(len(events)))
            )
            submitted = next(
                event
                for event in events
                if event["event_type"] == "candidate.submitted"
            )
            self.assertEqual(
                submitted["payload"]["source_sha256"],
                hashlib.sha256(source.read_bytes()).hexdigest(),
            )
            private_event = next(
                event
                for event in events
                if event["event_type"] == "evaluation.completed"
                and event["payload"]["visibility"] == "private-post-run"
            )
            self.assertEqual(private_event["payload"]["score"], 150)
            self.assertEqual(private_event["payload"]["rank"], 10)


if __name__ == "__main__":
    unittest.main()
