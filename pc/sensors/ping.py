#  Пинг — subprocess ping.exe
#  by KTH-Studio
import re
import subprocess
import config

#  Состояние
_targets = config.PING_TARGETS
_history = {}      # host -> [ms, ms, ...] (для avg/min/max)
_last_result = {}  # host -> последний ms

# Регулярки для разбора вывода Windows ping
# Английский: "Reply from 8.8.8.8: bytes=32 time=12ms TTL=118"
# Русский:    "Ответ от 8.8.8.8: число байт=32 время=12мс TTL=118"
# Таймаут:    "Request timed out." / "Превышен интервал ожидания"
_RE_TIME_EN = re.compile(r"time[=<](\d+)ms", re.IGNORECASE)
_RE_TIME_RU = re.compile(r"время[=<](\d+)\s*мс", re.IGNORECASE)
_RE_TIME_LOOSE = re.compile(r"[=<](\d+)\s*(ms|мс)", re.IGNORECASE)
_RE_LOSS = re.compile(r"\((\d+)%", re.IGNORECASE)

#  Инициализация
_initialized = False

def _ensure():
    global _initialized
    if _initialized:
        return
    _initialized = True

#  Один пинг
def _ping_one(host, timeout_ms=1000):
    """
    Вернуть ms (int) или None (если таймаут).
    Windows: ping -n 1 -w <timeout_ms> <host>
    """
    try:
        result = subprocess.run(
            ["ping", "-n", "1", "-w", str(timeout_ms), host],
            capture_output=True,
            timeout=timeout_ms / 1000.0 + 1.0,
            creationflags=0x08000000,   # CREATE_NO_WINDOW
        )
        out = result.stdout.decode(errors="ignore")

        # Ищем время
        for rx in (_RE_TIME_EN, _RE_TIME_RU, _RE_TIME_LOOSE):
            m = rx.search(out)
            if m:
                return int(m.group(1))

        # Если "<1ms" — считаем 0
        if "<" in out and ("ms" in out.lower() or "мс" in out.lower()):
            return 0

        return None   # таймаут / ошибка

    except subprocess.TimeoutExpired:
        return None
    except Exception as e:
        if config.DEBUG:
            print(f"[PING] {host} error: {e}")
        return None

#  Чтение всех целей
def read():
    """
    Пингует все цели из config.PING_TARGETS.
    Возврат:
    {
      'present': True,
      'avg': 6,
      'count': 5,
      'targets': [
          {'name': 'Google', 'ms': 8},
          ...
      ]
    }
    """
    _ensure()

    targets_out = []
    values = []

    for t in _targets:
        name = t["name"]
        host = t["host"]

        ms = _ping_one(host, timeout_ms=int(config.PING_TIMEOUT_SEC * 1000))

        # В историю — только успешные
        if ms is not None:
            hist = _history.setdefault(host, [])
            hist.append(ms)
            if len(hist) > 60:
                hist.pop(0)
            values.append(ms)
            _last_result[host] = ms
        else:
            _last_result[host] = None

        targets_out.append({
            "name": name,
            "ms":   ms if ms is not None else -1,   # -1 = таймаут
        })

    # Средний по всем успешным за последний замер
    avg = 0
    if values:
        avg = int(round(sum(values) / len(values)))

    result = {
        "present": True,
        "avg":     avg,
        "count":   len(targets_out),
        "targets": targets_out,
    }

    if config.DEBUG:
        line = "  ".join(
            f"{t['name']}:{'--' if t['ms'] < 0 else str(t['ms']) + 'ms'}"
            for t in targets_out
        )
        print(f"[PING] avg={avg}ms  {line}")

    return result

#  Статистика за сессию (для отладки)
def stats():
    result = {}
    for host, hist in _history.items():
        if not hist:
            continue
        result[host] = {
            "min": min(hist),
            "max": max(hist),
            "avg": int(round(sum(hist) / len(hist))),
            "n":   len(hist),
        }
    return result

#  Отладка
def dump():
    _ensure()
    print("=== Ping targets ===")
    for t in _targets:
        print(f"  {t['name']:12s}  {t['host']}")

    print("\n--- Single read ---")
    r = read()
    for t in r["targets"]:
        ms = t["ms"] if t["ms"] >= 0 else "--"
        print(f"  {t['name']:12s}  {ms}")

    print(f"\navg = {r['avg']}ms")

    print("\n--- Session stats ---")
    for host, st in stats().items():
        print(f"  {host}: min={st['min']} max={st['max']} "
              f"avg={st['avg']} n={st['n']}")
