"""Local configuration for mock and Volcengine Ark gateway modes."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path


DEFAULT_ARK_BASE_URL = "https://ark.cn-beijing.volces.com/api/v3"
ENV_PATH = Path(__file__).resolve().parent / ".env"


def load_env_file(path: Path = ENV_PATH) -> None:
    """Load a small KEY=VALUE file without adding a third-party dependency."""
    if not path.exists():
        return
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
            value = value[1:-1]
        if key:
            os.environ.setdefault(key, value)


@dataclass(frozen=True)
class GatewaySettings:
    mode: str
    ark_api_key: str
    ark_model_id: str
    ark_base_url: str
    ark_timeout_seconds: float
    device_api_token: str = ""

    @property
    def ark_configured(self) -> bool:
        return bool(self.ark_api_key and self.ark_model_id)


def get_settings() -> GatewaySettings:
    load_env_file()
    mode = os.getenv("CLOUD_GATEWAY_MODE", "mock").strip().lower()
    if mode not in {"mock", "ark"}:
        raise ValueError("CLOUD_GATEWAY_MODE must be mock or ark")
    model_id = (
        os.getenv("ARK_MODEL_ID", "").strip()
        or os.getenv("DOUBAO_ENDPOINT_ID", "").strip()
    )
    try:
        timeout = float(os.getenv("ARK_TIMEOUT_SECONDS", "30"))
    except ValueError as exc:
        raise ValueError("ARK_TIMEOUT_SECONDS must be a number") from exc
    if timeout <= 0 or timeout > 120:
        raise ValueError("ARK_TIMEOUT_SECONDS must be between 0 and 120")
    return GatewaySettings(
        mode=mode,
        ark_api_key=os.getenv("ARK_API_KEY", "").strip(),
        ark_model_id=model_id,
        ark_base_url=os.getenv("ARK_BASE_URL", DEFAULT_ARK_BASE_URL).strip().rstrip("/"),
        ark_timeout_seconds=timeout,
        device_api_token=os.getenv("DEVICE_API_TOKEN", "").strip(),
    )
