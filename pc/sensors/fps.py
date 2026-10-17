#  FPS — RTSS shared memory
#  by KTH-Studio
import ctypes
import time
import config

#  Структуры RTSS
# Формат: "RTSSSharedMemoryV2"
# Данные лежат в Windows named shared memory.
RTSS_SHARED_MEMORY_NAME = "RTSSSharedMemoryV2"

# Магия (первые 4 байта структуры)
RTSS_MAGIC = 0x52545353  # 'RTSS'

# Смещения/размеры (из исходников RTSS)
RTSS_APP_ENTRY_SIZE      = 0x14C   # размер одной записи (примерно)
RTSS_APP_MAX             = 256     # максимум приложений
RTSS_APP_HEADER_SIZE     = 8       # заголовок shared memory
RTSS_APP_ENTRY_OFFSET    = 0x10    # смещение массива записей

#  Состояние
_mm          = None
_view        = None
_initialized = False

# История FPS: {pid: [fps, fps, ...]}
_history = {}

#  Инициализация
def _ensure():
    global _mm, _view, _initialized
    if _initialized:
        return True

    try:
        import mmap
        _mm = mmap.mmap(
            -1,
            0,
            tagname=RTSS_SHARED_MEMORY_NAME,
            access=mmap.ACCESS_READ,
        )
        _view = memoryview(_mm)
        _initialized = True
        if config.DEBUG:
            print("[FPS] RTSS shared memory opened")
        return True
    except Exception as e:
        if config.DEBUG:
            print(f"[FPS] RTSS not available: {e}")
        _initialized = False
        return False

#  Парсинг
def _read_u32(offset):
    return int.from_bytes(_view[offset:offset + 4], "little")


def _read_u64(offset):
    return int.from_bytes(_view[offset:offset + 8], "little")


def _read_str(offset, max_len=260):
    """Читает null-terminated ASCII/UTF-8 строку."""
    end = offset
    limit = offset + max_len
    while end < limit and _view[end] != 0:
        end += 1
    try:
        return bytes(_view[offset:end]).decode("utf-8", errors="ignore")
    except Exception:
        return ""


def _find_active_app():
    """
    Ищет активную игру в RTSS shared memory.
    Возврат: dict с данными или None.
    """
    if not _ensure():
        return None

    # Проверка магического числа
    magic = _read_u32(0)
    if magic != RTSS_MAGIC:
        if config.DEBUG:
            print(f"[FPS] bad magic: {hex(magic)}")
        return None

    # Количество записей
    app_count = _read_u32(4)   # dwAppEntrySize
    # ... структура сложная, но для практики:
    # смещение массива записей = 0x10, размер = app_count

    # Ищем самую свежую активную запись
    best = None
    now_ms = int(time.time() * 1000)

    for i in range(RTSS_APP_MAX):
        offset = RTSS_APP_ENTRY_OFFSET + i * RTSS_APP_ENTRY_SIZE
        if offset + RTSS_APP_ENTRY_SIZE > len(_view):
            break

        # Сигнатура записи — 'RTSS' в начале?
        # В RTSS каждая запись начинается с dwSignature = 'RTSS'
        sig = _read_u32(offset)
        # В некоторых версиях записи не имеют сигнатуры, а идут подряд

        # Читаем PID (смещение 4)
        pid = _read_u32(offset + 4)

        if pid == 0:
            continue

        # Читаем имя приложения (обычно по смещению 0x24, до 260 байт)
        # Смещения в RTSS:
        #   0x00 — dwSignature
        #   0x04 — dwVersion
        #   0x08 — dwAppEntrySize
        #   0x0C — dwAppArrOffset
        #   0x10 — dwAppArrSize
        #   0x14 — dwOSDEntrySize
        #   0x18 — dwOSDArrOffset
        #   0x1C — dwOSDArrSize
        #   0x20 — dwOSDFrame
        #   0x24 — dwBusy
        #   0x28 — dwFrame
        #   0x2C — dwFrameTime
        #   0x30 — dwFramesPerSecond (???) — зависит от версии

        # Упрощённый подход: ищем непустой FPS и имя
        # На практике структура:
        #   +0x24  szName[260]
        #   ...
        #   +0x140 dwFramesPerSecond (по факту вычисляется)

        # Возьмём более надёжные поля (согласно RTSS SDK):
        # Каждая APP-запись:
        #   DWORD dwPID;
        #   char  szName[260];
        #   ...
        #   DWORD dwFrames;
        #   DWORD dwFrameTime;
        #   ...

        # Из-за разных версий RTSS — используем эмпирический подход.
        # Ключевые поля (по опыту):
        #   +0x04: PID
        #   +0x24: name (260 bytes)
        #   +0x144: FPS
        #   +0x148: frametime (us)

        fps      = _read_u32(offset + 0x144)
        frametime_us = _read_u32(offset + 0x148)
        name     = _read_str(offset + 0x24, 260)

        if fps == 0 or not name:
            continue

        # Фильтруем не-игры
        if _skip_app(name):
            continue

        # Обновляем "лучшую" запись (свежая по frame time)
        if best is None or frametime_us < best["frametime_us"]:
            best = {
                "pid":          pid,
                "name":         name,
                "fps":          fps,
                "frametime_us": frametime_us,
            }

    return best

#  Чтение
def read():
    """
    Возврат:
    {
      'present': bool,
      'game':    str,
      'current': int,
      'min':     int,
      'max':     int,
      'drop_pct': int,
      'gpu_load': int,
      'cpu_load': int,
    }
    """
    _ensure()

    if not _initialized:
        return {"present": False}

    app = _find_active_app()
    if app is None:
        return {"present": False}

    pid  = app["pid"]
    fps  = app["fps"]
    name = app["name"]

    # Обрезаем имя (убираем .exe)
    if name.lower().endswith(".exe"):
        name = name[:-4]
    if len(name) > 40:
        name = name[:40]

    # История для min/max
    hist = _history.setdefault(pid, [])
    hist.append(fps)
    if len(hist) > 300:    # ~5 минут при опросе 1/сек
        hist.pop(0)

    min_fps = min(hist) if hist else fps
    max_fps = max(hist) if hist else fps

    # Просадка = 1% low (приблизительно)
    # Считаем как (max - текущий) / max * 100
    if max_fps > 0:
        drop_pct = int(round((max_fps - fps) * 100 / max_fps))
        if drop_pct < 0:
            drop_pct = 0
        if drop_pct > 100:
            drop_pct = 100
    else:
        drop_pct = 0

    # Загрузки GPU и CPU (из общей шины, для контекста)
    gpu_load = 0
    cpu_load = 0
    try:
        from sensors import gpu, cpu
        g = gpu.read()
        if g.get("present"):
            gpu_load = g.get("load", 0)
        c = cpu.read()
        if c.get("present"):
            cpu_load = c.get("load", 0)
    except Exception:
        pass

    result = {
        "present":  True,
        "game":     name,
        "current":  int(fps),
        "min":      int(min_fps),
        "max":      int(max_fps),
        "drop_pct": drop_pct,
        "gpu_load": gpu_load,
        "cpu_load": cpu_load,
    }

    if config.DEBUG:
        print(f"[FPS] {name}: {fps}fps  "
              f"min={min_fps} max={max_fps} drop={drop_pct}%  "
              f"GPU={gpu_load}% CPU={cpu_load}%")

    return result

#  Фильтр приложений
_SKIP_APPS = {
    "dwm", "explorer", "rtss", "rivatuner",
    "afterburner", "msiafterburner",
    "obs64", "obs32", "obs", "streamlabs",
    "discord", "chrome", "firefox", "msedge",
    "steam", "steamwebhelper",
}

def _skip_app(name):
    if not name:
        return True
    n = name.lower()
    # Убираем .exe для проверки
    if n.endswith(".exe"):
        n = n[:-4]
    if n in _SKIP_APPS:
        return True
    return False

#  Закрытие
def close():
    global _mm, _view, _initialized
    try:
        if _view:
            _view.release()
        if _mm:
            _mm.close()
    except Exception:
        pass
    _mm = None
    _view = None
    _initialized = False

#  Отладка
def dump():
    if not _ensure():
        print("[FPS] RTSS недоступен. Установлен ли RTSS? Запущен?")
        return

    print(f"=== RTSS shared memory ({len(_view)} bytes) ===")
    print(f"  magic: {hex(_read_u32(0))} (ожидается 0x52545353)")

    for i in range(RTSS_APP_MAX):
        offset = RTSS_APP_ENTRY_OFFSET + i * RTSS_APP_ENTRY_SIZE
        if offset + RTSS_APP_ENTRY_SIZE > len(_view):
            break

        pid = _read_u32(offset + 4)
        if pid == 0:
            continue

        name = _read_str(offset + 0x24, 260)
        fps  = _read_u32(offset + 0x144)
        ft   = _read_u32(offset + 0x148)

        print(f"  #{i:3d}  PID={pid:6d}  {name:30s}  "
              f"FPS={fps:4d}  frame={ft}us")

    print("\n=== Active app ===")
    app = _find_active_app()
    if app:
        print(f"  {app}")
    else:
        print("  нет активной игры")

    print("\n=== Result ===")
    print(read())
