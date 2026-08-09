"""整理水果新鲜度连续实验数据，并分析相对 Day 0 的变化。"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault(
    "MPLCONFIGDIR",
    str(PROJECT_ROOT / ".cache" / "matplotlib"),
)

import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import MaxNLocator  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402


RAW_DIR = PROJECT_ROOT / "data" / "raw"
PROCESSED_DIR = PROJECT_ROOT / "data" / "processed"
FIGURE_DIR = PROJECT_ROOT / "figures"
DEFAULT_METADATA_CORRECTIONS_PATH = (
    PROJECT_ROOT / "config" / "freshness_metadata_corrections.csv"
)
DEFAULT_SELECTION_OVERRIDES_PATH = (
    PROJECT_ROOT / "config" / "freshness_file_selection_overrides.csv"
)

VISIBLE_CHANNELS = [
    "F1_415nm",
    "F2_445nm",
    "F3_480nm",
    "F4_515nm",
    "F5_555nm",
    "F6_590nm",
    "F7_630nm",
    "F8_680nm",
]
SPECTRAL_CHANNELS = [*VISIBLE_CHANNELS, "Clear", "NIR"]
WAVELENGTHS_NM = np.array([415, 445, 480, 515, 555, 590, 630, 680])
STATE_ORDER = {"fresh": 0, "warning": 1, "spoiled": 2}
CORRECTABLE_METADATA_FIELDS = {
    "freshness_state",
    "temperature_c",
    "weight_g",
    "firmness_score",
    "surface_note",
}

REQUIRED_COLUMNS = {
    "timestamp",
    "experiment_mode",
    "sample_id",
    "position",
    "fruit_type",
    "storage_day",
    "freshness_state",
    "temperature_c",
    "weight_g",
    "firmness_score",
    "surface_note",
    "distance_mm",
    "distance_in_range",
    "reflectance_valid",
    "quality_valid",
    *SPECTRAL_CHANNELS,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="整理水果新鲜度连续实验，并生成质量、Day 0基线和变化趋势报告。"
    )
    parser.add_argument("--raw-dir", default=str(RAW_DIR), help="原始CSV目录")
    parser.add_argument("--fruit-type", default="nectarine", help="水果类型")
    parser.add_argument(
        "--output-prefix",
        default="NECT_FRESHNESS",
        help="输出文件名前缀",
    )
    parser.add_argument(
        "--positions",
        nargs="+",
        default=["1", "2", "3", "4"],
        help="每天期望采集的位置编号",
    )
    parser.add_argument(
        "--frames-per-position",
        type=int,
        default=20,
        help="每个位置要求的合格帧数",
    )
    parser.add_argument(
        "--cv-warning-percent",
        type=float,
        default=5.0,
        help="通道CV中位数超过该值时发出稳定性警告",
    )
    parser.add_argument(
        "--weight-unavailable",
        action="store_true",
        help="重量未实际测量时忽略CSV中的占位值，不计算重量损失",
    )
    parser.add_argument(
        "--metadata-corrections",
        default=str(DEFAULT_METADATA_CORRECTIONS_PATH),
        help="可追溯元数据更正CSV；默认读取config/freshness_metadata_corrections.csv",
    )
    parser.add_argument(
        "--selection-overrides",
        default=str(DEFAULT_SELECTION_OVERRIDES_PATH),
        help="Auditable CSV overrides for intentionally selected retest files",
    )
    return parser.parse_args()


def coefficient_of_variation(data: pd.DataFrame) -> pd.Series:
    means = data[SPECTRAL_CHANNELS].mean()
    stds = data[SPECTRAL_CHANNELS].std(ddof=1).fillna(0.0)
    return (stds / means.replace(0, np.nan) * 100.0).replace(
        [np.inf, -np.inf], np.nan
    )


def validate_single_value(data: pd.DataFrame, column: str) -> Any:
    values = data[column].dropna().unique()
    if len(values) != 1:
        raise ValueError(f"字段 {column} 在同一文件中有 {len(values)} 个不同取值")
    return values[0]


def build_candidate_record(
    data: pd.DataFrame,
    source_file: str,
    expected_frames: int,
) -> dict[str, Any]:
    """为一份原始CSV生成可审计的质量记录。"""
    missing = sorted(REQUIRED_COLUMNS.difference(data.columns))
    if missing:
        raise ValueError(f"缺少字段: {', '.join(missing)}")

    valid = data.loc[pd.to_numeric(data["quality_valid"], errors="coerce") == 1]
    cv = coefficient_of_variation(valid) if not valid.empty else pd.Series(dtype=float)
    distance = pd.to_numeric(valid["distance_mm"], errors="coerce").dropna()
    timestamps = pd.to_datetime(data["timestamp"], errors="coerce").dropna()

    return {
        "source_file": source_file,
        "sample_id": str(validate_single_value(data, "sample_id")),
        "storage_day": int(validate_single_value(data, "storage_day")),
        "position": str(validate_single_value(data, "position")),
        "collection_started_at": timestamps.min().isoformat()
        if not timestamps.empty
        else "",
        "collection_ended_at": timestamps.max().isoformat()
        if not timestamps.empty
        else "",
        "freshness_state": str(validate_single_value(data, "freshness_state")),
        "fruit_type": str(validate_single_value(data, "fruit_type")),
        "raw_frames": int(len(data)),
        "valid_frames": int(len(valid)),
        "quality_rate_percent": float(len(valid) / len(data) * 100.0)
        if len(data)
        else 0.0,
        "complete": int(len(valid) >= expected_frames),
        "distance_mean_mm": float(distance.mean()) if not distance.empty else np.nan,
        "distance_std_mm": float(distance.std(ddof=1))
        if len(distance) > 1
        else 0.0,
        "distance_min_mm": float(distance.min()) if not distance.empty else np.nan,
        "distance_max_mm": float(distance.max()) if not distance.empty else np.nan,
        "median_cv_percent": float(cv.median()) if not cv.empty else np.nan,
        "max_cv_percent": float(cv.max()) if not cv.empty else np.nan,
        "worst_cv_channel": str(cv.idxmax()) if not cv.empty else "",
    }


def select_best_candidates(candidates: pd.DataFrame) -> pd.DataFrame:
    """在同一样品、天数、位置的多次采集中选择最完整且最稳定的一份。"""
    result = candidates.copy()
    result["selected"] = 0
    result["selection_reason"] = "excluded_duplicate"
    group_columns = ["sample_id", "storage_day", "position"]

    for _, group in result.groupby(group_columns, sort=False):
        ranked = group.sort_values(
            ["complete", "median_cv_percent", "valid_frames", "source_file"],
            ascending=[False, True, False, False],
            na_position="last",
        )
        winner = ranked.index[0]
        result.loc[winner, "selected"] = 1
        result.loc[winner, "selection_reason"] = (
            "selected_complete_lowest_cv"
            if result.loc[winner, "complete"]
            else "selected_incomplete_best_available"
        )

    return result.sort_values(
        ["sample_id", "storage_day", "position", "selected", "source_file"],
        ascending=[True, True, True, False, True],
    ).reset_index(drop=True)


def apply_selection_overrides(
    selection: pd.DataFrame,
    overrides_path: Path,
) -> pd.DataFrame:
    """Apply explicit source-file choices after automatic quality ranking."""
    result = selection.copy()
    result["selection_override_reason"] = ""
    if not overrides_path.exists():
        return result

    overrides = pd.read_csv(overrides_path, dtype=str).fillna("")
    required = {"source_file", "reason"}
    missing = sorted(required.difference(overrides.columns))
    if missing:
        raise ValueError(
            f"Selection override CSV is missing columns: {', '.join(missing)}"
        )

    group_columns = ["sample_id", "storage_day", "position"]
    for override in overrides.itertuples(index=False):
        source_file = str(override.source_file).strip()
        source_mask = result["source_file"].astype(str).eq(source_file)
        if int(source_mask.sum()) != 1:
            raise ValueError(
                f"Selection override must match exactly one candidate: {source_file}"
            )

        winner = result.loc[source_mask].iloc[0]
        group_mask = pd.Series(True, index=result.index)
        for column in group_columns:
            group_mask &= result[column].astype(str).eq(str(winner[column]))

        reason = str(override.reason).strip()
        result.loc[group_mask, "selected"] = 0
        result.loc[group_mask, "selection_reason"] = "excluded_by_manual_override"
        result.loc[group_mask, "selection_override_reason"] = reason
        result.loc[source_mask, "selected"] = 1
        result.loc[source_mask, "selection_reason"] = "selected_manual_override"

    return result


def discover_candidates(
    raw_dir: Path,
    fruit_type: str,
    expected_frames: int,
) -> tuple[pd.DataFrame, dict[str, pd.DataFrame], list[str]]:
    records: list[dict[str, Any]] = []
    frames: dict[str, pd.DataFrame] = {}
    skipped: list[str] = []

    for path in sorted(raw_dir.glob("*.csv")):
        try:
            data = pd.read_csv(path)
        except (OSError, UnicodeDecodeError, pd.errors.ParserError) as error:
            skipped.append(f"{path.name}: 无法读取（{error}）")
            continue

        if not REQUIRED_COLUMNS.issubset(data.columns):
            continue
        selected = data.loc[
            (data["experiment_mode"].astype(str) == "freshness")
            & (data["fruit_type"].astype(str) == fruit_type)
        ].copy()
        if selected.empty:
            continue

        try:
            record = build_candidate_record(selected, path.name, expected_frames)
        except (TypeError, ValueError) as error:
            skipped.append(f"{path.name}: {error}")
            continue

        records.append(record)
        frames[path.name] = selected

    if not records:
        raise FileNotFoundError(
            f"{raw_dir} 中没有 fruit_type={fruit_type!r} 的新鲜度实验CSV"
        )
    return pd.DataFrame(records), frames, skipped


def build_curated_frames(
    selection: pd.DataFrame,
    source_frames: dict[str, pd.DataFrame],
    expected_frames: int,
) -> pd.DataFrame:
    curated: list[pd.DataFrame] = []
    selected_rows = selection.loc[selection["selected"] == 1]

    for row in selected_rows.itertuples(index=False):
        data = source_frames[row.source_file]
        valid = data.loc[data["quality_valid"] == 1].copy().head(expected_frames)
        valid["selected_source_file"] = row.source_file
        curated.append(valid)

    if not curated:
        raise ValueError("没有选出可用的新鲜度数据")
    result = pd.concat(curated, ignore_index=True)
    result["storage_day"] = pd.to_numeric(result["storage_day"]).astype(int)
    result["position"] = result["position"].astype(str)
    return result.sort_values(
        ["sample_id", "storage_day", "position", "timestamp"]
    ).reset_index(drop=True)


def apply_metadata_corrections(
    curated: pd.DataFrame,
    corrections_path: Path,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    """通过独立审计表修正误输入元数据，不改写原始CSV。"""
    result = curated.copy()
    result["metadata_corrected"] = 0
    result["metadata_correction_notes"] = ""
    if not corrections_path.exists():
        return result, pd.DataFrame()

    corrections = pd.read_csv(corrections_path, dtype=str).fillna("")
    required = {"source_file", "field_name", "corrected_value", "reason"}
    missing = sorted(required.difference(corrections.columns))
    if missing:
        raise ValueError(
            f"元数据更正表缺少字段: {', '.join(missing)}"
        )

    audit_rows: list[dict[str, Any]] = []
    for correction in corrections.itertuples(index=False):
        field_name = str(correction.field_name).strip()
        if field_name not in CORRECTABLE_METADATA_FIELDS:
            raise ValueError(f"不允许更正字段: {field_name}")
        mask = result["selected_source_file"].astype(str) == str(
            correction.source_file
        ).strip()
        matched_frames = int(mask.sum())
        if matched_frames == 0:
            raise ValueError(
                f"元数据更正没有匹配选用文件: {correction.source_file}"
            )

        old_values = sorted(set(result.loc[mask, field_name].astype(str)))
        raw_value = str(correction.corrected_value).strip()
        if field_name == "firmness_score":
            corrected_value: Any = int(float(raw_value))
        elif field_name in {"temperature_c", "weight_g"}:
            corrected_value = float(raw_value) if raw_value else np.nan
        else:
            corrected_value = raw_value

        result.loc[mask, field_name] = corrected_value
        result.loc[mask, "metadata_corrected"] = 1
        note = (
            f"{field_name}: {' | '.join(old_values)} -> {raw_value}"
            f" ({str(correction.reason).strip()})"
        )
        existing_notes = result.loc[mask, "metadata_correction_notes"].astype(str)
        result.loc[mask, "metadata_correction_notes"] = existing_notes.where(
            existing_notes == "", existing_notes + "; "
        ) + note
        audit_rows.append(
            {
                "source_file": str(correction.source_file).strip(),
                "field_name": field_name,
                "original_value": " | ".join(old_values),
                "corrected_value": raw_value,
                "reason": str(correction.reason).strip(),
                "matched_frames": matched_frames,
            }
        )
    return result, pd.DataFrame(audit_rows)


def build_completeness_report(
    selection: pd.DataFrame,
    positions: list[str],
) -> pd.DataFrame:
    selected = selection.loc[selection["selected"] == 1].copy()
    rows: list[dict[str, Any]] = []

    for sample_id, sample in selected.groupby("sample_id"):
        minimum_day = int(sample["storage_day"].min())
        maximum_day = int(sample["storage_day"].max())
        for day in range(minimum_day, maximum_day + 1):
            for position in positions:
                match = sample.loc[
                    (sample["storage_day"] == day)
                    & (sample["position"].astype(str) == str(position))
                ]
                if match.empty:
                    rows.append(
                        {
                            "sample_id": sample_id,
                            "storage_day": day,
                            "position": str(position),
                            "status": "missing",
                            "valid_frames": 0,
                            "median_cv_percent": np.nan,
                            "selected_source_file": "",
                        }
                    )
                    continue
                item = match.iloc[0]
                rows.append(
                    {
                        "sample_id": sample_id,
                        "storage_day": day,
                        "position": str(position),
                        "status": "complete" if item["complete"] else "incomplete",
                        "valid_frames": int(item["valid_frames"]),
                        "median_cv_percent": item["median_cv_percent"],
                        "selected_source_file": item["source_file"],
                    }
                )
    return pd.DataFrame(rows)


def build_daily_summary(curated: pd.DataFrame) -> pd.DataFrame:
    rows: list[dict[str, Any]] = []
    group_columns = ["sample_id", "storage_day", "freshness_state"]

    for keys, group in curated.groupby(group_columns, sort=True):
        sample_id, storage_day, state = keys
        timestamps = pd.to_datetime(group["timestamp"], errors="coerce").dropna()
        record: dict[str, Any] = {
            "sample_id": sample_id,
            "storage_day": int(storage_day),
            "freshness_state": state,
            "state_code": STATE_ORDER.get(str(state), -1),
            "positions": int(group["position"].nunique()),
            "valid_frames": int(len(group)),
            "collection_started_at": timestamps.min().isoformat()
            if not timestamps.empty
            else "",
            "collection_ended_at": timestamps.max().isoformat()
            if not timestamps.empty
            else "",
            "collection_date": timestamps.min().strftime("%Y-%m-%d")
            if not timestamps.empty
            else "",
            "collection_time_start": timestamps.min().strftime("%H:%M:%S")
            if not timestamps.empty
            else "",
            "collection_time_end": timestamps.max().strftime("%H:%M:%S")
            if not timestamps.empty
            else "",
            "temperature_c": pd.to_numeric(
                group["temperature_c"], errors="coerce"
            ).median(),
            "weight_g": pd.to_numeric(group["weight_g"], errors="coerce").median(),
            "firmness_score": pd.to_numeric(
                group["firmness_score"], errors="coerce"
            ).median(),
            "distance_mean_mm": pd.to_numeric(
                group["distance_mm"], errors="coerce"
            ).mean(),
            "distance_std_mm": pd.to_numeric(
                group["distance_mm"], errors="coerce"
            ).std(ddof=1),
            "surface_note": " | ".join(
                sorted(set(group["surface_note"].dropna().astype(str)))
            ),
        }
        for channel in SPECTRAL_CHANNELS:
            values = pd.to_numeric(group[channel], errors="coerce")
            mean = float(values.mean())
            std = float(values.std(ddof=1))
            record[f"{channel}_mean"] = mean
            record[f"{channel}_spatial_cv_percent"] = (
                std / mean * 100.0 if mean else np.nan
            )
            if channel != "Clear":
                record[f"{channel}_clear_norm"] = mean / float(
                    pd.to_numeric(group["Clear"], errors="coerce").mean()
                )
        rows.append(record)

    summary = pd.DataFrame(rows).sort_values(["sample_id", "storage_day"])
    for sample_id, indexes in summary.groupby("sample_id").groups.items():
        sample = summary.loc[indexes]
        day_zero = sample.loc[sample["storage_day"] == 0]
        if day_zero.empty:
            continue
        baseline = day_zero.iloc[0]
        baseline_time = pd.to_datetime(baseline["collection_started_at"], errors="coerce")
        current_times = pd.to_datetime(
            summary.loc[indexes, "collection_started_at"], errors="coerce"
        )
        summary.loc[indexes, "elapsed_hours_from_day0"] = (
            current_times - baseline_time
        ).dt.total_seconds() / 3600.0
        baseline_weight = baseline["weight_g"]
        if pd.notna(baseline_weight) and baseline_weight != 0:
            summary.loc[indexes, "weight_loss_percent"] = (
                (baseline_weight - summary.loc[indexes, "weight_g"])
                / baseline_weight
                * 100.0
            )
        summary.loc[indexes, "firmness_change_from_day0"] = (
            summary.loc[indexes, "firmness_score"] - baseline["firmness_score"]
        )
        for channel in SPECTRAL_CHANNELS:
            column = f"{channel}_mean"
            baseline_value = baseline[column]
            if pd.notna(baseline_value) and baseline_value != 0:
                summary.loc[indexes, f"{channel}_vs_day0_percent"] = (
                    (summary.loc[indexes, column] - baseline_value)
                    / baseline_value
                    * 100.0
                )
    return summary.reset_index(drop=True)


def build_position_summary(curated: pd.DataFrame) -> pd.DataFrame:
    """按水果、储存日和P位置汇总，保留实际日期时间用于跨天复核。"""
    rows: list[dict[str, Any]] = []
    group_columns = [
        "sample_id",
        "storage_day",
        "freshness_state",
        "position",
    ]
    for keys, group in curated.groupby(group_columns, sort=True):
        sample_id, storage_day, state, position = keys
        timestamps = pd.to_datetime(group["timestamp"], errors="coerce").dropna()
        clear = pd.to_numeric(group["Clear"], errors="coerce").mean()
        record: dict[str, Any] = {
            "sample_id": sample_id,
            "storage_day": int(storage_day),
            "freshness_state": state,
            "position": str(position),
            "collection_started_at": timestamps.min().isoformat()
            if not timestamps.empty
            else "",
            "collection_ended_at": timestamps.max().isoformat()
            if not timestamps.empty
            else "",
            "valid_frames": int(len(group)),
            "distance_mean_mm": pd.to_numeric(
                group["distance_mm"], errors="coerce"
            ).mean(),
            "ambient_clear_mean": pd.to_numeric(
                group.get("Ambient_Clear"), errors="coerce"
            ).mean(),
            "lit_clear_mean": pd.to_numeric(
                group.get("Lit_Clear"), errors="coerce"
            ).mean(),
            "Clear_mean": clear,
        }
        for channel in SPECTRAL_CHANNELS:
            mean = pd.to_numeric(group[channel], errors="coerce").mean()
            record[f"{channel}_mean"] = mean
            if channel != "Clear":
                record[f"{channel}_clear_norm"] = mean / clear if clear else np.nan
        rows.append(record)
    return pd.DataFrame(rows).sort_values(
        ["sample_id", "storage_day", "position"]
    ).reset_index(drop=True)


def build_daily_model_samples(position_summary: pd.DataFrame) -> pd.DataFrame:
    """生成用于定标签和观察趋势的“一个水果×一天一个样本”特征表。"""
    rows: list[dict[str, Any]] = []
    group_columns = ["sample_id", "storage_day", "freshness_state"]
    normalized_channels = [
        f"{channel}_clear_norm" for channel in SPECTRAL_CHANNELS if channel != "Clear"
    ]
    for keys, group in position_summary.groupby(group_columns, sort=True):
        sample_id, storage_day, state = keys
        timestamps = pd.to_datetime(
            group["collection_started_at"], errors="coerce"
        ).dropna()
        clear_values = pd.to_numeric(group["Clear_mean"], errors="coerce")
        record: dict[str, Any] = {
            "sample_group_id": f"{sample_id}_day{int(storage_day)}",
            "sample_id": sample_id,
            "storage_day": int(storage_day),
            "freshness_state": state,
            "collection_started_at": timestamps.min().isoformat()
            if not timestamps.empty
            else "",
            "position_count": int(group["position"].nunique()),
            "distance_median_mm": pd.to_numeric(
                group["distance_mean_mm"], errors="coerce"
            ).median(),
            "corrected_clear_median": clear_values.median(),
            "corrected_clear_position_cv_percent": (
                clear_values.std(ddof=1) / clear_values.mean() * 100.0
                if clear_values.mean()
                else np.nan
            ),
        }
        for column in normalized_channels:
            values = pd.to_numeric(group[column], errors="coerce")
            record[f"{column}_median"] = values.median()
            record[f"{column}_position_iqr"] = values.quantile(0.75) - values.quantile(
                0.25
            )
        rows.append(record)

    result = pd.DataFrame(rows).sort_values(["sample_id", "storage_day"])
    for _, indexes in result.groupby("sample_id").groups.items():
        sample = result.loc[indexes]
        day_zero = sample.loc[sample["storage_day"] == 0]
        if day_zero.empty:
            continue
        baseline_time = pd.to_datetime(
            day_zero.iloc[0]["collection_started_at"], errors="coerce"
        )
        current_times = pd.to_datetime(
            result.loc[indexes, "collection_started_at"], errors="coerce"
        )
        result.loc[indexes, "elapsed_hours_from_day0"] = (
            current_times - baseline_time
        ).dt.total_seconds() / 3600.0
    return result.reset_index(drop=True)


def build_distance_compensation_report(
    position_summary: pd.DataFrame,
    reference_distance_mm: float = 20.0,
) -> pd.DataFrame:
    """比较不补偿、平方反比和数据内拟合指数；只用于评估，不直接改光谱。"""
    usable = position_summary.loc[
        (pd.to_numeric(position_summary["distance_mean_mm"], errors="coerce") > 0)
        & (pd.to_numeric(position_summary["Clear_mean"], errors="coerce") > 0)
    ].copy()
    if usable.empty:
        return pd.DataFrame()

    usable["log_distance"] = np.log(usable["distance_mean_mm"].astype(float))
    usable["log_clear"] = np.log(usable["Clear_mean"].astype(float))
    group_columns = ["sample_id", "storage_day"]
    usable["centered_log_distance"] = usable["log_distance"] - usable.groupby(
        group_columns
    )["log_distance"].transform("mean")
    usable["centered_log_clear"] = usable["log_clear"] - usable.groupby(
        group_columns
    )["log_clear"].transform("mean")
    denominator = float((usable["centered_log_distance"] ** 2).sum())
    fitted_exponent = (
        -float(
            (
                usable["centered_log_distance"] * usable["centered_log_clear"]
            ).sum()
        )
        / denominator
        if denominator
        else 0.0
    )

    methods = [
        ("none", 0.0, "第一版推荐：距离仅做范围门控"),
        ("linear_inverse", 1.0, "对照方法"),
        ("inverse_square", 2.0, "不建议直接假设自由空间平方反比"),
        ("in_sample_fitted", fitted_exponent, "仅供观察，不能替代独立距离标定"),
    ]
    rows: list[dict[str, Any]] = []
    baseline_median_cv = np.nan
    for method, exponent, note in methods:
        corrected = usable["Clear_mean"] * (
            usable["distance_mean_mm"] / reference_distance_mm
        ) ** exponent
        temporary = usable[group_columns].copy()
        temporary["corrected_clear"] = corrected
        grouped = temporary.groupby(group_columns)["corrected_clear"]
        cvs = grouped.std(ddof=1) / grouped.mean() * 100.0
        median_cv = float(cvs.median())
        if method == "none":
            baseline_median_cv = median_cv
        improvement = (
            (baseline_median_cv - median_cv) / baseline_median_cv * 100.0
            if baseline_median_cv
            else np.nan
        )
        rows.append(
            {
                "method": method,
                "exponent": exponent,
                "reference_distance_mm": reference_distance_mm,
                "median_position_cv_percent": median_cv,
                "mean_position_cv_percent": float(cvs.mean()),
                "improvement_vs_none_percent": improvement,
                "recommended_for_v1": int(method == "none"),
                "note": note,
            }
        )
    return pd.DataFrame(rows)


def assess_model_readiness(
    daily_model_samples: pd.DataFrame,
    minimum_fruits_per_state: int = 2,
) -> tuple[bool, str]:
    """分别判断fresh/risk二分类和三分类的最低独立水果条件。"""
    fruit_counts = (
        daily_model_samples.groupby("freshness_state")["sample_id"]
        .nunique()
        .sort_index()
    )
    if len(fruit_counts) < 2:
        return False, "当前只有一个新鲜度类别，先继续观察变化。"

    counts_text = ", ".join(
        f"{state}={int(count)}只水果" for state, count in fruit_counts.items()
    )
    fresh_fruits = int(fruit_counts.get("fresh", 0))
    risk_fruits = int(
        daily_model_samples.loc[
            daily_model_samples["freshness_state"].isin(["warning", "spoiled"]),
            "sample_id",
        ].nunique()
    )
    binary_ready = (
        fresh_fruits >= minimum_fruits_per_state
        and risk_fruits >= minimum_fruits_per_state
    )
    three_class_ready = (
        {"fresh", "warning", "spoiled"}.issubset(fruit_counts.index)
        and all(
            int(fruit_counts.get(state, 0)) >= minimum_fruits_per_state
            for state in ("fresh", "warning", "spoiled")
        )
    )

    if not binary_ready:
        return (
            False,
            f"已有多个类别，但独立水果不足（{counts_text}）；"
            f"fresh和risk各至少需要{minimum_fruits_per_state}只水果。",
        )
    if three_class_ready:
        return True, f"二分类和三分类均具备最低训练条件（{counts_text}）。"
    if "spoiled" in fruit_counts.index:
        return (
            True,
            f"fresh/risk二分类可训练（fresh={fresh_fruits}只，risk={risk_fruits}只）；"
            f"三分类暂不可训练（{counts_text}）。",
        )
    return True, f"具备第一版fresh/warning二分类训练条件（{counts_text}）。"


def save_day0_baseline_plot(summary: pd.DataFrame, path: Path) -> None:
    day_zero = summary.loc[summary["storage_day"] == 0]
    figure, axes = plt.subplots(1, 2, figsize=(13, 5), constrained_layout=True)

    for row in day_zero.itertuples(index=False):
        raw = np.array([getattr(row, f"{channel}_mean") for channel in VISIBLE_CHANNELS])
        normalized = np.array(
            [getattr(row, f"{channel}_clear_norm") for channel in VISIBLE_CHANNELS]
        )
        axes[0].plot(WAVELENGTHS_NM, raw, marker="o", label=row.sample_id)
        axes[1].plot(WAVELENGTHS_NM, normalized, marker="o", label=row.sample_id)

    axes[0].set_title("Day 0 raw reflectance baseline")
    axes[0].set_ylabel("Mean raw count")
    axes[1].set_title("Day 0 Clear-normalized baseline")
    axes[1].set_ylabel("Channel / Clear")
    for axis in axes:
        axis.set_xlabel("Wavelength (nm)")
        axis.grid(alpha=0.25)
        if not day_zero.empty:
            axis.legend()
    figure.suptitle("Nectarine freshness experiment - Day 0")
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
    plt.close(figure)


def save_trajectory_plot(summary: pd.DataFrame, path: Path) -> None:
    figure, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    storage_days = sorted(int(value) for value in summary["storage_day"].unique())
    metrics = [
        ("Clear_vs_day0_percent", "Clear change from Day 0", "%"),
        ("NIR_vs_day0_percent", "NIR change from Day 0", "%"),
        ("F8_680nm_clear_norm", "680 nm / Clear", "ratio"),
        ("weight_loss_percent", "Weight loss from Day 0", "%"),
    ]

    for sample_id, sample in summary.groupby("sample_id"):
        sample = sample.sort_values("storage_day")
        for axis, (column, title, unit) in zip(axes.flat, metrics):
            if column not in sample.columns:
                continue
            axis.plot(
                sample["storage_day"],
                sample[column],
                marker="o",
                label=sample_id,
            )
            axis.set_title(title)
            axis.set_xlabel("Storage day")
            axis.set_ylabel(unit)
            axis.grid(alpha=0.25)

    for axis, (column, title, unit) in zip(axes.flat, metrics):
        axis.set_title(title)
        axis.set_xlabel("Storage day")
        axis.set_ylabel(unit)
        axis.grid(alpha=0.25)
        if column not in summary.columns or not pd.to_numeric(
            summary[column], errors="coerce"
        ).notna().any():
            axis.text(
                0.5,
                0.5,
                "Not measured\nplaceholder values excluded",
                ha="center",
                va="center",
                transform=axis.transAxes,
                color="dimgray",
            )

    for axis in axes.flat:
        if len(storage_days) == 1:
            axis.set_xlim(storage_days[0] - 0.5, storage_days[0] + 0.5)
            axis.set_xticks(storage_days)
        else:
            axis.xaxis.set_major_locator(MaxNLocator(integer=True))
        handles, _ = axis.get_legend_handles_labels()
        if handles:
            axis.legend()
    dates = sorted(value for value in summary["collection_date"].dropna().unique() if value)
    date_text = " to ".join(dates) if dates else "date unavailable"
    figure.suptitle(f"Nectarine freshness trajectories ({date_text})")
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
    plt.close(figure)


def save_position_plot(position_summary: pd.DataFrame, path: Path) -> None:
    """显示每个水果在每一天P1-P4的原始亮度和归一化光谱指标。"""
    sample_ids = list(position_summary["sample_id"].drop_duplicates())
    figure, axes = plt.subplots(
        len(sample_ids),
        2,
        figsize=(13, max(4, 3.6 * len(sample_ids))),
        squeeze=False,
        constrained_layout=True,
    )
    for row_index, sample_id in enumerate(sample_ids):
        sample = position_summary.loc[position_summary["sample_id"] == sample_id]
        for storage_day, day_data in sample.groupby("storage_day"):
            day_data = day_data.sort_values("position", key=lambda s: s.astype(int))
            first_time = pd.to_datetime(
                day_data["collection_started_at"].iloc[0], errors="coerce"
            )
            time_label = (
                first_time.strftime("%m-%d %H:%M")
                if pd.notna(first_time)
                else "time unavailable"
            )
            label = f"Day {int(storage_day)} · {time_label}"
            positions = [f"P{value}" for value in day_data["position"]]
            axes[row_index, 0].plot(
                positions, day_data["Clear_mean"], marker="o", label=label
            )
            axes[row_index, 1].plot(
                positions,
                day_data["F8_680nm_clear_norm"],
                marker="o",
                label=label,
            )
        axes[row_index, 0].set_title(f"{sample_id}: corrected Clear by position")
        axes[row_index, 0].set_ylabel("Lit - ambient (count)")
        axes[row_index, 1].set_title(f"{sample_id}: 680 nm / Clear by position")
        axes[row_index, 1].set_ylabel("ratio")
        for axis in axes[row_index]:
            axis.set_xlabel("Marked fruit position")
            axis.grid(alpha=0.25)
            axis.legend(fontsize=8)
    figure.suptitle("Cross-day comparison by fruit, date/time and P1-P4")
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
    plt.close(figure)


def run(args: argparse.Namespace) -> int:
    raw_dir = Path(args.raw_dir)
    if not raw_dir.is_absolute():
        raw_dir = PROJECT_ROOT / raw_dir
    if args.frames_per_position <= 0:
        raise ValueError("--frames-per-position 必须大于0")

    candidates, source_frames, skipped = discover_candidates(
        raw_dir,
        args.fruit_type,
        args.frames_per_position,
    )
    selection = select_best_candidates(candidates)
    selection_overrides_path = Path(args.selection_overrides)
    if not selection_overrides_path.is_absolute():
        selection_overrides_path = PROJECT_ROOT / selection_overrides_path
    selection = apply_selection_overrides(selection, selection_overrides_path)
    curated = build_curated_frames(
        selection,
        source_frames,
        args.frames_per_position,
    )
    corrections_path = Path(args.metadata_corrections)
    if not corrections_path.is_absolute():
        corrections_path = PROJECT_ROOT / corrections_path
    curated, correction_audit = apply_metadata_corrections(
        curated, corrections_path
    )
    if args.weight_unavailable:
        curated["weight_g"] = np.nan
    completeness = build_completeness_report(selection, args.positions)
    summary = build_daily_summary(curated)
    position_summary = build_position_summary(curated)
    daily_model_samples = build_daily_model_samples(position_summary)
    distance_report = build_distance_compensation_report(position_summary)

    prefix = args.output_prefix.replace("/", "_").replace("\\", "_")
    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    FIGURE_DIR.mkdir(parents=True, exist_ok=True)
    selection_path = PROCESSED_DIR / f"{prefix}_file_selection.csv"
    curated_path = PROCESSED_DIR / f"{prefix}_curated_frames.csv"
    completeness_path = PROCESSED_DIR / f"{prefix}_completeness.csv"
    summary_path = PROCESSED_DIR / f"{prefix}_daily_summary.csv"
    position_summary_path = PROCESSED_DIR / f"{prefix}_position_summary.csv"
    model_samples_path = PROCESSED_DIR / f"{prefix}_daily_model_samples.csv"
    distance_report_path = PROCESSED_DIR / f"{prefix}_distance_compensation.csv"
    correction_audit_path = PROCESSED_DIR / f"{prefix}_metadata_corrections_applied.csv"
    baseline_path = FIGURE_DIR / f"{prefix}_day0_baseline.png"
    trajectory_path = FIGURE_DIR / f"{prefix}_trajectories.png"
    position_path = FIGURE_DIR / f"{prefix}_positions_by_time.png"

    selection.to_csv(selection_path, index=False, encoding="utf-8-sig")
    curated.to_csv(curated_path, index=False, encoding="utf-8-sig")
    completeness.to_csv(completeness_path, index=False, encoding="utf-8-sig")
    summary.to_csv(summary_path, index=False, encoding="utf-8-sig")
    position_summary.to_csv(position_summary_path, index=False, encoding="utf-8-sig")
    daily_model_samples.to_csv(model_samples_path, index=False, encoding="utf-8-sig")
    distance_report.to_csv(distance_report_path, index=False, encoding="utf-8-sig")
    correction_audit.to_csv(correction_audit_path, index=False, encoding="utf-8-sig")
    save_day0_baseline_plot(summary, baseline_path)
    save_trajectory_plot(summary, trajectory_path)
    save_position_plot(position_summary, position_path)

    selected = selection.loc[selection["selected"] == 1]
    missing = completeness.loc[completeness["status"] == "missing"]
    incomplete = completeness.loc[completeness["status"] == "incomplete"]
    unstable = selected.loc[
        selected["median_cv_percent"] > args.cv_warning_percent
    ]
    states = sorted(
        set(curated["freshness_state"].astype(str)),
        key=lambda value: STATE_ORDER.get(value, 99),
    )

    print("=== 油桃新鲜度连续实验检查 ===")
    print(f"候选原始文件: {len(selection)} 个")
    print(f"选用文件: {len(selected)} 个")
    print(f"排除重复/重测文件: {len(selection) - len(selected)} 个")
    print(f"正式有效帧: {len(curated)} 条")
    print(f"独立油桃: {curated['sample_id'].nunique()} 个")
    days = sorted(int(value) for value in curated["storage_day"].unique())
    print(f"储存天数: {days}")
    print(f"已有状态: {', '.join(states)}")
    print(f"已应用元数据更正: {len(correction_audit)} 项")

    if skipped:
        print("\n[跳过文件]")
        for message in skipped:
            print(f"- {message}")
    if not missing.empty:
        print(f"\n[缺少数据] {len(missing)} 个样品-天数-位置组合")
        print(missing[["sample_id", "storage_day", "position"]].to_string(index=False))
    if not incomplete.empty:
        print(f"\n[帧数不足] {len(incomplete)} 组")
    if not unstable.empty:
        print(f"\n[稳定性警告] {len(unstable)} 组CV中位数超过阈值")
        print(
            unstable[
                ["sample_id", "storage_day", "position", "median_cv_percent", "source_file"]
            ].to_string(index=False)
        )
    model_ready, model_message = assess_model_readiness(daily_model_samples)
    status = "可训练" if model_ready else "暂不可训练"
    print(f"\n[模型状态：{status}] {model_message}")

    print("\n输出文件:")
    for path in (
        selection_path,
        curated_path,
        completeness_path,
        summary_path,
        position_summary_path,
        model_samples_path,
        distance_report_path,
        correction_audit_path,
        baseline_path,
        trajectory_path,
        position_path,
    ):
        print(f"- {path}")
    return 0


def main() -> int:
    try:
        return run(parse_args())
    except (FileNotFoundError, ValueError, pd.errors.ParserError) as error:
        print(f"[错误] {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
