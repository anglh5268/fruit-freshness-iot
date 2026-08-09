"""Small LAN HTTP gateway for ESP32 integration tests.

Run from the spectrum_ai directory:
    python -m cloud_gateway.server
"""

from __future__ import annotations

import argparse
import hmac
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

from .engine import (
    ArkClientError,
    GatewayConfigurationError,
    analyze,
    gateway_status,
)
from .schemas import AnalyzeRequest, ProtocolError, error_response
from .settings import get_settings


MAX_BODY_BYTES = 16 * 1024


def device_request_is_authorized(supplied_token: str) -> bool:
    """Validate the optional device token without leaking timing information."""
    expected_token = get_settings().device_api_token
    return not expected_token or hmac.compare_digest(supplied_token, expected_token)


def default_listen_port() -> int:
    """Use the cloud runtime port while retaining a safe local default."""
    raw_port = os.getenv("_FAAS_RUNTIME_PORT", os.getenv("PORT", "8000"))
    try:
        port = int(raw_port)
    except ValueError:
        return 8000
    return port if 1 <= port <= 65535 else 8000


class GatewayHandler(BaseHTTPRequestHandler):
    server_version = "SpectrumGateway/0.1"

    def _send_json(self, status: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/health":
            try:
                self._send_json(200, gateway_status())
            except ValueError as exc:
                self._send_json(500, error_response(str(exc)))
            return
        self._send_json(404, error_response("not found"))

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path != "/analyze":
            self._send_json(404, error_response("not found"))
            return

        supplied_token = self.headers.get("X-Device-Token", "")
        if not device_request_is_authorized(supplied_token):
            self._send_json(401, error_response("unauthorized"))
            return

        request_id = "unknown"
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            if content_length <= 0 or content_length > MAX_BODY_BYTES:
                raise ProtocolError("invalid request body size")
            raw_body = self.rfile.read(content_length)
            payload = json.loads(raw_body.decode("utf-8"))
            if isinstance(payload, dict) and isinstance(payload.get("request_id"), str):
                request_id = payload["request_id"]
            request = AnalyzeRequest.from_dict(payload)
            self._send_json(200, analyze(request))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._send_json(400, error_response("body must be valid UTF-8 JSON", request_id))
        except ProtocolError as exc:
            self._send_json(422, error_response(str(exc), request_id))
        except GatewayConfigurationError as exc:
            self._send_json(503, error_response(str(exc), request_id))
        except ArkClientError as exc:
            self._send_json(502, error_response(str(exc), request_id))
        except (TypeError, ValueError):
            self._send_json(400, error_response("invalid request", request_id))

    def log_message(self, format: str, *args: object) -> None:
        print(f"[gateway] {self.address_string()} - {format % args}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="ESP32 spectrum cloud gateway")
    parser.add_argument("--host", default="0.0.0.0", help="listen address")
    parser.add_argument("--port", type=int, default=default_listen_port(), help="listen port")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    status = gateway_status()
    server = ThreadingHTTPServer((args.host, args.port), GatewayHandler)
    print(
        f"Spectrum cloud gateway ({status['mode']}) listening on "
        f"http://{args.host}:{args.port}"
    )
    print(f"Configured: {status['configured']}, model: {status['model']}")
    print("Health: GET /health, analysis: POST /analyze")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nGateway stopped.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
