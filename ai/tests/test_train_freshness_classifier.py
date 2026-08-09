"""Offline tests for fresh/risk model training."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from train_freshness_classifier import (  # noqa: E402
    FEATURES,
    aggregate_position_predictions,
    apply_training_overrides,
    build_models,
    leave_one_fruit_out,
    verify_logistic_export,
)


def make_training_rows() -> pd.DataFrame:
    rows: list[dict[str, object]] = []
    for fruit_index, sample_id in enumerate(
        ["NECT01", "NECT02", "NECT03", "NECT04"]
    ):
        for position in ("1", "2"):
            is_risk = fruit_index >= 2
            base = 0.30 if is_risk else 0.10
            row: dict[str, object] = {
                "sample_id": sample_id,
                "storage_day": 0,
                "position": position,
                "freshness_state": "warning" if is_risk else "fresh",
                "training_label": "risk" if is_risk else "fresh",
            }
            for feature_index, feature in enumerate(FEATURES):
                row[feature] = base + feature_index * 0.01
            rows.append(row)
    return pd.DataFrame(rows)


class FreshnessClassifierTests(unittest.TestCase):
    def test_override_excludes_unconfirmed_local_positions(self) -> None:
        data = pd.DataFrame(
            [
                {
                    "sample_id": "NECT06",
                    "storage_day": 0,
                    "position": "1",
                    "freshness_state": "spoiled",
                },
                {
                    "sample_id": "NECT06",
                    "storage_day": 0,
                    "position": "3",
                    "freshness_state": "spoiled",
                },
            ]
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "overrides.csv"
            pd.DataFrame(
                [
                    {
                        "sample_id": "NECT06",
                        "storage_day": 0,
                        "position": "1",
                        "action": "exclude",
                        "training_label": "",
                        "reason": "normal skin",
                    },
                    {
                        "sample_id": "NECT06",
                        "storage_day": 0,
                        "position": "3",
                        "action": "include",
                        "training_label": "risk",
                        "reason": "pit center",
                    },
                ]
            ).to_csv(path, index=False, encoding="utf-8-sig")
            result, audit = apply_training_overrides(data, path)

        self.assertEqual(
            int(result.loc[result["position"] == "1", "training_included"].iloc[0]),
            0,
        )
        self.assertEqual(
            result.loc[result["position"] == "3", "training_label"].iloc[0],
            "risk",
        )
        self.assertEqual(len(audit), 2)

    def test_leave_one_fruit_out_never_leaks_held_fruit(self) -> None:
        data = make_training_rows()
        result = leave_one_fruit_out(
            data, build_models()["logistic"], "logistic"
        )
        self.assertEqual(len(result.predictions), len(data))
        self.assertTrue(np.isfinite(result.risk_probabilities).all())
        self.assertEqual({row["held_fruit"] for row in result.folds}, {
            "NECT01",
            "NECT02",
            "NECT03",
            "NECT04",
        })

    def test_float32_logistic_export_reproduces_python(self) -> None:
        data = make_training_rows()
        model = build_models()["logistic"]
        model.fit(data[FEATURES], data["training_label"])
        verify_logistic_export(model, data)

    def test_position_probabilities_are_averaged_by_fruit_day(self) -> None:
        data = make_training_rows().iloc[:4].copy()
        predictions = np.array(["fresh", "risk", "risk", "risk"])
        probabilities = np.array([0.2, 0.6, 0.8, 0.9])
        daily = aggregate_position_predictions(
            data, predictions, probabilities
        )
        first = daily.loc[daily["sample_id"] == "NECT01"].iloc[0]
        second = daily.loc[daily["sample_id"] == "NECT02"].iloc[0]
        self.assertAlmostEqual(first["mean_risk_probability"], 0.4)
        self.assertEqual(first["predicted_label"], "fresh")
        self.assertAlmostEqual(second["mean_risk_probability"], 0.85)
        self.assertEqual(second["predicted_label"], "risk")


if __name__ == "__main__":
    unittest.main()
