"""Guide a four-position scan and predict nectarine fresh/risk probability."""

from __future__ import annotations

import argparse
import csv
import json
import time
from datetime import datetime
from pathlib import Path
from typing import Any

import joblib
import numpy as np
import pandas as pd
import serial

from serial_collect import (
    DEFAULT_CONFIG_PATH,
    DataLineError,
    SPECTRAL_FIELDS,
    build_csv_row,
    list_serial_ports,
    load_config,
    parse_data_line,
    safe_filename_part,
)
from train_freshness_classifier import FEATURES


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MODEL_PATH = (
    PROJECT_ROOT / "models" / "NECT_FRESHNESS_V1_logistic.joblib"
)
VALIDATION_DIR = PROJECT_ROOT / "data" / "validation"


def parse_args() -> argparse.Namespace:
    config = load_config(DEFAULT_CONFIG_PATH)
    measurement = config["measurement"]
    serial_config = config["serial"]
    parser = argparse.ArgumentParser(
        description=(
            "Collect four AS7341 fruit positions, average their risk "
            "probabilities, and save a validation record."
        )
    )
    parser.add_argument("--config", default=str(DEFAULT_CONFIG_PATH))
    parser.add_argument("--model", default=str(DEFAULT_MODEL_PATH))
    parser.add_argument("--sample-id")
    parser.add_argument("--port", default=serial_config["port"])
    parser.add_argument(
        "--baudrate",
        type=int,
        default=int(serial_config["baudrate"]),
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=20,
        help="Valid frames per position; V1 was trained with 20",
    )
    parser.add_argument("--positions", type=int, default=4)
    parser.add_argument(
        "--settle-seconds",
        type=int,
        default=int(measurement.get("settle_seconds", 3)),
    )
    parser.add_argument(
        "--warmup-frames",
        type=int,
        default=int(measurement.get("warmup_frames", 0)),
    )
    parser.add_argument("--position-timeout", type=float, default=60.0)
    parser.add_argument("--threshold", type=float, default=0.5)
    parser.add_argument(
        "--local-threshold",
        type=float,
        default=0.8,
        help="Any single position at or above this value raises a local-risk alert",
    )
    parser.add_argument(
        "--replay-files",
        nargs="+",
        help="Offline mode: use one existing raw CSV per position",
    )
    parser.add_argument("--no-save", action="store_true")
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if args.frames <= 0:
        raise ValueError("--frames must be positive")
    if args.positions <= 0:
        raise ValueError("--positions must be positive")
    if args.settle_seconds < 0 or args.warmup_frames < 0:
        raise ValueError("Settle and warmup values cannot be negative")
    if args.position_timeout <= 0:
        raise ValueError("--position-timeout must be positive")
    if not 0.0 < args.threshold < 1.0:
        raise ValueError("--threshold must be between 0 and 1")
    if not 0.0 < args.local_threshold < 1.0:
        raise ValueError("--local-threshold must be between 0 and 1")
    if args.replay_files and len(args.replay_files) != args.positions:
        raise ValueError(
            "--replay-files count must equal --positions "
            f"({len(args.replay_files)} != {args.positions})"
        )


def load_model(path: Path) -> Any:
    if not path.exists():
        raise FileNotFoundError(
            f"Model not found: {path}. Run src/train_freshness_classifier.py first."
        )
    model = joblib.load(path)
    classes = [str(value) for value in model.classes_]
    if classes != ["fresh", "risk"]:
        raise ValueError(f"Unexpected model classes: {classes}")
    if hasattr(model, "feature_names_in_"):
        actual = [str(value) for value in model.feature_names_in_]
        if actual != FEATURES:
            raise ValueError(
                "Model feature order does not match live predictor: "
                + ", ".join(actual)
            )
    return model


def summarize_position_frames(
    frames: list[dict[str, Any]],
    model: Any,
    position: str,
) -> dict[str, Any]:
    if not frames:
        raise ValueError(f"Position {position} has no valid frames")
    data = pd.DataFrame(frames)
    missing = sorted(set(SPECTRAL_FIELDS).difference(data.columns))
    if missing:
        raise ValueError(
            f"Position {position} is missing fields: {', '.join(missing)}"
        )

    clear_mean = float(pd.to_numeric(data["Clear"], errors="raise").mean())
    if clear_mean <= 0:
        raise ValueError(f"Position {position} has invalid mean Clear")

    feature_values: dict[str, float] = {}
    channel_means: dict[str, float] = {}
    for channel in SPECTRAL_FIELDS:
        channel_means[f"{channel}_mean"] = float(
            pd.to_numeric(data[channel], errors="raise").mean()
        )
    for channel in SPECTRAL_FIELDS:
        if channel == "Clear":
            continue
        feature_values[f"{channel}_clear_norm"] = (
            channel_means[f"{channel}_mean"] / clear_mean
        )

    feature_frame = pd.DataFrame(
        [[feature_values[name] for name in FEATURES]],
        columns=FEATURES,
    )
    risk_index = [str(value) for value in model.classes_].index("risk")
    risk_probability = float(
        model.predict_proba(feature_frame)[0, risk_index]
    )
    distances = pd.to_numeric(data["distance_mm"], errors="coerce")
    return {
        "position": str(position),
        "valid_frames": int(len(data)),
        "distance_mean_mm": float(distances.mean()),
        "distance_std_mm": float(distances.std(ddof=1))
        if len(distances) > 1
        else 0.0,
        **channel_means,
        **feature_values,
        "risk_probability": risk_probability,
        "position_prediction": (
            "risk" if risk_probability >= 0.5 else "fresh"
        ),
    }


def combine_position_results(
    position_results: list[dict[str, Any]],
    threshold: float,
    local_threshold: float = 0.8,
) -> dict[str, Any]:
    if not position_results:
        raise ValueError("No position predictions to combine")
    mean_risk_probability = float(
        np.mean([row["risk_probability"] for row in position_results])
    )
    max_result = max(
        position_results, key=lambda row: float(row["risk_probability"])
    )
    max_risk_probability = float(max_result["risk_probability"])
    mean_risk_detected = mean_risk_probability >= threshold
    local_anomaly_detected = max_risk_probability >= local_threshold
    freshness_state = (
        "risk"
        if mean_risk_detected or local_anomaly_detected
        else "fresh"
    )
    if mean_risk_detected and local_anomaly_detected:
        decision_reason = "mean_and_local_risk"
    elif mean_risk_detected:
        decision_reason = "mean_risk"
    elif local_anomaly_detected:
        decision_reason = "local_anomaly"
    else:
        decision_reason = "fresh"

    decision_score = (
        max(mean_risk_probability, max_risk_probability)
        if freshness_state == "risk"
        else mean_risk_probability
    )
    confidence = (
        decision_score
        if freshness_state == "risk"
        else 1.0 - decision_score
    )
    if freshness_state == "fresh":
        risk_level = "low"
    elif decision_score < 0.7:
        risk_level = "medium"
    else:
        risk_level = "high"
    return {
        "freshness_state": freshness_state,
        "freshness_confidence": confidence,
        "risk_probability": decision_score,
        "mean_risk_probability": mean_risk_probability,
        "max_position_risk_probability": max_risk_probability,
        "max_risk_position": str(max_result["position"]),
        "local_anomaly_detected": bool(local_anomaly_detected),
        "decision_reason": decision_reason,
        "risk_level": risk_level,
        "decision_threshold": float(threshold),
        "local_risk_threshold": float(local_threshold),
        "position_count": int(len(position_results)),
        "decision_protocol": "mean_or_high_local_position",
    }


def replay_position_file(
    path: Path,
    expected_frames: int,
) -> list[dict[str, Any]]:
    if not path.exists():
        raise FileNotFoundError(path)
    data = pd.read_csv(path)
    if "quality_valid" in data.columns:
        quality = pd.to_numeric(data["quality_valid"], errors="coerce")
        data = data.loc[quality == 1].copy()
    missing = sorted(
        {"distance_mm", *SPECTRAL_FIELDS}.difference(data.columns)
    )
    if missing:
        raise ValueError(f"{path.name} is missing fields: {', '.join(missing)}")
    if len(data) < expected_frames:
        raise ValueError(
            f"{path.name} has only {len(data)} valid frames; "
            f"{expected_frames} required"
        )
    return data.iloc[:expected_frames].to_dict(orient="records")


def collect_position_frames(
    serial_port: serial.Serial,
    *,
    position: str,
    target_frames: int,
    warmup_frames: int,
    timeout_seconds: float,
    distance_min_mm: int,
    distance_max_mm: int,
) -> list[dict[str, Any]]:
    serial_port.reset_input_buffer()
    deadline = time.monotonic() + timeout_seconds
    warmed_up = 0
    accepted: list[dict[str, Any]] = []
    rejected = 0

    while (
        warmed_up < warmup_frames or len(accepted) < target_frames
    ) and time.monotonic() < deadline:
        raw_bytes = serial_port.readline()
        if not raw_bytes:
            continue
        line = raw_bytes.decode("utf-8", errors="replace")
        if "DATA," not in line:
            continue
        try:
            parsed = parse_data_line(line)
        except DataLineError:
            continue

        if warmed_up < warmup_frames:
            warmed_up += 1
            continue

        row = build_csv_row(
            parsed=parsed,
            sample_id="LIVE",
            label="unknown",
            position=position,
            distance_min_mm=distance_min_mm,
            distance_max_mm=distance_max_mm,
        )
        if not row["quality_valid"]:
            rejected += 1
            if rejected == 1 or rejected % 5 == 0:
                print(
                    f"  Rejected {rejected}: distance="
                    f"{row['distance_mm']} mm, Clear={row['Clear']}"
                )
            continue

        accepted.append(row)
        print(
            f"  P{position} {len(accepted):02d}/{target_frames:02d}  "
            f"distance={row['distance_mm']:>2} mm  "
            f"Clear={row['Clear']}"
        )

    if len(accepted) < target_frames:
        raise TimeoutError(
            f"P{position} timed out with {len(accepted)}/{target_frames} "
            "valid frames. Keep the fruit at 15-25 mm and retry."
        )
    return accepted


def save_validation_record(
    sample_id: str,
    model_path: Path,
    position_results: list[dict[str, Any]],
    decision: dict[str, Any],
    accepted_frames: list[dict[str, Any]],
) -> tuple[Path, Path, Path]:
    VALIDATION_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    stem = (
        f"{timestamp}_{safe_filename_part(sample_id)}"
        "_freshness_validation"
    )
    positions_path = VALIDATION_DIR / f"{stem}_positions.csv"
    frames_path = VALIDATION_DIR / f"{stem}_frames.csv"
    result_path = VALIDATION_DIR / f"{stem}_result.json"

    pd.DataFrame(position_results).to_csv(
        positions_path, index=False, encoding="utf-8-sig"
    )
    if accepted_frames:
        pd.DataFrame(accepted_frames).to_csv(
            frames_path, index=False, encoding="utf-8-sig"
        )
    else:
        with frames_path.open("w", newline="", encoding="utf-8-sig") as file:
            writer = csv.writer(file)
            writer.writerow(["replay_mode_no_new_frames"])

    payload = {
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "sample_id": sample_id,
        "model": str(model_path),
        **decision,
        "positions": position_results,
    }
    result_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return positions_path, frames_path, result_path


def run(args: argparse.Namespace) -> int:
    validate_args(args)
    config = load_config(Path(args.config))
    measurement = config["measurement"]
    model_path = Path(args.model)
    if not model_path.is_absolute():
        model_path = PROJECT_ROOT / model_path
    model = load_model(model_path)

    sample_id = (args.sample_id or "").strip()
    if not sample_id:
        sample_id = input("请输入本次验证样品编号：").strip()
    if not sample_id:
        raise ValueError("Sample ID cannot be empty")

    position_results: list[dict[str, Any]] = []
    all_frames: list[dict[str, Any]] = []

    if args.replay_files:
        for index, value in enumerate(args.replay_files, start=1):
            frames = replay_position_file(
                Path(value), expected_frames=args.frames
            )
            result = summarize_position_frames(
                frames, model, position=str(index)
            )
            result["source_file"] = str(Path(value))
            position_results.append(result)
            for row in frames:
                row["validation_position"] = str(index)
                row["replay_source_file"] = str(Path(value))
                all_frames.append(row)
    else:
        print("\n=== 油桃四位置实时新鲜度验证 ===")
        print(f"串口: {args.port} @ {args.baudrate}")
        print(
            f"每个位置 {args.frames} 帧，距离范围 "
            f"{measurement['distance_min_mm']}-"
            f"{measurement['distance_max_mm']} mm"
        )
        print("请先关闭 ESP-IDF Monitor 和其他串口助手。\n")
        try:
            with serial.Serial(
                port=args.port,
                baudrate=args.baudrate,
                timeout=float(config["serial"]["timeout_seconds"]),
                rtscts=False,
                dsrdtr=False,
            ) as serial_port:
                serial_port.dtr = False
                serial_port.rts = False
                for index in range(1, args.positions + 1):
                    input(
                        f"\n摆好 P{index}（覆盖水果不同表面；异常处要直接对准），"
                        "按 Enter 开始："
                    )
                    for remaining in range(
                        args.settle_seconds, 0, -1
                    ):
                        print(f"  {remaining}...")
                        time.sleep(1)
                    frames = collect_position_frames(
                        serial_port,
                        position=str(index),
                        target_frames=args.frames,
                        warmup_frames=args.warmup_frames,
                        timeout_seconds=args.position_timeout,
                        distance_min_mm=int(
                            measurement["distance_min_mm"]
                        ),
                        distance_max_mm=int(
                            measurement["distance_max_mm"]
                        ),
                    )
                    result = summarize_position_frames(
                        frames, model, position=str(index)
                    )
                    position_results.append(result)
                    for row in frames:
                        row["validation_position"] = str(index)
                        all_frames.append(row)
                    print(
                        f"  P{index} risk_probability="
                        f"{result['risk_probability']:.3f}"
                    )
        except serial.SerialException as error:
            print(f"\n[串口错误] {error}")
            print("请关闭 ESP-IDF Monitor 和其他串口助手后重试。")
            list_serial_ports()
            return 1

    decision = combine_position_results(
        position_results,
        threshold=args.threshold,
        local_threshold=args.local_threshold,
    )
    print("\n=== 四位置综合结果 ===")
    for result in position_results:
        print(
            f"P{result['position']}: "
            f"risk={result['risk_probability']:.3f}, "
            f"distance={result['distance_mean_mm']:.1f} mm"
        )
    print(
        f"\nfreshness_state={decision['freshness_state']}\n"
        f"freshness_confidence={decision['freshness_confidence']:.3f}\n"
        f"risk_probability={decision['risk_probability']:.3f}\n"
        f"mean_risk_probability={decision['mean_risk_probability']:.3f}\n"
        f"max_position_risk_probability="
        f"{decision['max_position_risk_probability']:.3f} "
        f"(P{decision['max_risk_position']})\n"
        f"local_anomaly_detected="
        f"{str(decision['local_anomaly_detected']).lower()}\n"
        f"decision_reason={decision['decision_reason']}\n"
        f"risk_level={decision['risk_level']}"
    )

    if not args.no_save:
        positions_path, frames_path, result_path = save_validation_record(
            sample_id,
            model_path,
            position_results,
            decision,
            all_frames,
        )
        print("\n验证记录已保存：")
        print(f"- 位置结果: {positions_path}")
        print(f"- 有效帧: {frames_path}")
        print(f"- 综合JSON: {result_path}")
    return 0


def main() -> int:
    return run(parse_args())


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (
        FileNotFoundError,
        TimeoutError,
        ValueError,
        pd.errors.ParserError,
    ) as error:
        print(f"[ERROR] {error}")
        raise SystemExit(1)
