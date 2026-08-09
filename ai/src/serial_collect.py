"""从 ESP32-S3 串口采集 AS7341 与 VL53L0X 数据并保存为 CSV。"""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Any

import serial
from serial.tools import list_ports


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG_PATH = PROJECT_ROOT / "config" / "experiment.json"

ANSI_ESCAPE_RE = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")
INTEGER_PREFIX_RE = re.compile(r"^[+-]?\d+")

SPECTRAL_FIELDS = [
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

EXPECTED_FIELDS = [
    "sample",
    "distance_mm",
    *SPECTRAL_FIELDS,
]

RAW_LIGHT_FIELDS = [
    *[f"Ambient_{field}" for field in SPECTRAL_FIELDS],
    *[f"Lit_{field}" for field in SPECTRAL_FIELDS],
]

PARSED_FIELDS = [*EXPECTED_FIELDS, *RAW_LIGHT_FIELDS]

CSV_FIELDS = [
    "timestamp",
    "experiment_mode",
    "sample_id",
    "label",
    "position",
    "fruit_type",
    "storage_day",
    "freshness_state",
    "temperature_c",
    "weight_g",
    "firmness_score",
    "surface_note",
    "esp_sample",
    "distance_mm",
    "distance_valid",
    "distance_in_range",
    "reflectance_valid",
    "quality_valid",
    *SPECTRAL_FIELDS,
    *RAW_LIGHT_FIELDS,
]


class DataLineError(ValueError):
    """表示串口行不是一条完整有效的 DATA 记录。"""


FRESHNESS_STATES = ("fresh", "warning", "spoiled")


def load_config(path: Path = DEFAULT_CONFIG_PATH) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def strip_ansi(text: str) -> str:
    """去掉 ESP-IDF 日志中的颜色控制字符。"""
    return ANSI_ESCAPE_RE.sub("", text)


def parse_data_line(line: str) -> dict[str, int]:
    """解析 DATA,sample=...,distance_mm=... 格式，返回整数数据。"""
    clean_line = strip_ansi(line).strip()
    marker_index = clean_line.find("DATA,")
    if marker_index < 0:
        raise DataLineError("不是 DATA 数据行")

    payload = clean_line[marker_index + len("DATA,") :]
    parsed: dict[str, int] = {}

    for item in payload.split(","):
        if "=" not in item:
            continue

        key, raw_value = item.split("=", maxsplit=1)
        key = key.strip()
        if key not in PARSED_FIELDS:
            continue

        match = INTEGER_PREFIX_RE.match(raw_value.strip())
        if match is None:
            raise DataLineError(f"字段 {key} 不是整数: {raw_value!r}")

        parsed[key] = int(match.group())

    missing = [field for field in EXPECTED_FIELDS if field not in parsed]
    if missing:
        raise DataLineError(f"缺少字段: {', '.join(missing)}")

    return parsed


def list_serial_ports() -> list[str]:
    ports = list(list_ports.comports())
    if not ports:
        print("没有发现串口。请检查 USB_UART 数据线、开发板电源和 CH340 驱动。")
        return []

    print("当前检测到的串口：")
    for port in ports:
        print(f"  {port.device:<8} {port.description}")
    return [port.device for port in ports]


def safe_filename_part(value: str) -> str:
    cleaned = re.sub(r"[^\w.-]+", "_", value.strip(), flags=re.UNICODE)
    return cleaned.strip("._") or "unknown"


def prompt_if_missing(value: str | None, prompt: str) -> str:
    if value is not None and value.strip():
        return value.strip()

    while True:
        entered = input(prompt).strip()
        if entered:
            return entered
        print("该项不能为空，请重新输入。")


def build_output_path(
    raw_data_dir: Path,
    sample_id: str,
    label: str,
    position: str,
    storage_day: int | str = "",
) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    parts = [timestamp, safe_filename_part(sample_id)]
    if storage_day != "":
        parts.append(f"day{safe_filename_part(str(storage_day))}")
    parts.extend(
        [
            safe_filename_part(label),
            f"pos{safe_filename_part(position)}",
        ]
    )
    filename = "_".join(parts)
    return raw_data_dir / f"{filename}.csv"


def build_csv_row(
    parsed: dict[str, int],
    sample_id: str,
    label: str,
    position: str,
    distance_min_mm: int,
    distance_max_mm: int,
    *,
    experiment_mode: str = "generic",
    fruit_type: str = "",
    storage_day: int | str = "",
    freshness_state: str = "",
    temperature_c: float | str = "",
    weight_g: float | str = "",
    firmness_score: int | str = "",
    surface_note: str = "",
) -> dict[str, Any]:
    distance_mm = parsed["distance_mm"]
    distance_valid = distance_mm >= 0
    distance_in_range = (
        distance_valid
        and distance_min_mm <= distance_mm <= distance_max_mm
    )

    has_raw_light = all(
        field in parsed
        for field in ("Ambient_Clear", "Lit_Clear")
    )
    reflectance_valid = (
        parsed["Clear"] > 0
        and (
            not has_raw_light
            or parsed["Lit_Clear"] > parsed["Ambient_Clear"]
        )
    )
    quality_valid = distance_in_range and reflectance_valid

    return {
        "timestamp": datetime.now().isoformat(timespec="milliseconds"),
        "experiment_mode": experiment_mode,
        "sample_id": sample_id,
        "label": label,
        "position": position,
        "fruit_type": fruit_type,
        "storage_day": storage_day,
        "freshness_state": freshness_state,
        "temperature_c": temperature_c,
        "weight_g": weight_g,
        "firmness_score": firmness_score,
        "surface_note": surface_note,
        "esp_sample": parsed["sample"],
        "distance_mm": distance_mm,
        "distance_valid": int(distance_valid),
        "distance_in_range": int(distance_in_range),
        "reflectance_valid": int(reflectance_valid),
        "quality_valid": int(quality_valid),
        **{field: parsed[field] for field in SPECTRAL_FIELDS},
        **{field: parsed.get(field, "") for field in RAW_LIGHT_FIELDS},
    }


def resolve_experiment_metadata(args: argparse.Namespace) -> dict[str, Any]:
    """校验并整理普通实验或水果新鲜度实验的元数据。"""
    freshness_state = args.freshness_state or ""
    is_freshness = bool(freshness_state)

    if not is_freshness:
        return {
            "experiment_mode": "generic",
            "fruit_type": "",
            "storage_day": "",
            "freshness_state": "",
            "temperature_c": "",
            "weight_g": "",
            "firmness_score": "",
            "surface_note": "",
        }

    if not args.fruit_type or not args.fruit_type.strip():
        raise ValueError("新鲜度实验必须提供 --fruit-type，例如 nectarine")
    if args.storage_day is None or args.storage_day < 0:
        raise ValueError("新鲜度实验必须提供非负整数 --storage-day")
    if args.label and args.label.strip() != freshness_state:
        raise ValueError("新鲜度实验中 --label 必须与 --freshness-state 相同，或省略 --label")
    if args.temperature_c is not None and not -20.0 <= args.temperature_c <= 60.0:
        raise ValueError("--temperature-c 应位于 -20 到 60 摄氏度")
    if args.weight_g is not None and args.weight_g <= 0:
        raise ValueError("--weight-g 必须大于 0")
    if args.firmness_score is not None and not 1 <= args.firmness_score <= 5:
        raise ValueError("--firmness-score 必须是 1 到 5 的整数")

    return {
        "experiment_mode": "freshness",
        "fruit_type": args.fruit_type.strip(),
        "storage_day": args.storage_day,
        "freshness_state": freshness_state,
        "temperature_c": "" if args.temperature_c is None else args.temperature_c,
        "weight_g": "" if args.weight_g is None else args.weight_g,
        "firmness_score": "" if args.firmness_score is None else args.firmness_score,
        "surface_note": (args.surface_note or "").strip(),
    }


def collect(args: argparse.Namespace) -> int:
    config = load_config(Path(args.config))
    serial_config = config["serial"]
    measurement_config = config["measurement"]

    port = args.port or serial_config["port"]
    baudrate = args.baudrate or int(serial_config["baudrate"])
    timeout = float(serial_config["timeout_seconds"])
    target_frames = (
        args.frames
        if args.frames is not None
        else int(measurement_config["frames_per_position"])
    )
    settle_seconds = (
        args.settle_seconds
        if args.settle_seconds is not None
        else int(measurement_config.get("settle_seconds", 0))
    )
    warmup_frames = (
        args.warmup_frames
        if args.warmup_frames is not None
        else int(measurement_config.get("warmup_frames", 0))
    )

    try:
        metadata = resolve_experiment_metadata(args)
    except ValueError as error:
        print(f"[参数错误] {error}")
        return 2

    sample_id = prompt_if_missing(args.sample_id, "请输入样品编号（例如 NECT01）：")
    label = (
        metadata["freshness_state"]
        if metadata["experiment_mode"] == "freshness"
        else prompt_if_missing(args.label, "请输入样品类别（例如 green_tea）：")
    )
    position = prompt_if_missing(args.position, "请输入测量位置编号（例如 1）：")

    raw_data_dir = PROJECT_ROOT / config["paths"]["raw_data_dir"]
    raw_data_dir.mkdir(parents=True, exist_ok=True)
    output_path = build_output_path(
        raw_data_dir,
        sample_id,
        label,
        position,
        metadata["storage_day"],
    )

    print("\n=== 串口采集参数 ===")
    print(f"串口: {port}")
    print(f"波特率: {baudrate}")
    print(f"样品编号: {sample_id}")
    print(f"类别: {label}")
    print(f"测量位置: {position}")
    if metadata["experiment_mode"] == "freshness":
        print(f"水果类型: {metadata['fruit_type']}")
        print(f"储存天数: Day {metadata['storage_day']}")
        print(f"新鲜度标签: {metadata['freshness_state']}")
        print(f"温度: {metadata['temperature_c'] if metadata['temperature_c'] != '' else '未记录'} °C")
        print(f"重量: {metadata['weight_g'] if metadata['weight_g'] != '' else '未记录'} g")
        print(f"硬度评分: {metadata['firmness_score'] if metadata['firmness_score'] != '' else '未记录'}")
        print(f"表面备注: {metadata['surface_note'] or '无'}")
    print(f"目标帧数: {target_frames if target_frames > 0 else '无限，按 Ctrl+C 停止'}")
    print(f"手部离开倒计时: {settle_seconds} 秒")
    print(f"预热丢弃: {warmup_frames} 帧")
    print(f"保存文件: {output_path}")
    print("注意：运行前必须关闭 ESP-IDF Monitor 和其他串口助手。\n")

    accepted = 0
    received_data = 0
    rejected = 0
    ignored_lines = 0
    malformed_lines = 0

    try:
        with serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout,
            rtscts=False,
            dsrdtr=False,
        ) as serial_port, output_path.open(
            "w", newline="", encoding="utf-8-sig"
        ) as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=CSV_FIELDS)
            writer.writeheader()
            csv_file.flush()

            if settle_seconds > 0:
                print("\n请保持装置不动并把手移开：")
                for remaining in range(settle_seconds, 0, -1):
                    print(f"  {remaining}...")
                    time.sleep(1)

            # 倒计时期间串口仍在收数据，先清空旧帧再开始预热。
            serial_port.reset_input_buffer()

            warmed_up = 0
            if warmup_frames > 0:
                print(f"开始预热，前 {warmup_frames} 条DATA不会写入CSV：")

            while warmed_up < warmup_frames:
                raw_bytes = serial_port.readline()
                if not raw_bytes:
                    continue

                line = raw_bytes.decode("utf-8", errors="replace")
                if "DATA," not in line:
                    ignored_lines += 1
                    continue

                try:
                    parsed = parse_data_line(line)
                except DataLineError:
                    malformed_lines += 1
                    continue

                warmed_up += 1
                print(
                    f"  预热 {warmed_up:02d}/{warmup_frames:02d} "
                    f"距离={parsed['distance_mm']} mm Clear={parsed['Clear']}"
                )

            if warmup_frames > 0:
                print("预热完成，开始正式保存。\n")

            while target_frames <= 0 or accepted < target_frames:
                raw_bytes = serial_port.readline()
                if not raw_bytes:
                    continue

                line = raw_bytes.decode("utf-8", errors="replace")
                if "DATA," not in line:
                    ignored_lines += 1
                    continue

                try:
                    parsed = parse_data_line(line)
                except DataLineError as error:
                    malformed_lines += 1
                    print(f"[跳过不完整数据] {error}: {strip_ansi(line).strip()}")
                    continue

                row = build_csv_row(
                    parsed=parsed,
                    sample_id=sample_id,
                    label=label,
                    position=position,
                    distance_min_mm=int(measurement_config["distance_min_mm"]),
                    distance_max_mm=int(measurement_config["distance_max_mm"]),
                    **metadata,
                )
                writer.writerow(row)
                csv_file.flush()
                received_data += 1

                if row["quality_valid"]:
                    accepted += 1
                else:
                    rejected += 1

                progress = (
                    f"{accepted:03d}/{target_frames:03d}"
                    if target_frames > 0
                    else f"{accepted:03d}"
                )
                range_text = "范围内" if row["distance_in_range"] else "范围外/无效"
                quality_text = "接收" if row["quality_valid"] else "拒绝"
                print(
                    f"[{progress}] {quality_text} ESP样本={row['esp_sample']} "
                    f"距离={row['distance_mm']} mm ({range_text}) "
                    f"Clear={row['Clear']} NIR={row['NIR']}"
                )

    except KeyboardInterrupt:
        print("\n收到 Ctrl+C，正在安全结束采集。")
    except serial.SerialException as error:
        print(f"\n[串口错误] {error}")
        print("请确认 COM 口正确，并关闭 ESP-IDF Monitor 和其他串口助手。")
        list_serial_ports()
        return 1

    print("\n=== 采集完成 ===")
    print(f"串口DATA总数: {received_data} 条")
    print(f"合格样本: {accepted} 条")
    print(f"拒绝样本: {rejected} 条（仍保存在CSV中供排查）")
    print(f"忽略普通日志: {ignored_lines} 行")
    print(f"跳过不完整 DATA: {malformed_lines} 行")
    print(f"文件位置: {output_path}")
    return 0 if accepted > 0 else 1


def run_parser_test() -> int:
    example = (
        "DATA,sample=1726,distance_mm=59,"
        "F1_415nm=32,F2_445nm=35,F3_480nm=23,F4_515nm=151,"
        "F5_555nm=56,F6_590nm=267,F7_630nm=408,F8_680nm=105,"
        "Clear=496,NIR=15"
    )
    parsed = parse_data_line(example)
    print("解析测试成功：")
    print(json.dumps(parsed, ensure_ascii=False, indent=2))
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="采集 ESP32-S3 输出的 AS7341/VL53L0X 数据并保存为 CSV。"
    )
    parser.add_argument("--config", default=str(DEFAULT_CONFIG_PATH))
    parser.add_argument("--port", help="串口号，例如 COM9")
    parser.add_argument("--baudrate", type=int, help="串口波特率")
    parser.add_argument("--sample-id", help="独立样品编号，例如 G01")
    parser.add_argument("--label", help="类别标签，例如 green_tea")
    parser.add_argument("--position", help="测量位置，例如 1")
    parser.add_argument("--fruit-type", help="水果类型，例如 nectarine")
    parser.add_argument(
        "--storage-day",
        type=int,
        help="从 Day 0 开始计算的非负储存天数",
    )
    parser.add_argument(
        "--freshness-state",
        choices=FRESHNESS_STATES,
        help="新鲜度标签；提供后自动进入新鲜度实验模式",
    )
    parser.add_argument("--temperature-c", type=float, help="采集时室温，单位摄氏度")
    parser.add_argument("--weight-g", type=float, help="当前整果重量，单位克")
    parser.add_argument(
        "--firmness-score",
        type=int,
        help="手感硬度评分：1=很软，5=很硬",
    )
    parser.add_argument("--surface-note", help="表面颜色、斑点、皱缩等简短备注")
    parser.add_argument(
        "--frames",
        type=int,
        help="采集帧数；0 表示持续采集直到 Ctrl+C",
    )
    parser.add_argument(
        "--settle-seconds",
        type=int,
        help="打开串口后留给手部离开的倒计时秒数",
    )
    parser.add_argument(
        "--warmup-frames",
        type=int,
        help="正式保存前丢弃的完整DATA帧数",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="列出本机串口后退出",
    )
    parser.add_argument(
        "--test-parser",
        action="store_true",
        help="使用示例数据测试解析器，不连接开发板",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.list_ports:
        return 0 if list_serial_ports() else 1
    if args.test_parser:
        return run_parser_test()
    return collect(args)


if __name__ == "__main__":
    raise SystemExit(main())
