"""Validation and serialization for the ESP32/cloud JSON protocol."""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any


PROTOCOL_VERSION = 1
SPECTRUM_CHANNELS = (
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
)
SUPPORTED_TASKS = {"fruit_identity", "freshness"}


class ProtocolError(ValueError):
    """A request does not conform to the gateway protocol."""


def _required_mapping(value: Any, field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ProtocolError(f"{field} must be an object")
    return value


def _required_text(value: Any, field: str, max_length: int = 64) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ProtocolError(f"{field} must be a non-empty string")
    text = value.strip()
    if len(text) > max_length:
        raise ProtocolError(f"{field} is too long")
    return text


def _finite_number(value: Any, field: str, minimum: float | None = None) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ProtocolError(f"{field} must be a number")
    result = float(value)
    if not math.isfinite(result):
        raise ProtocolError(f"{field} must be finite")
    if minimum is not None and result < minimum:
        raise ProtocolError(f"{field} must be >= {minimum:g}")
    return result


@dataclass(frozen=True)
class EdgeResult:
    task: str
    label: str
    confidence: float


@dataclass(frozen=True)
class AnalyzeRequest:
    request_id: str
    device_id: str
    timestamp_ms: int
    distance_mm: int
    spectrum: dict[str, float]
    edge_result: EdgeResult

    @classmethod
    def from_dict(cls, payload: Any) -> "AnalyzeRequest":
        root = _required_mapping(payload, "request")
        if root.get("protocol_version") != PROTOCOL_VERSION:
            raise ProtocolError(f"protocol_version must be {PROTOCOL_VERSION}")

        measurement = _required_mapping(root.get("measurement"), "measurement")
        raw_spectrum = _required_mapping(measurement.get("spectrum"), "measurement.spectrum")
        spectrum: dict[str, float] = {}
        for channel in SPECTRUM_CHANNELS:
            spectrum[channel] = _finite_number(
                raw_spectrum.get(channel), f"measurement.spectrum.{channel}", minimum=0
            )

        edge = _required_mapping(root.get("edge_result"), "edge_result")
        task = _required_text(edge.get("task"), "edge_result.task", 32)
        if task not in SUPPORTED_TASKS:
            raise ProtocolError(
                "edge_result.task must be fruit_identity or freshness"
            )
        confidence = _finite_number(
            edge.get("confidence"), "edge_result.confidence", minimum=0
        )
        if confidence > 1:
            raise ProtocolError("edge_result.confidence must be between 0 and 1")

        timestamp_ms = _finite_number(root.get("timestamp_ms"), "timestamp_ms", minimum=0)
        distance_mm = _finite_number(
            measurement.get("distance_mm"), "measurement.distance_mm", minimum=0
        )
        return cls(
            request_id=_required_text(root.get("request_id"), "request_id", 64),
            device_id=_required_text(root.get("device_id"), "device_id", 64),
            timestamp_ms=int(timestamp_ms),
            distance_mm=int(distance_mm),
            spectrum=spectrum,
            edge_result=EdgeResult(
                task=task,
                label=_required_text(edge.get("label"), "edge_result.label", 32),
                confidence=confidence,
            ),
        )


def success_response(
    request: AnalyzeRequest,
    *,
    risk_level: str,
    summary: str,
    advice: str,
    model_source: str,
) -> dict[str, Any]:
    return {
        "protocol_version": PROTOCOL_VERSION,
        "request_id": request.request_id,
        "ok": True,
        "risk_level": risk_level,
        "summary": summary,
        "advice": advice,
        "model_source": model_source,
    }


def error_response(message: str, request_id: str = "unknown") -> dict[str, Any]:
    return {
        "protocol_version": PROTOCOL_VERSION,
        "request_id": request_id,
        "ok": False,
        "error": message,
    }
