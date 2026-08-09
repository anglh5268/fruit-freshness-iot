"""Offline tests for the role-4 cloud gateway protocol."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from cloud_gateway.mock_engine import analyze
from cloud_gateway.schemas import AnalyzeRequest, ProtocolError, SPECTRUM_CHANNELS
from cloud_gateway.server import default_listen_port, device_request_is_authorized


def make_payload(task: str = "freshness", label: str = "warning", confidence: float = 0.82):
    return {
        "protocol_version": 1,
        "request_id": "test-1",
        "device_id": "ESP32S3-01",
        "timestamp_ms": 1234,
        "measurement": {
            "distance_mm": 20,
            "spectrum": {channel: index * 10 for index, channel in enumerate(SPECTRUM_CHANNELS, 1)},
        },
        "edge_result": {"task": task, "label": label, "confidence": confidence},
    }


class CloudGatewayTests(unittest.TestCase):
    def test_freshness_request_generates_compatible_report(self) -> None:
        request = AnalyzeRequest.from_dict(make_payload())
        response = analyze(request)
        self.assertTrue(response["ok"])
        self.assertEqual(response["risk_level"], "medium")
        self.assertEqual(response["model_source"], "mock")
        self.assertEqual(response["request_id"], "test-1")

    def test_current_fruit_classifier_is_supported(self) -> None:
        request = AnalyzeRequest.from_dict(make_payload("fruit_identity", "nectarine", 0.71))
        response = analyze(request)
        self.assertIn("油桃", response["summary"])
        self.assertEqual(response["risk_level"], "unknown")

    def test_low_confidence_requests_remeasurement(self) -> None:
        request = AnalyzeRequest.from_dict(make_payload(confidence=0.4))
        response = analyze(request)
        self.assertEqual(response["risk_level"], "unknown")

    def test_missing_channel_is_rejected(self) -> None:
        payload = make_payload()
        del payload["measurement"]["spectrum"]["NIR"]
        with self.assertRaises(ProtocolError):
            AnalyzeRequest.from_dict(payload)

    def test_invalid_confidence_is_rejected(self) -> None:
        with self.assertRaises(ProtocolError):
            AnalyzeRequest.from_dict(make_payload(confidence=1.2))

    def test_device_token_is_optional_for_local_development(self) -> None:
        with patch.dict("os.environ", {"DEVICE_API_TOKEN": ""}):
            self.assertTrue(device_request_is_authorized(""))

    def test_configured_device_token_is_required(self) -> None:
        with patch.dict("os.environ", {"DEVICE_API_TOKEN": "test-device-secret"}):
            self.assertFalse(device_request_is_authorized(""))
            self.assertFalse(device_request_is_authorized("wrong-token"))
            self.assertTrue(device_request_is_authorized("test-device-secret"))

    def test_vefaas_runtime_port_has_priority(self) -> None:
        with patch.dict(
            "os.environ",
            {"_FAAS_RUNTIME_PORT": "9000", "PORT": "7000"},
        ):
            self.assertEqual(default_listen_port(), 9000)


if __name__ == "__main__":
    unittest.main()
