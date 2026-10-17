#  GPU — чтение через LHM
#  by KTH-Studio
import config
from sensors import lhm

#  Состояние
_gpu = None
_gpu_name = ""
_driver = ""

#  Инициализация (при первом вызове)
def _ensure():
    global _gpu, _gpu_name, _driver
    if _gpu is not None:
        return True

    _gpu = lhm.get_gpu()
    if _gpu is None:
        return False

    _gpu_name = lhm.get_gpu_name(_gpu)
    _driver   = _find_driver()
    return True


def _find_driver():
    """
    Версия драйвера — из сенсоров LHM её нет напрямую.
    Читаем из реестра Windows.
    """
    try:
        import winreg
        keys = [
            (winreg.HKEY_LOCAL_MACHINE,
             r"SOFTWARE\NVIDIA Corporation\Global\NvSMI"),
            (winreg.HKEY_LOCAL_MACHINE,
             r"SOFTWARE\AMD\CN"),
        ]
        # Для NVIDIA — через WMI проще
        try:
            import subprocess
            out = subprocess.check_output(
                ["wmic", "path", "win32_videocontroller", "get",
                 "DriverVersion", "/format:list"],
                stderr=subprocess.DEVNULL,
                creationflags=0x08000000  # CREATE_NO_WINDOW
            ).decode(errors="ignore")

            for line in out.splitlines():
                line = line.strip()
                if line.startswith("DriverVersion="):
                    v = line.split("=", 1)[1].strip()
                    if v:
                        return _short_driver(v)
        except Exception:
            pass
    except Exception:
        pass
    return ""


def _short_driver(v):
    """
    NVIDIA: '31.0.15.5222' → '552.22'
    AMD:    '31.0.24027.1012' → '24.10.12' (или как есть)
    """
    if not v:
        return ""
    parts = v.split(".")
    # NVIDIA — последние 5 цифр = версия
    if len(parts) >= 5:
        # 31.0.15.5222 → склеиваем 15.5222 → 155222 → 552.22
        last = parts[-2] + parts[-1]
        if len(last) >= 5:
            return last[-5:-3] + "." + last[-3:]
    # Фолбэк — как есть, но обрезаем
    return v[:12]

#  Чтение
def read():
    """
    Вернуть dict с данными GPU для отправки на ESP.
    Если GPU нет — {'present': False}.
    """
    if not _ensure():
        return {"present": False}

    # Обновить датчики (иначе значения застынут)
    try:
        _gpu.Update()
    except Exception as e:
        if config.DEBUG:
            print(f"[GPU] update error: {e}")

    # Загрузка
    load = lhm.find_sensor(_gpu, "Load", "GPU Core")
    if load is None:
        load = lhm.find_sensor(_gpu, "Load", "D3D")
    load = int(round(load)) if load is not None else 0

    # Температура
    # NVIDIA: "GPU Core"
    # AMD:    "GPU Core" или "GPU VR SoC"
    # Intel:  "GPU Core"
    temp = lhm.find_sensor(_gpu, "Temperature", "GPU Core")
    if temp is None:
        temp = lhm.find_sensor(_gpu, "Temperature", "GPU")
    temp = int(round(temp)) if temp is not None else 0

    # VRAM
    # Используем Data-сенсоры (МБ), переводим в ГБ
    vram_used_mb  = lhm.find_sensor(_gpu, "SmallData", "GPU Memory Used")
    if vram_used_mb is None:
        vram_used_mb = lhm.find_sensor(_gpu, "Data", "GPU Memory Used")

    vram_total_mb = lhm.find_sensor(_gpu, "SmallData", "GPU Memory Total")
    if vram_total_mb is None:
        vram_total_mb = lhm.find_sensor(_gpu, "Data", "GPU Memory Total")

    vram_used  = round(vram_used_mb  / 1024.0, 1) if vram_used_mb  is not None else 0.0
    vram_total = round(vram_total_mb / 1024.0, 1) if vram_total_mb is not None else 0.0

    # Питание
    power = lhm.find_sensor(_gpu, "Power", "GPU Package")
    if power is None:
        power = lhm.find_sensor(_gpu, "Power", "GPU Power")
    power = int(round(power)) if power is not None else 0

    # Кулер
    # Ищем первый сенсор Fan
    fans = lhm.find_all_sensors(_gpu, "Fan")
    fan = int(round(fans[0]["value"])) if fans else 0

    # Дополнительно (для отладки, не обязательно на экран)
    hotspot    = lhm.find_sensor(_gpu, "Temperature", "Hot Spot")
    core_clock = lhm.find_sensor(_gpu, "Clock", "GPU Core")
    mem_clock  = lhm.find_sensor(_gpu, "Clock", "GPU Memory")
    mem_temp   = lhm.find_sensor(_gpu, "Temperature", "Memory Junction")

    result = {
        "present":    True,
        "name":       _gpu_name,
        "load":       load,
        "temp":       temp,
        "vram_used":  vram_used,
        "vram_total": vram_total,
        "power":      power,
        "fan":        fan,
        "driver":     _driver,
        # расширенные (не обязательно на OLED):
        "hotspot":    int(round(hotspot)) if hotspot is not None else None,
        "core_clock": int(round(core_clock)) if core_clock is not None else None,
        "mem_clock":  int(round(mem_clock))  if mem_clock  is not None else None,
        "mem_temp":   int(round(mem_temp))   if mem_temp   is not None else None,
    }

    if config.DEBUG:
        print(f"[GPU] {_gpu_name}: {load}% {temp}C "
              f"{vram_used}/{vram_total}GB {power}W {fan}RPM")

    return result

#  Отладка — вывести все сенсоры GPU
def dump():
    if not _ensure():
        print("[GPU] not found")
        return
    print(f"=== GPU: {_gpu_name} ===")
    for s in lhm.list_sensors(_gpu):
        print(f"  {s['type']:12s}  {s['name']:30s}  {s['value']}")
