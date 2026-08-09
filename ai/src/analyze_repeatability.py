"""分析固定条件下 AS7341 连续采样的重复性。"""

from __future__ import annotations

import argparse
import os
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault("MPLCONFIGDIR", str(PROJECT_ROOT / ".cache" / "matplotlib"))

import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402


SPECTRAL_CHANNELS = [
    "F1_415nm",
    "F2_445nm",
    "F3_480nm",
    "F4_515nm",
    "F5_555nm",
    "F6_590nm",
    "F7_630nm",
    "F8_680nm",
    "Clear",
    "NIR",
]

VISIBLE_CHANNELS = SPECTRAL_CHANNELS[:8]
WAVELENGTHS_NM = [415, 445, 480, 515, 555, 590, 630, 680]


def find_latest_raw_csv() -> Path:
    raw_dir = PROJECT_ROOT / "data" / "raw"
    files = sorted(raw_dir.glob("*.csv"), key=lambda path: path.stat().st_mtime)
    if not files:
        raise FileNotFoundError(f"{raw_dir} 中没有 CSV 文件")
    return files[-1]


def resolve_input_path(value: str | None) -> Path:
    if value is None:
        return find_latest_raw_csv()

    path = Path(value)
    if not path.is_absolute():
        path = PROJECT_ROOT / path
    return path.resolve()


def validate_columns(data: pd.DataFrame) -> None:
    required = {
        "distance_mm",
        "distance_valid",
        "distance_in_range",
        *SPECTRAL_CHANNELS,
    }
    missing = sorted(required.difference(data.columns))
    if missing:
        raise ValueError(f"CSV 缺少字段: {', '.join(missing)}")


def calculate_summary(data: pd.DataFrame) -> pd.DataFrame:
    records: list[dict[str, float | int | str]] = []

    for channel in SPECTRAL_CHANNELS:
        values = pd.to_numeric(data[channel], errors="coerce").dropna()
        mean = float(values.mean())
        std = float(values.std(ddof=1)) if len(values) > 1 else 0.0
        cv_percent = float(std / mean * 100.0) if mean != 0 else np.nan
        minimum = float(values.min())
        maximum = float(values.max())

        records.append(
            {
                "channel": channel,
                "count": int(len(values)),
                "mean": mean,
                "std": std,
                "cv_percent": cv_percent,
                "min": minimum,
                "max": maximum,
                "range": maximum - minimum,
            }
        )

    return pd.DataFrame.from_records(records)


def evaluate_cv(cv_percent: float) -> str:
    if np.isnan(cv_percent):
        return "invalid"
    if cv_percent <= 2.0:
        return "excellent"
    if cv_percent <= 5.0:
        return "acceptable"
    return "unstable"


def save_figure(data: pd.DataFrame, summary: pd.DataFrame, path: Path) -> None:
    figure, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    frame_number = np.arange(1, len(data) + 1)

    for channel in VISIBLE_CHANNELS:
        axes[0, 0].plot(frame_number, data[channel], linewidth=1.2, label=channel)
    axes[0, 0].set_title("Visible channels over time")
    axes[0, 0].set_xlabel("Frame")
    axes[0, 0].set_ylabel("Raw count")
    axes[0, 0].grid(alpha=0.25)
    axes[0, 0].legend(fontsize=7, ncol=2)

    axes[0, 1].plot(frame_number, data["Clear"], label="Clear", linewidth=1.5)
    axes[0, 1].plot(frame_number, data["NIR"], label="NIR", linewidth=1.5)
    axes[0, 1].set_title("Clear and NIR over time")
    axes[0, 1].set_xlabel("Frame")
    axes[0, 1].set_ylabel("Raw count")
    axes[0, 1].grid(alpha=0.25)
    axes[0, 1].legend()

    visible_means = (
        summary.set_index("channel").loc[VISIBLE_CHANNELS, "mean"].to_numpy()
    )
    axes[1, 0].plot(WAVELENGTHS_NM, visible_means, marker="o", linewidth=2)
    axes[1, 0].set_title("Mean visible spectrum")
    axes[1, 0].set_xlabel("Wavelength (nm)")
    axes[1, 0].set_ylabel("Mean raw count")
    axes[1, 0].grid(alpha=0.25)

    cv_values = summary["cv_percent"].fillna(0.0)
    colors = [
        "#2e7d32" if value <= 2 else "#f9a825" if value <= 5 else "#c62828"
        for value in cv_values
    ]
    axes[1, 1].bar(summary["channel"], cv_values, color=colors)
    axes[1, 1].axhline(2.0, color="#2e7d32", linestyle="--", linewidth=1)
    axes[1, 1].axhline(5.0, color="#c62828", linestyle="--", linewidth=1)
    axes[1, 1].set_title("Coefficient of variation")
    axes[1, 1].set_ylabel("CV (%)")
    axes[1, 1].tick_params(axis="x", rotation=45, labelsize=8)
    axes[1, 1].grid(axis="y", alpha=0.25)

    figure.suptitle(f"AS7341 repeatability - {len(data)} frames", fontsize=14)
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
    plt.close(figure)


def analyze(args: argparse.Namespace) -> int:
    input_path = resolve_input_path(args.input)
    if not input_path.exists():
        print(f"[错误] 找不到输入文件: {input_path}")
        return 1

    data = pd.read_csv(input_path)
    validate_columns(data)
    original_count = len(data)

    if args.valid_distance_only:
        data = data.loc[data["distance_valid"] == 1].copy()
    if args.in_range_only:
        data = data.loc[data["distance_in_range"] == 1].copy()
        if "quality_valid" in data.columns:
            data = data.loc[data["quality_valid"] == 1].copy()

    if args.skip_frames > 0:
        data = data.iloc[args.skip_frames :].copy()

    if data.empty:
        print("[错误] 筛选后没有可分析的数据。")
        return 1

    timestamp = pd.Timestamp.now().strftime("%Y%m%d_%H%M%S")
    stem = input_path.stem
    filter_suffix = "_in_range" if args.in_range_only else (
        "_valid_distance" if args.valid_distance_only else "_all"
    )
    if args.skip_frames > 0:
        filter_suffix += f"_skip{args.skip_frames}"
    summary_path = (
        PROJECT_ROOT
        / "data"
        / "processed"
        / f"{stem}_repeatability{filter_suffix}.csv"
    )
    figure_path = (
        PROJECT_ROOT
        / "figures"
        / f"{stem}_repeatability{filter_suffix}.png"
    )
    summary_path.parent.mkdir(parents=True, exist_ok=True)

    summary = calculate_summary(data)
    summary["assessment"] = summary["cv_percent"].map(evaluate_cv)
    summary.to_csv(summary_path, index=False, encoding="utf-8-sig")
    save_figure(data, summary, figure_path)

    valid_rate = float((data["distance_valid"] == 1).mean() * 100.0)
    in_range_rate = float((data["distance_in_range"] == 1).mean() * 100.0)

    print("=== AS7341 重复性分析 ===")
    print(f"输入文件: {input_path}")
    print(f"原始记录: {original_count} 条")
    print(f"参与分析: {len(data)} 条")
    print(f"距离有效率: {valid_rate:.1f}%")
    print(f"距离范围合格率: {in_range_rate:.1f}%")
    if len(data) < 20:
        print("[提醒] 少于20条，只能验证程序流程，不能作为正式重复性结论。")

    display = summary[["channel", "mean", "std", "cv_percent", "assessment"]].copy()
    print("\n" + display.to_string(index=False, float_format=lambda value: f"{value:.3f}"))
    print(f"\n汇总CSV: {summary_path}")
    print(f"分析图片: {figure_path}")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="分析 AS7341 连续采样的重复性。")
    parser.add_argument(
        "--input",
        help="原始 CSV 路径；省略时自动选择 data/raw 中最新文件",
    )
    parser.add_argument(
        "--valid-distance-only",
        action="store_true",
        help="只分析 distance_valid=1 的记录",
    )
    parser.add_argument(
        "--in-range-only",
        action="store_true",
        help="只分析 distance_in_range=1 的记录",
    )
    parser.add_argument(
        "--skip-frames",
        type=int,
        default=0,
        help="筛选后跳过开头若干帧，用于分析旧数据的稳定段",
    )
    return parser.parse_args()


def main() -> int:
    try:
        return analyze(parse_args())
    except (FileNotFoundError, ValueError, pd.errors.ParserError) as error:
        print(f"[错误] {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
