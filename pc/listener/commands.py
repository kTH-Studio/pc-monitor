#  CommandListener — приём команд с ESP32
#  by KTH-Studio
import json
import threading
import time
import config

from actions import power, media_control, reset


class CommandListener:
    def __init__(self, link_manager):
        self.link   = link_manager
        self.running = False
        self.thread = None
    
    #  Старт / стоп    
    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()

    def stop(self):
        self.running = False

    #  Основной цикл — читаем с активного канала    
    def _loop(self):
        while self.running:
            try:
                line = self._read_line()
                if line:
                    self._dispatch(line)
            except Exception as e:
                if config.DEBUG:
                    print(f"[LISTEN] error: {e}")
            time.sleep(0.05)

    def _read_line(self):
        """Читает одну строку с текущего канала."""
        link = self.link.current
        if link is None:
            return None

        link_id = self.link.current_id

        # Serial
        if link_id == "serial":
            ser = getattr(link, "ser", None)
            if ser is None or not ser.is_open:
                return None
            try:
                if ser.in_waiting > 0:
                    line = ser.readline()
                    return line.decode("utf-8", errors="ignore").strip()
            except Exception:
                return None

        # TCP
        elif link_id == "tcp":
            sock = getattr(link, "sock", None)
            if sock is None:
                return None
            try:
                sock.settimeout(0.1)
                data = sock.recv(4096)
                if data:
                    return data.decode("utf-8", errors="ignore").strip()
            except Exception:
                return None

        # HTTP — приём невозможен
        else:
            return None

        return None

    #  Разбор и выполнение    
    def _dispatch(self, line):
        if not line:
            return

        # Может быть несколько строк в одном recv
        for raw in line.split("\n"):
            raw = raw.strip()
            if not raw:
                continue
            try:
                cmd = json.loads(raw)
            except json.JSONDecodeError:
                if config.DEBUG:
                    print(f"[LISTEN] bad json: {raw}")
                continue

            self._handle(cmd)

    def _handle(self, cmd):
        c = cmd.get("cmd", "")
        if config.DEBUG:
            print(f"[LISTEN] rx: {cmd}")

        # Питание
        if c == "power":
            action = cmd.get("action", "")
            if action == "cancel":
                power.cancel()
            else:
                power.execute(action)

        # Медиа
        elif c == "media":
            action = cmd.get("action", "")
            media_control.execute(action)

        # Язык (синхронизация с ESP → ПК)
        elif c == "lang":
            value = cmd.get("value", 0)
            # Тут можно переключить локаль Python (если нужно)
            if config.DEBUG:
                print(f"[LISTEN] lang set to {value}")

        # Сброс счётчиков
        elif c == "reset":
            what = cmd.get("what", "counters")
            if what == "all":
                reset.all()
            else:
                reset.counters()

        # Перезапуск ESP (не наша забота, но пусть будет)
        elif c == "esp_restart":
            if config.DEBUG:
                print("[LISTEN] ESP restart requested (ignoring)")

        else:
            if config.DEBUG:
                print(f"[LISTEN] unknown cmd: {c}")
