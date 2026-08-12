import unittest

from scripts.build_human_reference import (
    compile_reference,
    nearest_rank,
    validate_standings,
)


class HumanReferenceTest(unittest.TestCase):
    def test_nearest_rank(self) -> None:
        self.assertEqual(nearest_rank([0, 10, 20, 30], 50), 10)
        self.assertEqual(nearest_rank([0, 10, 20, 30], 90), 30)

    def test_best_per_human_at_each_checkpoint(self) -> None:
        rows = [
            {
                "id": "1",
                "elapsed_seconds": "5",
                "user_id": "A",
                "result": "WA",
                "point": "0",
            },
            {
                "id": "2",
                "elapsed_seconds": "10",
                "user_id": "B",
                "result": "AC",
                "point": "20",
            },
            {
                "id": "3",
                "elapsed_seconds": "20",
                "user_id": "A",
                "result": "AC",
                "point": "10",
            },
            {
                "id": "4",
                "elapsed_seconds": "30",
                "user_id": "B",
                "result": "AC",
                "point": "15",
            },
            {
                "id": "5",
                "elapsed_seconds": "40",
                "user_id": "A",
                "result": "AC",
                "point": "30",
            },
        ]
        result = compile_reference(rows, checkpoints=(15, 35, 45), percentiles=(50,))
        checkpoints = result["checkpoints"]
        self.assertEqual(checkpoints[0]["humans"], 2)
        self.assertEqual(checkpoints[0]["humans_with_score"], 1)
        self.assertEqual(checkpoints[0]["score_percentiles"]["p50"], 0)
        self.assertEqual(checkpoints[1]["score_percentiles"]["p50"], 10)
        self.assertEqual(checkpoints[2]["score_percentiles"]["p50"], 20)
        b = next(user for user in result["trajectories"] if user["user_id"] == "B")
        self.assertEqual(b["improvements"], [[10, 20, 2]])

    def test_standings_validation_can_fail(self) -> None:
        reference = {
            "trajectories": [
                {"improvements": [[10, 10, 1]]},
                {"improvements": [[10, 20, 2]]},
            ]
        }
        validation = validate_standings(
            reference, [10, 30], minimum_match_fraction=0.75
        )
        self.assertEqual(validation["status"], "failed")
        self.assertEqual(validation["matched_public_fraction"], 0.5)


if __name__ == "__main__":
    unittest.main()
