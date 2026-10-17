#  Утечка памяти — psutil + история процессов
#  by KTH-Studio
import time
import config
import psutil

#  Состояние
# Для каждого процесса храним:
# {
#   pid: {
#     'name':       str,             # имя процесса
#     'base_mb':    float | None,    # базовый размер (после успокоения)
#     'calm_start': float | None,    # когда начали отслеживать окно спокойствия
#     'samples':    [(t, mb), ...],  # последние замеры (для определения спокойствия)
#     'first_seen': float,           # когда впервые увидели
#     'max_mb':     float,           # максимум за сессию
#     'seen':       bool,            # видели ли в последнем скане
#   }
# }
_tracked = {}
_initialized = False

#  Инициализация
def _ensure():
    global _initialized
    if _initialized:
        return
    _initialized = True

#  Чтение
def read():
    """
    Возврат:
    {
      'present': bool,        # есть ли утечка > LEAK_MIN_DELTA_MB
      'max_delta_mb': int,    # максимальный прирост среди топ
      'items': [
          {'name': 'chrome', 'delta_mb': 245},
          ...
      ]
    }
    """
    _ensure()

    now = time.time()

    # Помечаем все как "не видели"
    for pid_data in _tracked.values():
        pid_data["seen"] = False

    # Скан всех процессов
    for proc in psutil.process_iter(["pid", "name", "memory_info"]):
        try:
            info = proc.info
            pid = info["pid"]
            name = info["name"] or "?"
            mem_info = info.get("memory_info")
            if mem_info is None:
                continue
            mem_mb = mem_info.rss / (1024 * 1024)

        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue
        except Exception:
            continue

        # Пропускаем системные и служебные
        if _skip_process(name):
            continue

        # Ищем процесс в трекере
        data = _tracked.get(pid)

        # Если PID переиспользован (новый процесс с тем же PID) —
        # сбрасываем
        if data is not None and data["name"] != name:
            data = None

        if data is None:
            # Новый процесс — начинаем отслеживать
            _tracked[pid] = {
                "name":       name,
                "base_mb":    None,
                "samples":    [(now, mem_mb)],
                "first_seen": now,
                "max_mb":     mem_mb,
                "seen":       True,
            }
            continue

        # Обновляем
        data["seen"] = True
        data["samples"].append((now, mem_mb))
        if mem_mb > data["max_mb"]:
            data["max_mb"] = mem_mb

        # Оставляем только последние 60 секунд истории
        cutoff = now - 60
        data["samples"] = [(t, m) for (t, m) in data["samples"] if t >= cutoff]

        # Определение "успокоения"
        if data["base_mb"] is None:
            _check_calm(pid, data, now)

    # Удаляем невидимые процессы (были >60 сек назад и не вернулись)
    dead = [pid for pid, d in _tracked.items()
            if not d["seen"] and (now - d["first_seen"]) > 60]
    for pid in dead:
        _tracked.pop(pid, None)

    # Сбор топа
    items = []
    max_delta = 0

    for pid, data in _tracked.items():
        if data["base_mb"] is None:
            continue

        # Текущий размер — последний замер
        if not data["samples"]:
            continue
        current_mb = data["samples"][-1][1]

        delta = current_mb - data["base_mb"]
        if delta < config.LEAK_MIN_DELTA_MB:
            continue

        items.append({
            "name":     _trim_name(data["name"]),
            "delta_mb": int(round(delta)),
        })
        if delta > max_delta:
            max_delta = delta

    # Сортировка по убыванию и обрезка до топ-N
    items.sort(key=lambda x: -x["delta_mb"])
    items = items[:config.LEAK_TOP_N]

    present = len(items) > 0

    result = {
        "present":      present,
        "max_delta_mb": int(round(max_delta)) if present else 0,
        "items":        items,
    }

    if config.DEBUG and present:
        line = "  ".join(f"{it['name']}:+{it['delta_mb']}MB" for it in items)
        print(f"[LEAK] max={result['max_delta_mb']}MB  {line}")

    return result
    
#  Определение "успокоения"
def _check_calm(pid, data, now):
    """
    Проверяем, успокоился ли процесс.
    Условие: за последние LEAK_CALM_WINDOW_SEC секунд
             прирост < LEAK_CALM_THRESHOLD_MB.
    Если да — фиксируем base_mb как текущий размер.
    """
    samples = data["samples"]
    if len(samples) < 2:
        return

    window = config.LEAK_CALM_WINDOW_SEC
    threshold = config.LEAK_CALM_THRESHOLD_MB

    # Находим точку "window секунд назад"
    start_time = now - window
    window_samples = [(t, m) for (t, m) in samples if t >= start_time]

    # Если данных за окно меньше, чем достаточно (например, только что стартанули) —
    # проверяем, сколько времени вообще наблюдаем
    time_span = now - data["first_seen"]
    if time_span < window:
        # Ещё не прошло полное окно — проверяем по всей доступной истории
        if len(samples) < 2:
            return
        mn = min(m for (_, m) in samples)
        mx = max(m for (_, m) in samples)
        if (mx - mn) < threshold:
            # Уже стабилен — фиксируем базу
            data["base_mb"] = samples[-1][1]
        return

    # Полное окно прошло — проверяем прирост внутри окна
    if len(window_samples) < 2:
        return

    mn = min(m for (_, m) in window_samples)
    mx = max(m for (_, m) in window_samples)

    if (mx - mn) < threshold:
        # Спокойствие — фиксируем базу
        data["base_mb"] = samples[-1][1]

#  Хелперы
_SKIP_NAMES = {
    # Системные процессы Windows
    "system", "idle", "registry", "memory compression",
    "system interrupts", "smss.exe", "csrss.exe", "wininit.exe",
    "winlogon.exe", "services.exe", "lsass.exe", "svchost.exe",
    "fontdrvhost.exe", "dwm.exe", "taskhostw.exe",
    "runtimebroker.exe", "sihost.exe", "ctfmon.exe",
    "searchindexer.exe", "searchhost.exe", "startmenuexperiencehost.exe",
    "shellexperiencehost.exe", "textinputhost.exe", "securityhealthsystray.exe",
    "securityhealthservice.exe", "spoolsv.exe", "audiodg.exe",

    # Наши процессы
    "python.exe", "pythonw.exe", "pc_monitor",
}

def _skip_process(name):
    if not name:
        return True
    n = name.lower()
    if n in _SKIP_NAMES:
        return True
    # Пропускаем всё, что явно системное
    if n.startswith("windows") and "app" not in n:
        return True
    return False

def _trim_name(name):
    """Имя процесса для экрана — до 14 символов."""
    if not name:
        return "?"
    # Убираем .exe
    if name.lower().endswith(".exe"):
        name = name[:-4]
    if len(name) > 14:
        name = name[:14]
    return name

#  Отладка
def dump():
    _ensure()
    print("=== Tracked processes ===")
    print(f"Total tracked: {len(_tracked)}")
    print()

    now = time.time()
    rows = []
    for pid, d in _tracked.items():
        current = d["samples"][-1][1] if d["samples"] else 0
        base = d["base_mb"]
        delta = (current - base) if base is not None else None

        rows.append({
            "pid":    pid,
            "name":   d["name"],
            "base":   base,
            "current": current,
            "delta":  delta,
            "max":    d["max_mb"],
            "age_s":  int(now - d["first_seen"]),
        })

    # Сортировка: с базой и приростом — первыми
    rows.sort(key=lambda r: (-(r["delta"] or -1), -(r["current"] or 0)))

    for r in rows[:20]:
        base_str = f"{r['base']:.0f}" if r["base"] is not None else "--"
        delta_str = f"{r['delta']:+.0f}" if r["delta"] is not None else "--"
        print(f"  PID {r['pid']:6d}  {r['name']:25s}  "
              f"base={base_str:>7s}MB  cur={r['current']:>7.0f}MB  "
              f"delta={delta_str:>7s}MB  max={r['max']:>7.0f}MB  "
              f"age={r['age_s']}s")

    print()
    print("=== Result ===")
    print(read())
