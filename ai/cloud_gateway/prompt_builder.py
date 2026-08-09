"""Build a constrained prompt that explains, but does not replace, edge inference."""

from __future__ import annotations

import json

from .schemas import AnalyzeRequest


SYSTEM_RULES = """你是水果光谱检测设备的报告解释器。
本地边缘模型结果是报告的唯一判断依据，不得根据原始光谱自行重新分类。
当task为fruit_identity时，设备尚不能判断新鲜度，risk_level必须为unknown，明确提示需要新鲜度模型。
当task为freshness时，只能按本地label解释：fresh对应low，warning对应medium，spoiled对应high。
置信度低于0.55时risk_level必须为unknown，并建议重新测量。
只返回一个JSON对象，禁止Markdown、代码围栏和额外文字。
字段必须为risk_level、summary、advice；risk_level只能是low、medium、high、unknown。
summary不超过30个汉字，advice不超过45个汉字。不要声称这是食品安全或医学结论。"""


def build_prompt(request: AnalyzeRequest) -> str:
    measurement = {
        "device_id": request.device_id,
        "distance_mm": request.distance_mm,
        "spectrum": request.spectrum,
        "edge_result": {
            "task": request.edge_result.task,
            "label": request.edge_result.label,
            "confidence": round(request.edge_result.confidence, 4),
        },
    }
    data = json.dumps(measurement, ensure_ascii=False, separators=(",", ":"))
    return (
        SYSTEM_RULES
        + "\n待解释的设备数据："
        + data
        + '\n返回示例：{"risk_level":"unknown","summary":"本地识别为油桃",'
          '"advice":"尚不能判断新鲜度，请等待新鲜度模型"}'
    )
