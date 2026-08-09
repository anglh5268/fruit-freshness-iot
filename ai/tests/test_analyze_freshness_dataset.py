"""新鲜度连续实验整理程序的离线测试。"""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from analyze_freshness_dataset import (  # noqa: E402
    SPECTRAL_CHANNELS,
    assess_model_readiness,
    apply_metadata_corrections,
    apply_selection_overrides,
    build_candidate_record,
    build_daily_summary,
    build_daily_model_samples,
    build_distance_compensation_report,
    build_position_summary,
    select_best_candidates,
)


def make_frames(
    sample_id: str = "NECT01",
    storage_day: int = 0,
    position: str = "1",
    count: int = 20,
    variation: float = 0.0,
    weight_g: float = 100.0,
) -> pd.DataFrame:
    rows: list[dict[str, object]] = []
    for index in range(count):
        multiplier = 1.0 + (variation if index % 2 else -variation)
        row: dict[str, object] = {
            "timestamp": f"2026-07-18T12:00:{index:02d}",
            "experiment_mode": "freshness",
            "sample_id": sample_id,
            "label": "fresh",
            "position": position,
            "fruit_type": "nectarine",
            "storage_day": storage_day,
            "freshness_state": "fresh" if storage_day == 0 else "warning",
            "temperature_c": 25.0,
            "weight_g": weight_g,
            "firmness_score": 4 if storage_day == 0 else 3,
            "surface_note": "无异常",
            "distance_mm": 20,
            "distance_in_range": 1,
            "reflectance_valid": 1,
            "quality_valid": 1,
        }
        for channel_index, channel in enumerate(SPECTRAL_CHANNELS, start=1):
            row[channel] = 100.0 * channel_index * multiplier
        rows.append(row)
    return pd.DataFrame(rows)


class FreshnessDatasetTests(unittest.TestCase):
    def test_selection_prefers_complete_low_cv_retest(self) -> None:
        unstable = build_candidate_record(
            make_frames(variation=0.25), "old.csv", expected_frames=20
        )
        stable = build_candidate_record(
            make_frames(variation=0.001), "new.csv", expected_frames=20
        )
        selected = select_best_candidates(pd.DataFrame([unstable, stable]))
        winner = selected.loc[selected["selected"] == 1].iloc[0]
        self.assertEqual(winner["source_file"], "new.csv")

    def test_selection_prefers_complete_over_stable_incomplete(self) -> None:
        incomplete = build_candidate_record(
            make_frames(count=19), "incomplete.csv", expected_frames=20
        )
        complete = build_candidate_record(
            make_frames(count=20, variation=0.01), "complete.csv", expected_frames=20
        )
        selected = select_best_candidates(pd.DataFrame([incomplete, complete]))
        winner = selected.loc[selected["selected"] == 1].iloc[0]
        self.assertEqual(winner["source_file"], "complete.csv")

    def test_selection_override_can_choose_intentional_targeted_retest(self) -> None:
        old = build_candidate_record(
            make_frames(variation=0.001), "old.csv", expected_frames=20
        )
        targeted = build_candidate_record(
            make_frames(variation=0.01), "targeted.csv", expected_frames=20
        )
        selection = select_best_candidates(pd.DataFrame([old, targeted]))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "selection_overrides.csv"
            pd.DataFrame(
                [
                    {
                        "source_file": "targeted.csv",
                        "reason": "Sensor aimed at the defect center",
                    }
                ]
            ).to_csv(path, index=False, encoding="utf-8-sig")
            selected = apply_selection_overrides(selection, path)

        winner = selected.loc[selected["selected"] == 1].iloc[0]
        self.assertEqual(winner["source_file"], "targeted.csv")
        self.assertEqual(winner["selection_reason"], "selected_manual_override")

    def test_daily_summary_calculates_day0_changes(self) -> None:
        day_zero = make_frames(storage_day=0, weight_g=100.0)
        day_one = make_frames(storage_day=1, weight_g=90.0)
        for channel in SPECTRAL_CHANNELS:
            day_one[channel] = day_one[channel] * 0.8
        summary = build_daily_summary(pd.concat([day_zero, day_one]))
        result = summary.loc[summary["storage_day"] == 1].iloc[0]
        self.assertAlmostEqual(result["weight_loss_percent"], 10.0)
        self.assertAlmostEqual(result["Clear_vs_day0_percent"], -20.0)
        self.assertAlmostEqual(result["firmness_change_from_day0"], -1.0)
        self.assertTrue(np.isfinite(result["F8_680nm_clear_norm"]))
        self.assertEqual(result["collection_date"], "2026-07-18")
        self.assertAlmostEqual(result["elapsed_hours_from_day0"], 0.0)

    def test_position_summary_keeps_time_and_position(self) -> None:
        frames = make_frames(position="3")
        frames["Ambient_Clear"] = 10.0
        frames["Lit_Clear"] = frames["Clear"] + 10.0
        summary = build_position_summary(frames)
        self.assertEqual(summary.iloc[0]["position"], "3")
        self.assertTrue(summary.iloc[0]["collection_started_at"].startswith("2026-07-18"))
        self.assertAlmostEqual(summary.iloc[0]["ambient_clear_mean"], 10.0)

    def test_daily_model_sample_uses_one_row_per_fruit_day(self) -> None:
        frames = []
        for position in ("1", "2", "3", "4"):
            item = make_frames(position=position)
            item["Ambient_Clear"] = 10.0
            item["Lit_Clear"] = item["Clear"] + 10.0
            frames.append(item)
        position_summary = build_position_summary(pd.concat(frames))
        samples = build_daily_model_samples(position_summary)
        self.assertEqual(len(samples), 1)
        self.assertEqual(samples.iloc[0]["sample_group_id"], "NECT01_day0")
        self.assertEqual(samples.iloc[0]["position_count"], 4)

    def test_distance_report_keeps_no_compensation_as_v1(self) -> None:
        frames = []
        for position, distance in zip(("1", "2", "3", "4"), (16, 18, 20, 22)):
            item = make_frames(position=position)
            item["distance_mm"] = distance
            item["Ambient_Clear"] = 10.0
            item["Lit_Clear"] = item["Clear"] + 10.0
            frames.append(item)
        position_summary = build_position_summary(pd.concat(frames))
        report = build_distance_compensation_report(position_summary)
        recommended = report.loc[report["recommended_for_v1"] == 1].iloc[0]
        self.assertEqual(recommended["method"], "none")
        self.assertIn("inverse_square", set(report["method"]))

    def test_model_readiness_requires_two_fruits_in_each_state(self) -> None:
        samples = pd.DataFrame(
            {
                "sample_id": ["NECT01", "NECT02", "NECT03"],
                "freshness_state": ["fresh", "fresh", "warning"],
            }
        )
        ready, message = assess_model_readiness(samples)
        self.assertFalse(ready)
        self.assertIn("warning=1只水果", message)

        samples.loc[len(samples)] = ["NECT04", "warning"]
        ready, _ = assess_model_readiness(samples)
        self.assertTrue(ready)

    def test_spoiled_single_fruit_still_allows_fresh_risk_binary(self) -> None:
        samples = pd.DataFrame(
            {
                "sample_id": ["NECT01", "NECT04", "NECT02", "NECT03", "NECT03"],
                "freshness_state": [
                    "fresh",
                    "fresh",
                    "warning",
                    "warning",
                    "spoiled",
                ],
            }
        )
        ready, message = assess_model_readiness(samples)
        self.assertTrue(ready)
        self.assertIn("fresh/risk二分类可训练", message)
        self.assertIn("三分类暂不可训练", message)

    def test_metadata_correction_is_auditable_and_keeps_source_separate(self) -> None:
        frames = make_frames(storage_day=3)
        frames["selected_source_file"] = "day3.csv"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "corrections.csv"
            pd.DataFrame(
                [
                    {
                        "source_file": "day3.csv",
                        "field_name": "firmness_score",
                        "corrected_value": "2",
                        "reason": "录入错误",
                    },
                    {
                        "source_file": "day3.csv",
                        "field_name": "surface_note",
                        "corrected_value": "表皮变皱",
                        "reason": "录入错误",
                    },
                ]
            ).to_csv(path, index=False, encoding="utf-8-sig")
            corrected, audit = apply_metadata_corrections(frames, path)

        self.assertEqual(set(corrected["firmness_score"]), {2})
        self.assertEqual(set(corrected["surface_note"]), {"表皮变皱"})
        self.assertTrue((corrected["metadata_corrected"] == 1).all())
        self.assertEqual(len(audit), 2)


if __name__ == "__main__":
    unittest.main()
