import hashlib
import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from scripts.ahc044_runner import (
    continuation_prompt,
    init_run,
    private_evaluate,
    stop_run,
    submit,
)
from scripts.ars_events import read_events


class Ahc044RunnerTest(unittest.TestCase):
    def test_continuation_prompt_reports_floor_of_time_left(self):
        state = {"started_monotonic": 1000.0, "duration_seconds": 8100}
        with patch("scripts.ahc044_runner.time.monotonic", return_value=1011.1):
            self.assertEqual(
                continuation_prompt(state),
                "Continue working on the task. Time remaining: 8,088 seconds.",
            )

        with patch("scripts.ahc044_runner.time.monotonic", return_value=9200.0):
            self.assertEqual(
                continuation_prompt(state),
                "Continue working on the task. Time remaining: 0 seconds.",
            )

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

    def test_stop_aggregates_resumed_aop_invocations(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "run"
            init_run(
                SimpleNamespace(
                    run_dir=run_dir,
                    run_id="resumed-run",
                    duration=60,
                    suite_id="test",
                    track="single-model",
                    model="test-model",
                    system_manifest_sha256="0" * 64,
                )
            )
            results = []
            for number, values in enumerate(((100, 20, 30, 0.5), (200, 40, 60, 0.75))):
                path = root / f"aop-{number}.json"
                input_tokens, output_tokens, cached_tokens, cost = values
                path.write_text(
                    json.dumps(
                        {
                            "succeeded": True,
                            "usage": {
                                "input_tokens": input_tokens,
                                "output_tokens": output_tokens,
                                "cached_input_tokens": cached_tokens,
                            },
                            "api_equivalent_cost": {"amount_usd": cost},
                        }
                    )
                )
                results.append(path)

            stop_run(SimpleNamespace(run_dir=run_dir, aop_result=results))

            state = json.loads((run_dir / "state.json").read_text())
            self.assertEqual(state["model_calls"], 2)
            self.assertEqual(state["input_tokens"], 300)
            self.assertEqual(state["output_tokens"], 60)
            self.assertEqual(state["cached_tokens"], 90)
            self.assertEqual(state["cost_usd"], 1.25)

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
            output = io.StringIO()
            with redirect_stdout(output):
                with patch("scripts.ahc044_runner.evaluate", return_value=public):
                    submit(
                        SimpleNamespace(
                            run_dir=run_dir,
                            source=source,
                            language="cpp23",
                            workers=1,
                        )
                    )
            public_response = json.loads(output.getvalue())
            self.assertGreaterEqual(public_response["elapsed_seconds"], 0)
            self.assertGreaterEqual(public_response["remaining_seconds"], 0)
            self.assertAlmostEqual(
                public_response["elapsed_seconds"]
                + public_response["remaining_seconds"],
                60,
                places=5,
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
            public_event = next(
                event
                for event in events
                if event["event_type"] == "evaluation.completed"
                and event["payload"]["visibility"] == "public"
            )
            self.assertEqual(
                public_event["payload"]["elapsed_seconds"],
                public_response["elapsed_seconds"],
            )
            self.assertEqual(
                public_event["payload"]["remaining_seconds"],
                public_response["remaining_seconds"],
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
