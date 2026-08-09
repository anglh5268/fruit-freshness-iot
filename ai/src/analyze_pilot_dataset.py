"""合并第一轮多材料试采数据，并生成质量报告与光谱对比图。"""

from __future__ import annotations

import argparse
import os
from pathlib import Path

os.environ.setdefault(
    "MPLCONFIGDIR",
    str(Path(__file__).resolve().parents[1] / ".cache" / "matplotlib"),
)

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RAW_DIR = PROJECT_ROOT / "data" / "raw"
PROCESSED_DIR = PROJECT_ROOT / "data" / "processed"
FIGURE_DIR = PROJECT_ROOT / "figures"

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
FEATURE_CHANNELS = [*VISIBLE_CHANNELS, "NIR"]
WAVELENGTHS = np.array([415, 445, 480, 515, 555, 590, 630, 680])
REQUIRED_COLUMNS = {
    "sample_id",
    "label",
    "position",
    "distance_mm",
    "distance_valid",
    "distance_in_range",
    "Clear",
    *FEATURE_CHANNELS,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="分析三种材料的第一轮试采数据")
    parser.add_argument(
        "--sample-prefix",
        default="P1",
        help="只读取样品编号以该文字开头的数据，默认 P1",
    )
    parser.add_argument(
        "--all-samples",
        action="store_true",
        help="不按样品编号前缀筛选；仍会按 --labels 筛选类别",
    )
    parser.add_argument(
        "--output-prefix",
        help="输出文件前缀；省略时使用 --sample-prefix",
    )
    parser.add_argument(
        "--labels",
        nargs="+",
        default=["white_card", "green_tea", "black_tea"],
        help="期望出现的材料标签",
    )
    parser.add_argument(
        "--include-out-of-range",
        action="store_true",
        help="保留距离范围外的数据（默认删除）",
    )
    return parser.parse_args()


def load_matching_files(
    prefix: str | None,
    labels: list[str],
) -> tuple[pd.DataFrame, list[Path]]:
    frames: list[pd.DataFrame] = []
    matched_files: list[Path] = []

    for path in sorted(RAW_DIR.glob("*.csv")):
        try:
            frame = pd.read_csv(path)
        except (OSError, pd.errors.ParserError, UnicodeDecodeError) as exc:
            print(f"[跳过] 无法读取 {path.name}: {exc}")
            continue

        missing = REQUIRED_COLUMNS.difference(frame.columns)
        if missing:
            continue

        selected = frame.loc[frame["label"].astype(str).isin(labels)].copy()
        if prefix is not None:
            selected = selected.loc[
                selected["sample_id"].astype(str).str.startswith(prefix)
            ].copy()
        if selected.empty:
            continue

        selected["source_file"] = path.name
        frames.append(selected)
        matched_files.append(path)

    if not frames:
        raise FileNotFoundError(
            "data/raw 中没有找到符合样品编号和类别条件的数据。"
        )
    return pd.concat(frames, ignore_index=True), matched_files


def build_quality_report(all_data: pd.DataFrame) -> pd.DataFrame:
    group_columns = ["label", "sample_id", "position", "source_file"]
    rows: list[dict[str, object]] = []
    for keys, group in all_data.groupby(group_columns, dropna=False):
        label, sample_id, position, source_file = keys
        rows.append(
            {
                "label": label,
                "sample_id": sample_id,
                "position": position,
                "source_file": source_file,
                "frames": len(group),
                "valid_distance_frames": int(group["distance_valid"].sum()),
                "in_range_frames": int(group["distance_in_range"].sum()),
                "distance_mean_mm": group.loc[
                    group["distance_valid"] == 1, "distance_mm"
                ].mean(),
                "distance_std_mm": group.loc[
                    group["distance_valid"] == 1, "distance_mm"
                ].std(),
            }
        )
    return pd.DataFrame(rows).sort_values(["label", "sample_id", "position"])


def add_normalized_features(data: pd.DataFrame) -> pd.DataFrame:
    result = data.copy()
    result = result.loc[result["Clear"] > 0].copy()
    for channel in FEATURE_CHANNELS:
        result[f"{channel}_norm"] = result[channel] / result["Clear"]
    return result


def save_spectrum_plot(
    data: pd.DataFrame,
    path: Path,
    dataset_name: str,
) -> None:
    position_means = data.groupby(
        ["label", "sample_id", "position"], as_index=False
    ).mean(numeric_only=True)

    figure, axes = plt.subplots(1, 2, figsize=(13, 5))
    for label, group in position_means.groupby("label"):
        raw_mean = group[VISIBLE_CHANNELS].mean().to_numpy()
        normalized_columns = [f"{name}_norm" for name in VISIBLE_CHANNELS]
        norm_mean = group[normalized_columns].mean().to_numpy()
        norm_std = group[normalized_columns].std().fillna(0).to_numpy()

        axes[0].plot(WAVELENGTHS, raw_mean, marker="o", label=label)
        axes[1].plot(WAVELENGTHS, norm_mean, marker="o", label=label)
        axes[1].fill_between(
            WAVELENGTHS,
            norm_mean - norm_std,
            norm_mean + norm_std,
            alpha=0.15,
        )

    axes[0].set_title("Mean raw spectrum by class")
    axes[0].set_ylabel("Raw count")
    axes[1].set_title("Clear-normalized spectrum by class")
    axes[1].set_ylabel("Channel / Clear")
    for axis in axes:
        axis.set_xlabel("Wavelength (nm)")
        axis.grid(alpha=0.25)
        axis.legend()

    figure.suptitle(f"{dataset_name} class comparison")
    figure.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
    plt.close(figure)


def main() -> int:
    args = parse_args()
    sample_prefix = None if args.all_samples else args.sample_prefix
    try:
        all_data, matched_files = load_matching_files(sample_prefix, args.labels)
    except FileNotFoundError as exc:
        print(f"[等待采集] {exc}")
        return 1

    quality = build_quality_report(all_data)
    data = all_data.copy()
    if not args.include_out_of_range:
        data = data.loc[data["distance_in_range"] == 1].copy()
        if "quality_valid" in data.columns:
            data = data.loc[data["quality_valid"] == 1].copy()
    data = add_normalized_features(data)

    if data.empty:
        print("[错误] 筛选后没有可用数据。")
        return 1

    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    output_prefix = args.output_prefix or args.sample_prefix
    prefix = output_prefix.replace("/", "_").replace("\\", "_")
    combined_path = PROCESSED_DIR / f"{prefix}_pilot_combined.csv"
    quality_path = PROCESSED_DIR / f"{prefix}_pilot_quality.csv"
    figure_path = FIGURE_DIR / f"{prefix}_pilot_spectra.png"

    data.to_csv(combined_path, index=False, encoding="utf-8-sig")
    quality.to_csv(quality_path, index=False, encoding="utf-8-sig")
    save_spectrum_plot(data, figure_path, prefix)

    print("=== 第一轮材料试采检查 ===")
    print(f"读取文件: {len(matched_files)} 个")
    print(f"原始帧数: {len(all_data)}")
    print(f"参与分析: {len(data)}")
    print("\n各类别有效帧数:")
    print(data.groupby("label").size().to_string())
    print("\n各类别独立位置数:")
    print(data.groupby("label")["position"].nunique().to_string())

    present_labels = set(data["label"].astype(str))
    missing_labels = [label for label in args.labels if label not in present_labels]
    if missing_labels:
        print(f"\n[尚未完成] 缺少类别: {', '.join(missing_labels)}")

    position_counts = data.groupby("label")["position"].nunique()
    insufficient = position_counts[position_counts < 3]
    if not insufficient.empty:
        details = ", ".join(f"{label}={count}" for label, count in insufficient.items())
        print(f"[尚未完成] 以下类别不足3个位置: {details}")

    print(f"\n合并数据: {combined_path}")
    print(f"质量报告: {quality_path}")
    print(f"光谱对比图: {figure_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
