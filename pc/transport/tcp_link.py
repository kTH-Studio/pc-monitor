#  TcpLink — отправка по TCP
#  by KTH-Studio
import socket
import time
import config


class TcpLink:
    def __init__(self):
        self.sock    = None
        self.host    = config.ESP_TCP_HOST
        self.port    = config.ESP_TCP_PORT
        self.last_ok = 0

    def connect(self):
        if self.sock is not None:
            return True

        host = self.host or self._auto_detect()
        if not host:
            return False

        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(2.0)
            self.sock.connect((host, self.port))
            self.sock.settimeout(1.0)
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            if config.DEBUG:
                print(f"[TCP] connected to {host}:{self.port}")
            return True
        except Exception as e:
            if config.DEBUG:
                print(f"[TCP] connect error ({host}:{self.port}): {e}")
            self.sock = None
            return False

    def _auto_detect(self):
        """
        Автоопределение IP ESP32:
        - ищем mDNS 'pcmonitor.local' (если включён)
        - или в локальной подсети (медленно, не делаем)
        """
        try:
            import socket as s
            # Попробуем mDNS имя
            for name in ("pcmonitor.local", "esp32.local", "esp.local"):
                try:
                    ip = s.gethostbyname(name)
                    if ip:
                        if config.DEBUG:
                            print(f"[TCP] auto-detected via mDNS: {name} → {ip}")
                        return ip
                except Exception:
                    continue
        except Exception:
            pass
        return None

    def send(self, payload: str):
        if self.sock is None:
            return False
        try:
            data = (payload + "\n").encode("utf-8")
            self.sock.sendall(data)
            self.last_ok = time.time()
            return True
        except Exception as e:
            if config.DEBUG:
                print(f"[TCP] write error: {e}")
            # Закрыть битый сокет
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None
            return False

    def is_alive(self):
        return self.sock is not None

    def close(self):
        if self.sock is not None:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None
