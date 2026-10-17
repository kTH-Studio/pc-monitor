#  SerialLink — отправка по COM-порту
#  by KTH-Studio
import time
import config


class SerialLink:
    def __init__(self):
        self.ser     = None
        self.port    = config.ESP_COM_PORT
        self.baud    = config.ESP_BAUD
        self.last_ok = 0

    def connect(self):
        if self.ser is not None and self.ser.is_open:
            return True

        try:
            import serial
        except ImportError:
            if config.DEBUG:
                print("[SERIAL] pyserial not installed")
            return False

        # Если COM не задан — попробуем автоопределить
        port = self.port
        if not port:
            port = self._auto_detect()
            if not port:
                return False

        try:
            self.ser = serial.Serial(
                port=port,
                baudrate=self.baud,
                timeout=1.0,
                write_timeout=1.0,
            )
            time.sleep(0.5)   # ESP32 перезагружается при открытии порта
            # Сбросить буфер
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            if config.DEBUG:
                print(f"[SERIAL] opened {port}")
            return True
        except Exception as e:
            if config.DEBUG:
                print(f"[SERIAL] open error ({port}): {e}")
            self.ser = None
            return False

    def _auto_detect(self):
        """Ищем порт с описанием 'CH340', 'CP210x', 'Silicon Labs' или 'ESP'."""
        try:
            from serial.tools import list_ports
        except ImportError:
            return None

        candidates = []
        for p in list_ports.comports():
            desc = (p.description or "").lower()
            hwid = (p.hwid or "").lower()
            # Типичные USB-UART чипы на ESP32
            if any(k in desc or k in hwid for k in (
                "ch340", "ch341", "cp210", "silicon labs",
                "ft232", "esp32", "espressif",
            )):
                candidates.append(p.device)

        if candidates:
            if config.DEBUG:
                print(f"[SERIAL] auto-detected: {candidates[0]}")
            return candidates[0]
        return None

    def send(self, payload: str):
        if self.ser is None or not self.ser.is_open:
            return False
        try:
            data = (payload + "\n").encode("utf-8")
            self.ser.write(data)
            self.ser.flush()
            self.last_ok = time.time()
            return True
        except Exception as e:
            if config.DEBUG:
                print(f"[SERIAL] write error: {e}")
            return False

    def is_alive(self):
        if self.ser is None or not self.ser.is_open:
            return False
        # Если давно не отправляли — попробуем записать пустышку?
        # Нет, просто проверяем порт
        return True

    def close(self):
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None
