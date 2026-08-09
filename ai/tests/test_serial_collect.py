"""serial_collect.py 的离线解析测试。"""

from __future__ import annotations

import sys
import unittest
from argparse import Namespace
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from serial_collect import (  # noqa: E402
    DataLineError,
    build_csv_row,
    build_output_path,
    parse_data_line,
    resolve_experiment_metadata,
    strip_ansi,
)


VALID_LINE = (
    "DATA,sample=1726,distance_mm=59,"
    "F1_415nm=32,F2_445nm=35,F3_480nm=23,F4_515nm=151,"
    "F5_555nm=56,F6_590nm=267,F7_630nm=408,F8_680nm=105,"
    "Clear=496,NIR=15"
)


class SerialParserTests(unittest.TestCase):
    def test_freshness_output_path_includes_storage_day_and_position(self) -> None:
        path = build_output_path(
            Path("data/raw"), "NECT01", "fresh", "3", storage_day=2
        )
        self.assertRegex(path.name, r"^\d{8}_\d{6}_NECT01_day2_fresh_pos3\.csv$")

    def test_valid_line(self) -> None:
        parsed = parse_data_line(VALID_LINE)
        self.assertEqual(parsed["sample"], 1726)
        self.assertEqual(parsed["distance_mm"], 59)
        self.assertEqual(parsed["F7_630nm"], 408)
        self.assertEqual(parsed["NIR"], 15)

    def test_ansi_codes_are_removed(self) -> None:
        line = "\x1b[0;33mW warning\x1b[0m " + VALID_LINE
        parsed = parse_data_line(line)
        self.assertEqual(parsed["Clear"], 496)
        self.assertNotIn("\x1b", strip_ansi(line))

    def test_reflectance_raw_fields_are_preserved(self) -> None:
        line = VALID_LINE + ",Ambient_Clear=100,Lit_Clear=596"
        parsed = parse_data_line(line)
        self.assertEqual(parsed["Ambient_Clear"], 100)
        self.assertEqual(parsed["Lit_Clear"], 596)
        self.assertEqual(parsed["Clear"], 496)

    def test_incomplete_line_is_rejected(self) -> None:
        with self.assertRaises(DataLineError):
            parse_data_line("DATA,sample=1,distance_mm=59,F1_415nm=32")

    def test_non_data_line_is_rejected(self) -> None:
        with self.assertRaises(DataLineError):
            parse_data_line("I (100) APP: Project starting")

    def test_quality_gate_accepts_good_reflectance(self) -> None:
        parsed = parse_data_line(
            VALID_LINE + ",Ambient_Clear=100,Lit_Clear=596"
        )
        row = build_csv_row(parsed, "W01", "paper", "1", 15, 25)
        self.assertEqual(row["distance_in_range"], 0)
        self.assertEqual(row["reflectance_valid"], 1)
        self.assertEqual(row["quality_valid"], 0)

        parsed["distance_mm"] = 20
        row = build_csv_row(parsed, "W01", "paper", "1", 15, 25)
        self.assertEqual(row["quality_valid"], 1)

    def test_quality_gate_rejects_no_fill_light_increase(self) -> None:
        parsed = parse_data_line(
            VALID_LINE + ",Ambient_Clear=600,Lit_Clear=596"
        )
        parsed["distance_mm"] = 20
        row = build_csv_row(parsed, "W01", "paper", "1", 15, 25)
        self.assertEqual(row["reflectance_valid"], 0)
        self.assertEqual(row["quality_valid"], 0)

    def test_freshness_metadata_is_written_to_csv_row(self) -> None:
        parsed = parse_data_line(VALID_LINE)
        parsed["distance_mm"] = 20
        row = build_csv_row(
            parsed,
            "NECT01",
            "fresh",
            "1",
            15,
            25,
            experiment_mode="freshness",
            fruit_type="nectarine",
            storage_day=0,
            freshness_state="fresh",
            temperature_c=25.5,
            weight_g=148.2,
            firmness_score=5,
            surface_note="表面完整",
        )
        self.assertEqual(row["experiment_mode"], "freshness")
        self.assertEqual(row["fruit_type"], "nectarine")
        self.assertEqual(row["storage_day"], 0)
        self.assertEqual(row["freshness_state"], "fresh")
        self.assertEqual(row["surface_note"], "表面完整")

    def test_freshness_mode_requires_day_and_fruit_type(self) -> None:
        args = Namespace(
            freshness_state="fresh",
            fruit_type=None,
            storage_day=None,
            label=None,
            temperature_c=None,
            weight_g=None,
            firmness_score=None,
            surface_note=None,
        )
        with self.assertRaises(ValueError):
            resolve_experiment_metadata(args)

    def test_freshness_metadata_validation(self) -> None:
        args = Namespace(
            freshness_state="warning",
            fruit_type="nectarine",
            storage_day=3,
            label=None,
            temperature_c=24.0,
            weight_g=142.5,
            firmness_score=3,
            surface_note="轻微软化",
        )
        metadata = resolve_experiment_metadata(args)
        self.assertEqual(metadata["experiment_mode"], "freshness")
        self.assertEqual(metadata["freshness_state"], "warning")
        self.assertEqual(metadata["firmness_score"], 3)


if __name__ == "__main__":
    unittest.main()
