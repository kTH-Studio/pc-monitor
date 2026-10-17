#  PC Monitor v.5.0 — точка входа
#  by KTH-Studio
import time
import json
import threading
from queue import Queue, Empty

import config
from transport.link import LinkManager

# Датчики
from sensors import gpu, cpu, ram, disks, net, ping, leak, fps, media, notify

# Слушатель команд с ESP
from listener.commands import CommandListener


#  Состояние (общее для всех потоков)

class State:
    def __init__(self):
        self.lock    = threading.Lock()
        self.data    = {
            "gpu":    {},
            "cpu":    {},
            "ram":    {},
            "disks":  [],
            "net":    {},
            "ping":   {},
            "leak":   {},
            "fps":    {},
            "media":  {},
            "notify": [],
            "time":   int(time.time()),
            "tz":     config.TZ_OFFSET_HOURS,
        }
        self.running = True
        self.start_time = time.time()

    def set(self, key, value):
        with self.lock:
            self.data[key] = value

    def snapshot(self):
    with self.lock:
        self.data["time"] = int(time.time())
        self.data["tz"]   = config.TZ_OFFSET_HOURS
        return dict(self.data)

state = State()


#  Потоки датчиков

def run_sensor(name, fn, interval, key, *args):
    """Запускает функцию опроса в цикле с интервалом."""
    while state.running:
        try:
            value = fn(*args)
            if value is not None:
                state.set(key, value)
        except Exception as e:
            print(f"[{name}] error: {e}")
        time.sleep(interval)


def start_sensors():
    threads = [
        threading.Thread(target=run_sensor,
                         args=("GPU", gpu.read, config.INTERVAL_GPU, "gpu"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("CPU", cpu.read, config.INTERVAL_CPU, "cpu"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("RAM", ram.read, config.INTERVAL_RAM, "ram"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("DISKS", disks.read, config.INTERVAL_DISKS, "disks"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("NET", net.read, config.INTERVAL_NET, "net"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("PING", ping.read, config.INTERVAL_PING, "ping"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("LEAK", leak.read, config.INTERVAL_LEAK, "leak"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("FPS", fps.read, config.INTERVAL_FPS, "fps"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("MEDIA", media.read, config.INTERVAL_MEDIA, "media"),
                         daemon=True),
        threading.Thread(target=run_sensor,
                         args=("NOTIFY", notify.read, config.INTERVAL_NOTIFY, "notify"),
                         daemon=True),
    ]
    for t in threads:
        t.start()
    return threads



#  Поток отправки на ESP

def sender_loop(link):
    while state.running:
        try:
            data = state.snapshot()
            payload = json.dumps(data, ensure_ascii=False, separators=(',', ':'))
            link.send(payload)
        except Exception as e:
            print(f"[SEND] error: {e}")
        time.sleep(config.INTERVAL_SEND)



#  MAIN

def main():
    print("=" * 50)
    print("  PC Monitor v.5.0")
    print("  by KTH-Studio")
    print("=" * 50)

    # 1. Инициализация датчиков (LHM DLL и т.п.)
    from sensors import lhm
    lhm.init()

    # 2. Канал связи
    link = LinkManager()
    link.start()

    # 3. Слушатель команд с ESP
    listener = CommandListener(link)
    listener.start()

    # 4. Датчики
    start_sensors()

    # 5. Отправка
    sender = threading.Thread(target=sender_loop, args=(link,), daemon=True)
    sender.start()

    print("[MAIN] running. Ctrl+C to exit.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[MAIN] stopping...")
        state.running = False
        link.stop()
        listener.stop()
        time.sleep(0.5)


if __name__ == "__main__":
    main()
