#  HttpLink — отправка POST'ом на ESP32
#  by KTH-Studio
import time
import config


class HttpLink:
    def __init__(self):
        self.host    = config.ESP_HTTP_HOST
        self.port    = config.ESP_HTTP_PORT
        self.url     = ""
        self.session = None
        self.last_ok = 0

    def connect(self):
        host = self.host or self._auto_detect()
        if not host:
            return False

        self.url = f"http://{host}:{self.port}/api/data"

        try:
            import requests
        except ImportError:
            if config.DEBUG:
                print("[HTTP] requests not installed")
            return False

        self.session = requests.Session()
        # Проверим доступность — GET /api/status
        try:
            r = self.session.get(
                f"http://{host}:{self.port}/api/status",
                timeout=2.0,
            )
            if r.status_code == 200:
                if config.DEBUG:
                    print(f"[HTTP] connected to {host}:{self.port}")
                return True
        except Exception as e:
            if config.DEBUG:
                print(f"[HTTP] connect error: {e}")

        self.session = None
        return False

    def _auto_detect(self):
        """Так же, как в TCP — mDNS."""
        try:
            import socket
            for name in ("pcmonitor.local", "esp32.local", "esp.local"):
                try:
                    ip = socket.gethostbyname(name)
                    if ip:
                        return ip
                except Exception:
                    continue
        except Exception:
            pass
        return None

    def send(self, payload: str):
        if self.session is None or not self.url:
            return False
        try:
            r = self.session.post(
                self.url,
                data=payload.encode("utf-8"),
                headers={"Content-Type": "application/json"},
                timeout=2.0,
            )
            if r.status_code == 200:
                self.last_ok = time.time()
                return True
            if config.DEBUG:
                print(f"[HTTP] bad status: {r.status_code}")
            return False
        except Exception as e:
            if config.DEBUG:
                print(f"[HTTP] post error: {e}")
            self.session = None   # сброс — переподключимся
            return False

    def is_alive(self):
        return self.session is not None

    def close(self):
        if self.session is not None:
            try:
                self.session.close()
            except Exception:
                pass
            self.session = None
