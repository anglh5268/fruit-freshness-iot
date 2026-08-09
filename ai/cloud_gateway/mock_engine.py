"""Deterministic report generator used before the real LLM is connected."""

from __future__ import annotations

from .schemas import AnalyzeRequest, success_response


FRESHNESS_REPORTS = {
    "fresh": ("low", "果实表面状态较新鲜", "建议阴凉保存并尽快食用"),
    "warning": ("medium", "检测到早期品质下降迹象", "建议尽快食用并检查局部软化"),
    "spoiled": ("high", "检测到明显腐败风险", "建议停止食用并进一步人工检查"),
}

FRUIT_NAMES = {
    "banana": "香蕉",
    "fuzzy_peach": "毛桃",
    "nectarine": "油桃",
}


def analyze(request: AnalyzeRequest) -> dict[str, object]:
    """Return a stable mock response with the same schema as the future LLM."""
    result = request.edge_result
    if result.task == "freshness":
        risk, summary, advice = FRESHNESS_REPORTS.get(
            result.label,
            ("unknown", "本地结果暂时无法解释", "请重新测量并人工检查"),
        )
    else:
        fruit_name = FRUIT_NAMES.get(result.label, result.label)
        risk = "unknown"
        summary = f"本地模型识别为{fruit_name}"
        advice = "当前仅验证水果类别，腐败判断需等待新鲜度模型"

    if result.confidence < 0.55:
        risk = "unknown"
        advice = "置信度偏低，请调整距离与位置后重新测量"

    return success_response(
        request,
        risk_level=risk,
        summary=summary,
        advice=advice,
        model_source="mock",
    )
