"""Offline tests for Volcengine Ark mode; no real API call is made."""

from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cloud_gateway.ark_client import (
    ArkClient,
    ArkClientError,
    extract_output_text,
    parse_report_text,
)
from cloud_gateway.prompt_builder import build_prompt
from cloud_gateway.schemas import AnalyzeRequest, SPECTRUM_CHANNELS
from cloud_gateway.settings import GatewaySettings, load_env_file


def make_request(task: str = "fruit_identity") -> AnalyzeRequest:
    return AnalyzeRequest.from_dict(
        {
            "protocol_version": 1,
            "request_id": "esp32-42",
            "device_id": "ESP32S3-01",
            "timestamp_ms": 1234,
            "measurement": {
                "distance_mm": 20,
                "spectrum": {
                    channel: index * 10
                    for index, channel in enumerate(SPECTRUM_CHANNELS, 1)
                },
            },
            "edge_result": {
                "task": task,
                "label": "nectarine" if task == "fruit_identity" else "warning",
                "confidence": 0.71,
            },
        }
    )


class FakeHttpResponse:
    def __init__(self, payload: dict[str, object]) -> None:
        self.body = json.dumps(payload, ensure_ascii=False).encode("utf-8")

    def __enter__(self) -> "FakeHttpResponse":
        return self

    def __exit__(self, *args: object) -> None:
        return None

    def read(self) -> bytes:
        return self.body


class ArkClientTests(unittest.TestCase):
    def test_extracts_responses_api_output_text(self) -> None:
        payload = {
            "output": [
                {
                    "type": "message",
                    "content": [{"type": "output_text", "text": "hello"}],
                }
            ]
        }
        self.assertEqual(extract_output_text(payload), "hello")

    def test_parses_json_even_when_model_adds_code_fence(self) -> None:
        report = parse_report_text(
            '```json\n{"risk_level":"unknown","summary":"本地识别为油桃",'
            '"advice":"请等待新鲜度模型"}\n```'
        )
        self.assertEqual(report["risk_level"], "unknown")

    def test_rejects_unapproved_report_fields(self) -> None:
        with self.assertRaises(ArkClientError):
            parse_report_text(
                '{"risk_level":"low","summary":"ok","advice":"ok","extra":1}'
            )

    def test_identity_prompt_forbids_freshness_claim(self) -> None:
        prompt = build_prompt(make_request())
        self.assertIn("risk_level必须为unknown", prompt)
        self.assertIn('"task":"fruit_identity"', prompt)

    def test_real_client_maps_valid_model_json_to_esp_response(self) -> None:
        model_text = json.dumps(
            {
                "risk_level": "unknown",
                "summary": "本地识别为油桃",
                "advice": "尚不能判断新鲜度，请等待新鲜度模型",
            },
            ensure_ascii=False,
        )
        api_payload = {
            "output": [
                {
                    "type": "message",
                    "content": [{"type": "output_text", "text": model_text}],
                }
            ]
        }
        settings = GatewaySettings(
            mode="ark",
            ark_api_key="test-key-not-real",
            ark_model_id="doubao-test-model",
            ark_base_url="https://ark.example/api/v3",
            ark_timeout_seconds=5,
        )
        with patch(
            "cloud_gateway.ark_client.urllib.request.urlopen",
            return_value=FakeHttpResponse(api_payload),
        ) as urlopen:
            result = ArkClient(settings).analyze(make_request())
        self.assertEqual(result["model_source"], "ark")
        self.assertEqual(result["risk_level"], "unknown")
        sent_request = urlopen.call_args.args[0]
        self.assertEqual(sent_request.full_url, "https://ark.example/api/v3/responses")
        self.assertEqual(sent_request.get_header("Authorization"), "Bearer test-key-not-real")

    def test_env_loader_does_not_overwrite_existing_process_value(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".env"
            path.write_text("ARK_API_KEY=file-value\nARK_MODEL_ID=model-a\n", encoding="utf-8")
            with patch.dict(os.environ, {"ARK_API_KEY": "process-value"}, clear=True):
                load_env_file(path)
                self.assertEqual(os.environ["ARK_API_KEY"], "process-value")
                self.assertEqual(os.environ["ARK_MODEL_ID"], "model-a")


if __name__ == "__main__":
    unittest.main()
