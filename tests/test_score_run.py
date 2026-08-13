import unittest

from scripts.score_run import percentile_with_ties, replay


def event(sequence, event_type, elapsed, payload):
    return {
        "schema_version": "ars.run-event.v1",
        "run_id": "test-run",
        "sequence": sequence,
        "event_type": event_type,
        "elapsed_seconds": elapsed,
        "recorded_at": "2026-08-13T00:00:00Z",
        "payload": payload,
    }


class ReplayScoreTest(unittest.TestCase):
    def setUp(self):
        self.reference = {
            "problem_id": "test",
            "trajectories": [
                {
                    "first_submission_seconds": 1,
                    "improvements": [[1, 10, 1], [20, 30, 2]],
                },
                {"first_submission_seconds": 1, "improvements": [[1, 20, 3]]},
                {"first_submission_seconds": 1, "improvements": [[1, 20, 4]]},
                {"first_submission_seconds": 1, "improvements": []},
            ],
            "checkpoints": [
                {
                    "elapsed_seconds": 10,
                    "score_percentiles": {"p50": 10, "p90": 20, "p99": 20},
                },
                {
                    "elapsed_seconds": 14400,
                    "score_percentiles": {"p50": 20, "p90": 30, "p99": 30},
                },
            ],
        }

    def test_ties_report_interval_and_midrank(self):
        self.assertEqual(
            percentile_with_ties([0, 10, 20, 20], 20),
            {"lower": 50.0, "midrank": 75.0, "upper": 100.0},
        )

    def test_checkpoints_thresholds_and_censoring(self):
        events = [
            event(0, "run.started", 0, {}),
            event(1, "candidate.submitted", 5, {"candidate_id": "a"}),
            event(2, "candidate.submitted", 20, {"candidate_id": "b"}),
            event(3, "run.stopped", 30, {"reason": "completed"}),
            event(
                4,
                "evaluation.completed",
                40,
                {"candidate_id": "a", "visibility": "private-post-run", "score": 20},
            ),
            event(
                5,
                "evaluation.completed",
                50,
                {"candidate_id": "b", "visibility": "private-post-run", "score": 25},
            ),
        ]
        result = replay(events, self.reference)
        first = result["checkpoints"][0]
        self.assertEqual(first["agent_score"], 20)
        self.assertEqual(first["human_percentile"]["midrank"], 75.0)
        thresholds = {item["target"]: item for item in result["time_to_human_4h"]}
        self.assertEqual(thresholds["p50"]["elapsed_seconds"], 5)
        self.assertEqual(thresholds["p90"]["status"], "right-censored")
        self.assertEqual(thresholds["p90"]["censor_seconds"], 14400)

    def test_threshold_tie_counts_as_reached(self):
        events = [
            event(0, "run.started", 0, {}),
            event(1, "candidate.submitted", 7, {"candidate_id": "a"}),
            event(2, "run.stopped", 8, {"reason": "completed"}),
            event(
                3,
                "evaluation.completed",
                9,
                {"candidate_id": "a", "visibility": "private-post-run", "score": 30},
            ),
        ]
        result = replay(events, self.reference)
        thresholds = {item["target"]: item for item in result["time_to_human_4h"]}
        self.assertEqual(thresholds["p90"]["status"], "reached")
        self.assertEqual(thresholds["p99"]["elapsed_seconds"], 7)


if __name__ == "__main__":
    unittest.main()
