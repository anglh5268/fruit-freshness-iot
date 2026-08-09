"""检查 3 号位 Python 数据工作区是否配置成功。"""

from __future__ import annotations

import importlib
import json
import os
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = PROJECT_ROOT / "config" / "experiment.json"
os.environ.setdefault("MPLCONFIGDIR", str(PROJECT_ROOT / ".cache" / "matplotlib"))

REQUIRED_MODULES = {
    "numpy": "numpy",
    "pandas": "pandas",
    "matplotlib": "matplotlib",
    "seaborn": "seaborn",
    "scikit-learn": "sklearn",
    "pyserial": "serial",
    "openpyxl": "openpyxl",
    "joblib": "joblib",
}


def main() -> int:
    print("=== 光谱数据工作区检查 ===")
    print(f"Python 版本: {sys.version.split()[0]}")
    print(f"Python 路径: {sys.executable}")
    print(f"项目根目录: {PROJECT_ROOT}")

    if not CONFIG_PATH.exists():
        print(f"[失败] 找不到配置文件: {CONFIG_PATH}")
        return 1

    with CONFIG_PATH.open("r", encoding="utf-8") as file:
        config = json.load(file)

    print(f"串口配置: {config['serial']['port']} @ {config['serial']['baudrate']}")

    missing: list[str] = []
    for package_name, import_name in REQUIRED_MODULES.items():
        try:
            module = importlib.import_module(import_name)
            version = getattr(module, "__version__", "已安装")
            print(f"[正常] {package_name}: {version}")
        except ImportError:
            missing.append(package_name)
            print(f"[缺少] {package_name}")

    if missing:
        print("\n请在 VS Code 终端运行：")
        print("python -m pip install -r requirements.txt")
        return 1

    print("\n环境配置成功，可以开始编写串口采集程序。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
