"""读取少量串口帧，验证净反射值是否等于开灯值减去环境光值。"""

from __future__ import annotations

import argparse
import time

import serial

from serial_collect import (
    DEFAULT_CONFIG_PATH,
    DataLineError,
    SPECTRAL_FIELDS,
    load_config,
    parse_data_line,
)


def parse_args() -> argparse.Namespace:
    config = load_config(DEFAULT_CONFIG_PATH)
    parser = argparse.ArgumentParser(description="验证AS7341补光净反射串口数据。")
    parser.add_argument("--port", default=config["serial"]["port"])
    parser.add_argument(
        "--baudrate",
        type=int,
        default=int(config["serial"]["baudrate"]),
    )
    parser.add_argument("--frames", type=int, default=5)
    parser.add_argument("--timeout", type=float, default=15.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    deadline = time.monotonic() + args.timeout
    received = 0
    all_valid = True

    print(f"读取 {args.port}，目标 {args.frames} 帧……")

    with serial.Serial(
        args.port,
        args.baudrate,
        timeout=1.0,
        dsrdtr=False,
        rtscts=False,
    ) as port:
        port.dtr = False
        port.rts = False

        while received < args.frames and time.monotonic() < deadline:
            line = port.readline().decode("utf-8", errors="replace")
            if "DATA," not in line:
                continue

            try:
                data = parse_data_line(line)
            except DataLineError:
                continue

            missing_raw = [
                field
                for channel in SPECTRAL_FIELDS
                for field in (f"Ambient_{channel}", f"Lit_{channel}")
                if field not in data
            ]
            if missing_raw:
                print(f"[失败] 缺少原始补光字段: {', '.join(missing_raw)}")
                return 1

            frame_valid = all(
                data[channel]
                == max(
                    data[f"Lit_{channel}"] - data[f"Ambient_{channel}"],
                    0,
                )
                for channel in SPECTRAL_FIELDS
            )
            all_valid = all_valid and frame_valid
            received += 1

            print(
                f"[{received}/{args.frames}] sample={data['sample']} "
                f"distance={data['distance_mm']}mm "
                f"Clear(net/ambient/lit)={data['Clear']}/"
                f"{data['Ambient_Clear']}/{data['Lit_Clear']} "
                f"减法={'正确' if frame_valid else '错误'}"
            )

    if received < args.frames:
        print(f"[失败] 超时，只收到 {received} 帧。")
        return 1

    if not all_valid:
        print("[失败] 至少一帧的净反射减法不正确。")
        return 1

    print("验证通过：所有通道的净反射减法均正确。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
