"""Offline tests for the four-position live freshness predictor."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from predict_freshness_live import (  # noqa: E402
    combine_position_results,
    summarize_position_frames,
)
from train_freshness_classifier import FEATURES


class FakeModel:
    classes_ = np.array(["fresh", "risk"])
    feature_names_in_ = np.array(FEATURES)

    def predict_proba(self, values):
        probability = float(values.iloc[0]["NIR_clear_norm"])
        return np.array([[1.0 - probability, probability]])


def make_frames(count: int = 20) -> list[dict[str, float]]:
    frames: list[dict[str, float]] = []
    for index in range(count):
        clear = 1000.0 + index
        row = {
            "distance_mm": 20.0,
            "Clear": clear,
            "NIR": clear * 0.2,
        }
        for feature_index, channel in enumerate(
            [
                "F1_415nm",
                "F2_445nm",
                "F3_480nm",
                "F4_515nm",
                "F5_555nm",
                "F6_590nm",
                "F7_630nm",
                "F8_680nm",
            ],
            start=1,
        ):
            row[channel] = clear * feature_index * 0.05
        frames.append(row)
    return frames


class LiveFreshnessPredictorTests(unittest.TestCase):
    def test_position_summary_matches_training_normalization(self) -> None:
        result = summarize_position_frames(
            make_frames(), FakeModel(), position="3"
        )
        self.assertEqual(result["valid_frames"], 20)
        self.assertAlmostEqual(result["F1_415nm_clear_norm"], 0.05)
        self.assertAlmostEqual(result["F8_680nm_clear_norm"], 0.4)
        self.assertAlmostEqual(result["NIR_clear_norm"], 0.2)
        self.assertAlmostEqual(result["risk_probability"], 0.2)

    def test_four_positions_are_combined_with_mean_probability(self) -> None:
        rows = [
            {"position": "1", "risk_probability": 0.4},
            {"position": "2", "risk_probability": 0.6},
            {"position": "3", "risk_probability": 0.8},
            {"position": "4", "risk_probability": 0.2},
        ]
        result = combine_position_results(
            rows, threshold=0.5, local_threshold=0.8
        )
        self.assertAlmostEqual(result["mean_risk_probability"], 0.5)
        self.assertAlmostEqual(result["risk_probability"], 0.8)
        self.assertEqual(result["freshness_state"], "risk")
        self.assertEqual(result["risk_level"], "high")
        self.assertAlmostEqual(result["freshness_confidence"], 0.8)

    def test_high_local_position_is_not_diluted_by_normal_skin(self) -> None:
        rows = [
            {"position": "1", "risk_probability": 0.23},
            {"position": "2", "risk_probability": 0.34},
            {"position": "3", "risk_probability": 0.82},
            {"position": "4", "risk_probability": 0.29},
        ]
        result = combine_position_results(
            rows, threshold=0.5, local_threshold=0.8
        )
        self.assertAlmostEqual(result["mean_risk_probability"], 0.42)
        self.assertEqual(result["freshness_state"], "risk")
        self.assertEqual(result["decision_reason"], "local_anomaly")
        self.assertTrue(result["local_anomaly_detected"])
        self.assertEqual(result["max_risk_position"], "3")
        self.assertAlmostEqual(result["freshness_confidence"], 0.82)


if __name__ == "__main__":
    unittest.main()
