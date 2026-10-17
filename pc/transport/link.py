#  LinkManager — управление каналами связи с ESP32
#  by KTH-Studio
import time
import threading
import config

from .serial_link import SerialLink
from .tcp_link    import TcpLink
from .http_link   import HttpLink


class LinkManager:
    def __init__(self):
        self.lock       = threading.Lock()
        self.current    = None          # текущий канал
        self.current_id = ""            # "serial" / "tcp" / "http"
        self.links      = {
            "serial": SerialLink(),
            "tcp":    TcpLink(),
            "http":   HttpLink(),
        }
        self.running = False
        self.thread  = None
        self.last_ok = 0
        self.fail_count = 0

    #  Запуск / остановка    
    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._watch_loop, daemon=True)
        self.thread.start()
        self._try_connect_first()

    def stop(self):
        self.running = False
        with self.lock:
            if self.current:
                try:
                    self.current.close()
                except Exception:
                    pass
                self.current = None
        # Закрыть все
        for link in self.links.values():
            try:
                link.close()
            except Exception:
                pass

    #  Подключение    
    def _try_connect_first(self):
        """Пробует каналы в порядке приоритета."""
        for name in config.LINK_PRIORITY:
            link = self.links.get(name)
            if link is None:
                continue
            if link.connect():
                with self.lock:
                    self.current    = link
                    self.current_id = name
                print(f"[LINK] connected via {name.upper()}")
                return True
        print("[LINK] no channel available — running in offline mode")
        return False

    def _try_switch(self):
        """Пробует переключиться на следующий доступный канал."""
        order = config.LINK_PRIORITY
        try:
            start = order.index(self.current_id)
        except ValueError:
            start = -1

        # Проходим по кругу, начиная со следующего
        for i in range(1, len(order) + 1):
            idx = (start + i) % len(order)
            name = order[idx]
            if name == self.current_id:
                continue
            link = self.links.get(name)
            if link is None:
                continue
            if link.connect():
                # Закрыть старый
                with self.lock:
                    if self.current and self.current is not link:
                        try:
                            self.current.close()
                        except Exception:
                            pass
                    self.current    = link
                    self.current_id = name
                print(f"[LINK] switched to {name.upper()}")
                return True
        return False

    #  Отправка    
    def send(self, payload: str):
        """Отправить строку (JSON). При ошибке — переключиться."""
        with self.lock:
            link = self.current

        if link is None:
            # Нет канала — пробуем подключиться
            if self._try_connect_first():
                with self.lock:
                    link = self.current
            if link is None:
                return False

        try:
            ok = link.send(payload)
            if ok:
                self.last_ok = time.time()
                self.fail_count = 0
                return True
        except Exception as e:
            if config.DEBUG:
                print(f"[LINK] send error ({self.current_id}): {e}")

        # Ошибка — считаем провал
        self.fail_count += 1
        if self.fail_count >= 3:
            self.fail_count = 0
            print(f"[LINK] {self.current_id.upper()} failing, trying switch...")
            self._try_switch()
        return False

    #  Фоновый watchdog    
    def _watch_loop(self):
        while self.running:
            time.sleep(config.LINK_RETRY_SEC)

            with self.lock:
                link = self.current
                link_id = self.current_id

            # Проверяем «здоровье» текущего канала
            if link is None or not link.is_alive():
                print(f"[LINK] {link_id or 'none'} lost, reconnecting...")
                if not self._try_connect_first():
                    self._try_switch()

    #  Информация    
    def status(self):
        return {
            "channel":   self.current_id or "none",
            "last_ok":   self.last_ok,
            "fails":     self.fail_count,
        }
