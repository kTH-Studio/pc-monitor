#  RAM — чтение через LHM + psutil + WMI
#  by KTH-Studio
import config
import psutil
from sensors import lhm

#  Состояние
_mem_hw   = None
_ram_name = ""
_ram_type = ""        # DDR4 / DDR5
_dual     = False
_info_loaded = False

#  Инициализация
def _ensure():
    global _mem_hw, _ram_name, _ram_type, _dual, _info_loaded

    if _mem_hw is None:
        _mem_hw = lhm.get_memory()

    if _info_loaded:
        return True

    _load_wmi_info()
    _info_loaded = True
    return True

#  WMI — тип памяти, каналы, имена модулей
def _load_wmi_info():
    global _ram_name, _ram_type, _dual

    try:
        import subprocess

        # Тип памяти (DDR4/DDR5) и производитель
        # SMBIOSMemoryType: 26=DDR4, 34=DDR5 (по SMBIOS 3.2+), 24=DDR3
        # MemoryType: 20=DDR, 21=DDR2, 24=DDR3, 26=DDR4 (старое поле)
        out = subprocess.check_output(
            ["wmic", "memorychip", "get",
             "Manufacturer,PartNumber,SMBIOSMemoryType,Speed,Capacity",
             "/format:list"],
            stderr=subprocess.DEVNULL,
            creationflags=0x08000000,
        ).decode(errors="ignore")

        modules = []
        cur = {}
        for line in out.splitlines():
            line = line.strip()
            if not line:
                if cur:
                    modules.append(cur)
                    cur = {}
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                cur[k.strip()] = v.strip()
        if cur:
            modules.append(cur)

        # Тип DDR
        smbios_type = 0
        for m in modules:
            t = m.get("SMBIOSMemoryType", "")
            if t and t.isdigit():
                smbios_type = int(t)
                break

        type_map = {
            20: "DDR",
            21: "DDR2",
            24: "DDR3",
            26: "DDR4",
            27: "LPDDR",
            28: "LPDDR2",
            29: "LPDDR3",
            30: "LPDDR4",
            34: "DDR5",
            35: "LPDDR5",
        }
        _ram_type = type_map.get(smbios_type, "")

        # Если не определилось — попробуем из скорости
        if not _ram_type:
            for m in modules:
                sp = m.get("Speed", "")
                if sp.isdigit():
                    speed = int(sp)
                    if speed >= 4800:
                        _ram_type = "DDR5"
                    elif speed >= 2133:
                        _ram_type = "DDR4"
                    elif speed >= 1066:
                        _ram_type = "DDR3"
                    break

        # Имя — производитель + part number (первого модуля)
        if modules:
            m0 = modules[0]
            manu = m0.get("Manufacturer", "").strip()
            part = m0.get("PartNumber", "").strip()

            # Убираем пробелы по краям и дубли
            if manu.lower() in ("unknown", "undefined", ""):
                manu = ""
            if part.lower() in ("unknown", "undefined", ""):
                part = ""

            if manu and part:
                # Если производитель уже в part — не дублируем
                if manu.lower() in part.lower():
                    _ram_name = part
                else:
                    _ram_name = f"{manu} {part}"
            elif part:
                _ram_name = part
            elif manu:
                _ram_name = manu
            else:
                _ram_name = "RAM"

            # Обрезаем до 30 символов
            if len(_ram_name) > 30:
                _ram_name = _ram_name[:30]

        # Каналы — по количеству модулей (упрощённо)
        _dual = len(modules) >= 2

    except Exception as e:
        if config.DEBUG:
            print(f"[RAM] WMI error: {e}")
        _ram_name = "RAM"
        _ram_type = ""
        _dual = False

#  Чтение
def read():
    """
    Вернуть dict с данными RAM.
    """
    _ensure()

    # Использование (psutil — самый надёжный)
    vm = psutil.virtual_memory()
    used_gb  = round(vm.used  / (1024 ** 3), 1)
    total_gb = round(vm.total / (1024 ** 3), 1)
    load     = int(round(vm.percent))

    # Частота, температура, напряжение из LHM
    freq    = 0
    temp    = 0
    voltage = 0

    if _mem_hw is not None:
        try:
            _mem_hw.Update()
        except Exception as e:
            if config.DEBUG:
                print(f"[RAM] update error: {e}")

        # Частота памяти — Clock
        f = lhm.find_sensor(_mem_hw, "Clock", "Memory")
        if f is None:
            f = lhm.find_sensor(_mem_hw, "Clock", "")
        if f is not None:
            freq = int(round(f))

        # Если LHM не дал частоту — из WMI
        if freq == 0:
            freq = _get_speed_from_wmi()

        # Температура (если модули поддерживают)
        t = lhm.find_sensor(_mem_hw, "Temperature", "DIMM")
        if t is None:
            t = lhm.find_sensor(_mem_hw, "Temperature", "")
        if t is not None:
            temp = int(round(t))

        # Напряжение (если LHM отдаёт)
        v = lhm.find_sensor(_mem_hw, "Voltage", "DIMM")
        if v is None:
            # Иногда на материнке — "DRAM Voltage"
            mb = lhm.get_motherboard()
            if mb is not None:
                try:
                    mb.Update()
                except Exception:
                    pass
                v = lhm.find_sensor(mb, "Voltage", "DRAM")
        if v is not None:
            voltage = int(round(v * 1000))   # мВ

    result = {
        "present":  True,
        "name":     _ram_name,
        "load":     load,
        "used":     used_gb,
        "total":    total_gb,
        "freq":     freq,
        "type":     _ram_type,
        "temp":     temp,
        "voltage":  voltage,
        "dual":     _dual,
    }

    if config.DEBUG:
        print(f"[RAM] {_ram_name}: {load}% {used_gb}/{total_gb}GB "
              f"{freq}MHz {_ram_type} {temp}C {voltage}mV dual={_dual}")

    return result
    
#  Хелпер: скорость из WMI (МГц)
_speed_cache = 0

def _get_speed_from_wmi():
    global _speed_cache
    if _speed_cache:
        return _speed_cache

    try:
        import subprocess
        out = subprocess.check_output(
            ["wmic", "memorychip", "get", "Speed", "/format:list"],
            stderr=subprocess.DEVNULL,
            creationflags=0x08000000,
        ).decode(errors="ignore")

        speeds = []
        for line in out.splitlines():
            line = line.strip()
            if line.startswith("Speed="):
                v = line.split("=", 1)[1].strip()
                if v.isdigit():
                    speeds.append(int(v))

        if speeds:
            _speed_cache = max(speeds)
            return _speed_cache
    except Exception:
        pass
    return 0

#  Отладка
def dump():
    _ensure()
    print(f"=== RAM: {_ram_name} ({_ram_type}) ===")
    print(f"  dual channel: {_dual}")

    if _mem_hw is not None:
        _mem_hw.Update()
        print("--- LHM Memory")
        for s in lhm.list_sensors(_mem_hw):
            print(f"  {s['type']:12s}  {s['name']:30s}  {s['value']}")

    print("--- psutil")
    vm = psutil.virtual_memory()
    print(f"  used:  {vm.used / (1024**3):.1f} GB")
    print(f"  total: {vm.total / (1024**3):.1f} GB")
    print(f"  percent: {vm.percent}%")
