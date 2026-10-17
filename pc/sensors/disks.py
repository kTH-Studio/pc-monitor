#  Диски — LHM + psutil + WMI
#  by KTH-Studio
import os
import config
import psutil
from sensors import lhm

#  Состояние
_initialized = False

# Кэш WMI-инфы по букве диска: {'C': {'name': 'Samsung 980 Pro', 'type': 'NVMe', 'max_speed': 7000}}
_wmi_cache = {}

#  Инициализация
def _ensure():
    global _initialized
    if _initialized:
        return
    _load_wmi_info()
    _initialized = True

#  WMI — модель и тип диска
def _load_wmi_info():
    """
    Заполняет _wmi_cache:
        { 'C': {'name': 'Samsung SSD 980 PRO 500GB', 'type': 'NVMe', 'max_speed': 7000} }
    """
    global _wmi_cache

    try:
        import subprocess

        # Физические диски: модель, интерфейс, размер
        out = subprocess.check_output(
            ["wmic", "diskdrive", "get",
             "Model,InterfaceType,MediaType,Size,Index",
             "/format:list"],
            stderr=subprocess.DEVNULL,
            creationflags=0x08000000,
        ).decode(errors="ignore")

        physical = []
        cur = {}
        for line in out.splitlines():
            line = line.strip()
            if not line:
                if cur:
                    physical.append(cur)
                    cur = {}
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                cur[k.strip()] = v.strip()
        if cur:
            physical.append(cur)

        # Логические диски → физические (через partition)
        # Проще: сопоставить по индексу диска (Disk Index)
        out2 = subprocess.check_output(
            ["wmic", "partition", "get",
             "DiskIndex,DeviceID",
             "/format:list"],
            stderr=subprocess.DEVNULL,
            creationflags=0x08000000,
        ).decode(errors="ignore")

        parts = []
        cur = {}
        for line in out2.splitlines():
            line = line.strip()
            if not line:
                if cur:
                    parts.append(cur)
                    cur = {}
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                cur[k.strip()] = v.strip()
        if cur:
            parts.append(cur)

        # Строим карту: буква → физический диск
        for p in parts:
            dev = p.get("DeviceID", "")        # "\\\\.\\C:"
            idx = p.get("DiskIndex", "")
            if not dev or not idx.isdigit():
                continue

            letter = dev.replace("\\\\", "").replace(".", "").replace("\\", "").strip(":")
            # dev обычно "\\\\.\\C:" → "C"

            disk_idx = int(idx)
            if disk_idx < len(physical):
                pd = physical[disk_idx]
                model   = pd.get("Model", "").strip()
                iface   = pd.get("InterfaceType", "").strip()
                media   = pd.get("MediaType", "").strip()

                # Определяем тип
                dtype = _detect_type(iface, media, model)

                # Максимальная скорость по типу
                max_speed = _max_speed_by_type(dtype, iface, model)

                _wmi_cache[letter.upper()] = {
                    "name":      _shorten_name(model),
                    "type":      dtype,
                    "max_speed": max_speed,
                }

    except Exception as e:
        if config.DEBUG:
            print(f"[DISKS] WMI error: {e}")


def _detect_type(iface, media, model):
    """NVMe / SATA / HDD."""
    iface_l = iface.lower()
    media_l = media.lower()
    model_l = model.lower()

    if "nvme" in model_l or "nvme" in iface_l:
        return "NVMe"
    if "ssd" in media_l or "ssd" in model_l:
        return "SATA"
    if "hdd" in media_l or "fixed hard disk" in media_l:
        return "HDD"
    if "usb" in iface_l:
        return "USB"
    return "SATA"


def _max_speed_by_type(dtype, iface, model):
    """Максимальная скорость интерфейса (MB/s)."""
    model_l = model.lower()

    if dtype == "NVMe":
        # По модели определяем поколение
        if "gen5" in model_l or "pcie 5" in model_l:
            return 14000
        if "gen4" in model_l or "pcie 4" in model_l:
            return 7000
        # По умолчанию Gen3
        return 3500

    if dtype == "SATA":
        return 550

    if dtype == "HDD":
        return 200

    if dtype == "USB":
        return 500

    return 550


def _shorten_name(name):
    """Убираем лишнее из модели."""
    if not name:
        return "Disk"
    name = name.strip()
    # Убираем "Samsung SSD 980 PRO 500GB" → оставляем как есть, но обрезаем
    if len(name) > 28:
        name = name[:28]
    return name

#  Чтение
def read():
    """
    Вернуть список дисков (список dict'ов).
    Системный (C:) — первым.
    """
    _ensure()

    # Обновить LHM Storage
    storage_devices = lhm.get_storage_devices()
    for sd in storage_devices:
        try:
            sd.Update()
        except Exception:
            pass

    disks = []

    # Разделы из psutil
    partitions = psutil.disk_partitions(all=False)

    for part in partitions:
        # Только локальные фиксированные диски
        if "cdrom" in part.opts or part.fstype == "":
            continue
        # Только буквы (Windows)
        if len(part.device) < 2 or not part.device[1] == ":":
            continue

        letter = part.device[0].upper()

        # Использование
        try:
            usage = psutil.disk_usage(part.mountpoint)
        except (PermissionError, OSError):
            continue

        used_gb  = round(usage.used  / (1024 ** 3), 1)
        total_gb = round(usage.total / (1024 ** 3), 1)
        load_pct = int(round(usage.percent))

        # LHM: температуры, активность, жизнь
        temp       = 0
        activity   = 0
        life_left  = 100
        read_rate  = 0
        write_rate = 0

        # Ищем LHM-диск, соответствующий букве
        sd = _find_lhm_disk(storage_devices, letter)
        if sd is not None:
            try:
                sd.Update()
            except Exception:
                pass

            # Температура
            t = lhm.find_sensor(sd, "Temperature", "Temperature")
            if t is None:
                t = lhm.find_sensor(sd, "Temperature", "")
            if t is not None:
                temp = int(round(t))

            # Активность (Total Activity) — %
            a = lhm.find_sensor(sd, "Load", "Total Activity")
            if a is None:
                a = lhm.find_sensor(sd, "Load", "Activity")
            if a is None:
                a = lhm.find_sensor(sd, "Load", "")
            if a is not None:
                activity = int(round(a))

            # Остаток жизни (Remaining Life) — %
            lf = lhm.find_sensor(sd, "Level", "Remaining Life")
            if lf is None:
                lf = lhm.find_sensor(sd, "Level", "Life")
            if lf is not None:
                life_left = int(round(lf))

            # Скорость чтения/записи (Throughput)
            rr = lhm.find_sensor(sd, "Throughput", "Read Rate")
            wr = lhm.find_sensor(sd, "Throughput", "Write Rate")
            # LHM отдаёт в Б/с или КБ/с — обычно в МБ/с через Data
            # Проверим на Data-сенсоры
            rr_d = lhm.find_sensor(sd, "Data", "Read Rate")
            wr_d = lhm.find_sensor(sd, "Data", "Write Rate")
            if rr_d is not None:
                rr = rr_d
            if wr_d is not None:
                wr = wr_d

            # Иногда LHM отдаёт в байтах/сек — переводим
            read_rate  = _to_mb_s(rr)
            write_rate = _to_mb_s(wr)

        # WMI-инфа
        wmi = _wmi_cache.get(letter, {})
        name     = wmi.get("name", "Disk")
        dtype    = wmi.get("type", "SATA")
        max_spd  = wmi.get("max_speed", 550)

        disk = {
            "letter":     letter,
            "name":       name,
            "load":       load_pct,
            "temp":       temp,
            "used":       used_gb,
            "total":      total_gb,
            "max_speed":  max_spd,
            "type":       dtype,
            "read_rate":  read_rate,
            "write_rate": write_rate,
            "life_left":  life_left,
            "activity":   activity,
        }
        disks.append(disk)

    # Сортировка: системный (C:) первым, потом по букве
    disks.sort(key=lambda d: (d["letter"] != "C", d["letter"]))

    if config.DEBUG:
        for d in disks:
            print(f"[DISK] {d['letter']}: {d['name']} "
                  f"{d['load']}% {d['temp']}C {d['used']}/{d['total']}GB "
                  f"{d['type']} {d['life_left']}%")

    return disks
    
#  Поиск LHM-диска по букве
def _find_lhm_disk(storage_devices, letter):
    """
    LHM называет диски как-то вроде "Samsung SSD 980 PRO 500GB".
    Точного сопоставления с буквой нет, поэтому — эвристика.

    Стратегия:
    1. Попробовать найти по индексу (если буква C — берём первый системный).
    2. Если не вышло — вернуть по индексу в списке.
    """
    if not storage_devices:
        return None

    # Для C: — обычно первый в списке (системный)
    if letter == "C":
        return storage_devices[0]

    # Для остальных — берём по позиции
    idx = ord(letter) - ord("A")
    if idx < len(storage_devices):
        return storage_devices[idx]

    return storage_devices[0]

#  Хелпер: перевод в МБ/с
def _to_mb_s(value):
    """LHM может отдавать в Б/с, КБ/с или МБ/с. Приводим к МБ/с."""
    if value is None:
        return 0
    v = float(value)

    # Если число огромное (>100000) — это байты
    if v > 100000:
        return int(round(v / (1024 * 1024)))
    # Если очень большое (>10000) — это КБ
    if v > 10000:
        return int(round(v / 1024))
    # Иначе — уже МБ/с
    return int(round(v))

#  Отладка
def dump():
    _ensure()
    storage = lhm.get_storage_devices()
    print(f"=== Storage devices in LHM: {len(storage)} ===")
    for i, sd in enumerate(storage):
        try:
            sd.Update()
        except Exception:
            pass
        print(f"--- Device #{i}: {sd.Name}")
        for s in lhm.list_sensors(sd):
            print(f"  {s['type']:12s}  {s['name']:35s}  {s['value']}")

    print("--- WMI cache")
    for k, v in _wmi_cache.items():
        print(f"  {k}: {v}")

    print("--- Partitions (psutil)")
    for p in psutil.disk_partitions(all=False):
        print(f"  {p.device}  {p.mountpoint}  {p.fstype}  {p.opts}")
