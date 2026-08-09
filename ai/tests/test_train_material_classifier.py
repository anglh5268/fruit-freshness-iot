"""Tests for leakage-safe validation group selection."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from train_material_classifier import (  # noqa: E402
    build_models,
    choose_validation_group,
    cross_validate_by_group,
    verify_export,
)


class ValidationGroupingTests(unittest.TestCase):
    def test_auto_uses_position_for_one_specimen_per_class(self) -> None:
        data = pd.DataFrame(
            {
                "label": ["banana", "banana", "peach", "peach"],
                "sample_id": ["B01", "B01", "P01", "P01"],
                "position": [1, 2, 1, 2],
            }
        )
        self.assertEqual(choose_validation_group(data, "auto"), "position")

    def test_auto_uses_specimen_when_each_class_has_two(self) -> None:
        data = pd.DataFrame(
            {
                "label": ["banana", "banana", "peach", "peach"],
                "sample_id": ["B01", "B02", "P01", "P02"],
                "position": [1, 1, 1, 1],
            }
        )
        self.assertEqual(choose_validation_group(data, "auto"), "sample_id")

    def test_leave_one_specimen_out_allows_single_class_test_fold(self) -> None:
        data = pd.DataFrame(
            {
                "label": ["banana"] * 4 + ["peach"] * 4,
                "sample_id": ["B01"] * 2 + ["B02"] * 2 + ["P01"] * 2 + ["P02"] * 2,
                "position": [1, 2] * 4,
                "F1_415nm": [1.0, 1.1, 1.2, 1.3, 9.0, 9.1, 9.2, 9.3],
            }
        )
        result = cross_validate_by_group(
            data=data,
            model=build_models()["logistic"],
            model_name="logistic",
            feature_set="test",
            features=["F1_415nm"],
            group_column="sample_id",
        )
        self.assertEqual(len(result.predictions), len(data))
        self.assertEqual(len(result.fold_rows), 4)
        self.assertEqual(result.validation_group, "sample_id")

    def test_float32_rbf_export_math_matches_python_prediction(self) -> None:
        data = pd.DataFrame(
            {
                "label": ["a"] * 3 + ["b"] * 3 + ["c"] * 3,
                "F1_415nm": [0.0, 0.1, -0.1, 5.0, 5.1, 4.9, 10.0, 10.1, 9.9],
            }
        )
        model = build_models()["svm_rbf"]
        model.fit(data[["F1_415nm"]], data["label"])
        verify_export(model, data, ["F1_415nm"])


if __name__ == "__main__":
    unittest.main()
