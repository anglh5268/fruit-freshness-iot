"""Select the deterministic mock engine or the real Volcengine Ark engine."""

from __future__ import annotations

from typing import Any

from .ark_client import ArkClient, ArkClientError
from .mock_engine import analyze as mock_analyze
from .schemas import AnalyzeRequest
from .settings import GatewaySettings, get_settings


class GatewayConfigurationError(RuntimeError):
    """The selected gateway mode is not ready to serve analysis requests."""


def gateway_status(settings: GatewaySettings | None = None) -> dict[str, Any]:
    configuration = settings or get_settings()
    configured = configuration.mode == "mock" or configuration.ark_configured
    return {
        "ok": True,
        "service": "spectrum-cloud-gateway",
        "mode": configuration.mode,
        "configured": configured,
        "model": configuration.ark_model_id if configuration.mode == "ark" else "mock",
        "device_auth_required": bool(configuration.device_api_token),
    }


def analyze(request: AnalyzeRequest, settings: GatewaySettings | None = None) -> dict[str, Any]:
    configuration = settings or get_settings()
    if configuration.mode == "mock":
        return mock_analyze(request)
    if not configuration.ark_configured:
        raise GatewayConfigurationError(
            "ark mode requires ARK_API_KEY and ARK_MODEL_ID in cloud_gateway/.env"
        )
    return ArkClient(configuration).analyze(request)


__all__ = [
    "ArkClientError",
    "GatewayConfigurationError",
    "analyze",
    "gateway_status",
]
