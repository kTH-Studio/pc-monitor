#  Сеть — psutil + socket
#  by KTH-Studio
import time
import socket
import config
import psutil

#  Состояние
_last_time = 0
_last_rx   = 0
_last_tx   = 0
_start_rx  = 0
_start_tx  = 0

_iface_name = ""
_iface_ip   = ""
_link_speed = 0     # Mbit/s

#  Инициализация
_initialized = False

def _ensure():
    global _initialized, _start_rx, _start_tx, _last_rx, _last_tx, _last_time

    if _initialized:
        return

    # Первое чтение счётчиков — базовые значения
    counters = psutil.net_io_counters(pernic=False)
    _start_rx = counters.bytes_recv
    _start_tx = counters.bytes_sent
    _last_rx  = counters.bytes_recv
    _last_tx  = counters.bytes_sent
    _last_time = time.time()

    _detect_iface()
    _initialized = True

#  Определение активного адаптера
def _detect_iface():
    """
    Ищем активный Ethernet или Wi-Fi адаптер:
    - с реальным IP (не 127.0.0.1, не 169.254.*)
    - с ненулевым трафиком
    """
    global _iface_name, _iface_ip, _link_speed

    try:
        stats = psutil.net_if_stats()
        addrs = psutil.net_if_addrs()
        counters = psutil.net_io_counters(pernic=True)
    except Exception as e:
        if config.DEBUG:
            print(f"[NET] iface detect error: {e}")
        return

    best = None

    for name, st in stats.items():
        # Только активные
        if not st.isup:
            continue
        # Пропускаем Loopback, VPN-виртуальные и т.п.
        name_l = name.lower()
        if any(skip in name_l for skip in (
            "loopback", "virtual", "vmware", "vbox", "hyper-v",
            "bluetooth", "tunnel", "tap", "wintun"
        )):
            continue

        # Ищем IPv4
        ipv4 = None
        for a in addrs.get(name, []):
            if a.family == socket.AF_INET:
                ip = a.address
                if ip.startswith("127.") or ip.startswith("169.254."):
                    continue
                ipv4 = ip
                break

        if not ipv4:
            continue

        # Трафик
        c = counters.get(name)
        traffic = (c.bytes_recv + c.bytes_sent) if c else 0

        # Приоритет: Ethernet > Wi-Fi > прочее
        score = 0
        if "ethernet" in name_l or "eth" in name_l:
            score = 100
        elif "wi-fi" in name_l or "wifi" in name_l or "wlan" in name_l:
            score = 50

        # Трафик как вторичный критерий
        score += min(traffic // (1024 * 1024), 40)

        if best is None or score > best["score"]:
            best = {
                "name":    name,
                "ip":      ipv4,
                "speed":   st.speed,     # Mbit/s
                "score":   score,
            }

    if best:
        _iface_name = best["name"]
        _iface_ip   = best["ip"]
        _link_speed = best["speed"]
        if config.DEBUG:
            print(f"[NET] iface: {_iface_name} ({_iface_ip}) "
                  f"link={_link_speed}Mbit/s")

#  Чтение
def read():
    """
    Вернуть dict с данными сети.
    """
    _ensure()
    global _last_rx, _last_tx, _last_time

    # Если адаптер не нашли — пробуем ещё раз
    if not _iface_name:
        _detect_iface()
        if not _iface_name:
            return {"present": False}

    # Счётчики (общие + по адаптеру)
    now = time.time()
    dt  = now - _last_time

    try:
        per = psutil.net_io_counters(pernic=True)
        counters = per.get(_iface_name)
        if counters is None:
            counters = psutil.net_io_counters(pernic=False)
    except Exception as e:
        if config.DEBUG:
            print(f"[NET] counters error: {e}")
        return {"present": False}

    rx = counters.bytes_recv
    tx = counters.bytes_sent

    # Скорости (Mbit/s)
    down_mbit = 0.0
    up_mbit   = 0.0
    if dt > 0:
        d_rx = rx - _last_rx
        d_tx = tx - _last_tx
        # Байты → биты → Мбит
        down_mbit = (d_rx * 8) / (dt * 1_000_000)
        up_mbit   = (d_tx * 8) / (dt * 1_000_000)

    _last_rx = rx
    _last_tx = tx
    _last_time = now

    # Округление: если мало — 0, если больше 1 — до 0.1
    down_mbit = _round_speed(down_mbit)
    up_mbit   = _round_speed(up_mbit)

    # Всего скачано/отдано с запуска монитора
    total_down_gb = round((rx - _start_rx) / (1024 ** 3), 2)
    total_up_gb   = round((tx - _start_tx) / (1024 ** 3), 2)

    # Загрузка канала (%)
    link = _link_speed if _link_speed > 0 else 100
    channel_load = int(round((down_mbit + up_mbit) * 100.0 / link))
    if channel_load > 100:
        channel_load = 100

    result = {
        "present":       True,
        "adapter":       _iface_name,
        "down_mbit":     down_mbit,
        "up_mbit":       up_mbit,
        "total_down_gb": total_down_gb,
        "total_up_gb":   total_up_gb,
        "ip":            _iface_ip,
        "link_speed":    _link_speed,
        "channel_load":  channel_load,
    }

    if config.DEBUG:
        print(f"[NET] {_iface_name} ({_iface_ip}): "
              f"v{down_mbit:.1f} ^{up_mbit:.1f} Mbit/s "
              f"total {total_down_gb}/{total_up_gb}GB "
              f"load {channel_load}%")

    return result

#  Хелпер: округление скорости
def _round_speed(v):
    if v < 0.05:
        return 0.0
    if v < 10:
        return round(v, 1)
    return round(v)

#  Отладка
def dump():
    _ensure()
    print("=== Interfaces ===")
    stats = psutil.net_if_stats()
    addrs = psutil.net_if_addrs()
    counters = psutil.net_io_counters(pernic=True)

    for name, st in stats.items():
        ipv4 = ""
        for a in addrs.get(name, []):
            if a.family == socket.AF_INET:
                ipv4 = a.address
                break
        c = counters.get(name)
        traffic = (c.bytes_recv + c.bytes_sent) if c else 0
        print(f"  {name:30s}  up={st.isup}  speed={st.speed}Mbit/s  "
              f"ip={ipv4}  traffic={traffic / (1024**3):.2f}GB")

    print(f"\n=== Selected: {_iface_name} ({_iface_ip}) link={_link_speed} ===")

    # Второе чтение — покажет скорость
    time.sleep(1)
    print(read())
