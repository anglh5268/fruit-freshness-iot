"""Minimal Volcengine Ark Responses API client using Python's standard library."""

from __future__ import annotations

import json
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any

from .prompt_builder import build_prompt
from .schemas import AnalyzeRequest, success_response
from .settings import GatewaySettings


ALLOWED_RISK_LEVELS = {"low", "medium", "high", "unknown"}


class ArkClientError(RuntimeError):
    """The Ark service call or its model output was invalid."""


def extract_output_text(response: Any) -> str:
    if not isinstance(response, dict):
        raise ArkClientError("Ark response must be a JSON object")
    output = response.get("output")
    if not isinstance(output, list):
        raise ArkClientError("Ark response has no output list")
    for item in output:
        if not isinstance(item, dict) or item.get("type") != "message":
            continue
        content = item.get("content")
        if not isinstance(content, list):
            continue
        for part in content:
            if (
                isinstance(part, dict)
                and part.get("type") == "output_text"
                and isinstance(part.get("text"), str)
                and part["text"].strip()
            ):
                return part["text"].strip()
    raise ArkClientError("Ark response contains no output_text")


def parse_report_text(text: str) -> dict[str, str]:
    candidate = text.strip()
    if candidate.startswith("```"):
        lines = candidate.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].strip() == "```":
            lines = lines[:-1]
        candidate = "\n".join(lines).strip()
    start = candidate.find("{")
    end = candidate.rfind("}")
    if start < 0 or end <= start:
        raise ArkClientError("model output is not a JSON object")
    try:
        payload = json.loads(candidate[start : end + 1])
    except json.JSONDecodeError as exc:
        raise ArkClientError("model output contains invalid JSON") from exc
    if not isinstance(payload, dict):
        raise ArkClientError("model report must be an object")

    expected = {"risk_level", "summary", "advice"}
    if set(payload) != expected:
        raise ArkClientError("model report fields must be risk_level, summary and advice")
    risk = payload.get("risk_level")
    summary = payload.get("summary")
    advice = payload.get("advice")
    if risk not in ALLOWED_RISK_LEVELS:
        raise ArkClientError("model risk_level is invalid")
    if not isinstance(summary, str) or not summary.strip() or len(summary.strip()) > 30:
        raise ArkClientError("model summary is empty or too long")
    if not isinstance(advice, str) or not advice.strip() or len(advice.strip()) > 45:
        raise ArkClientError("model advice is empty or too long")
    return {
        "risk_level": risk,
        "summary": summary.strip(),
        "advice": advice.strip(),
    }


@dataclass(frozen=True)
class ArkClient:
    settings: GatewaySettings

    def analyze(self, request: AnalyzeRequest) -> dict[str, Any]:
        if not self.settings.ark_configured:
            raise ArkClientError("ARK_API_KEY and ARK_MODEL_ID are required")
        endpoint = self.settings.ark_base_url + "/responses"
        body = json.dumps(
            {
                "model": self.settings.ark_model_id,
                "input": build_prompt(request),
                "store": False,
            },
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8")
        http_request = urllib.request.Request(
            endpoint,
            data=body,
            method="POST",
            headers={
                "Authorization": f"Bearer {self.settings.ark_api_key}",
                "Content-Type": "application/json",
                "Accept": "application/json",
            },
        )
        try:
            with urllib.request.urlopen(
                http_request, timeout=self.settings.ark_timeout_seconds
            ) as response:
                raw_response = response.read()
        except urllib.error.HTTPError as exc:
            details = exc.read().decode("utf-8", errors="replace")[:500]
            raise ArkClientError(f"Ark HTTP {exc.code}: {details}") from exc
        except urllib.error.URLError as exc:
            raise ArkClientError(f"Ark connection failed: {exc.reason}") from exc
        except TimeoutError as exc:
            raise ArkClientError("Ark request timed out") from exc

        try:
            response_payload = json.loads(raw_response.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ArkClientError("Ark returned invalid UTF-8 JSON") from exc
        report = parse_report_text(extract_output_text(response_payload))
        return success_response(
            request,
            risk_level=report["risk_level"],
            summary=report["summary"],
            advice=report["advice"],
            model_source="ark",
        )
