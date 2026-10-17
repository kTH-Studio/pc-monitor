#  LibreHardwareMonitor wrapper
#  by KTH-Studio
import os
import sys
import clr
import config

#  Инициализация .NET + DLL
_computer = None
_initialized = False

# Пути, где ищем DLL
DLL_PATHS = [
    "LibreHardwareMonitorLib.dll",                     # рядом со скриптом
    os.path.join(os.path.dirname(__file__), "..", "LibreHardwareMonitorLib.dll"),
    r"C:\Program Files\LibreHardwareMonitor\LibreHardwareMonitorLib.dll",
    r"C:\Program Files (x86)\LibreHardwareMonitor\LibreHardwareMonitorLib.dll",
]


def _find_dll():
    for p in DLL_PATHS:
        p = os.path.abspath(p)
        if os.path.isfile(p):
            return p
    return None


def init():
    """Инициализация LHM. Должна вызываться один раз при старте."""
    global _computer, _initialized

    if _initialized:
        return

    dll = _find_dll()
    if not dll:
        raise FileNotFoundError(
            "LibreHardwareMonitorLib.dll не найден.\n"
            "Скачай с https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases\n"
            "и положи рядом со скриптом или установи LibreHardwareMonitor."
        )

    print(f"[LHM] using DLL: {dll}")

    # Загружаем сборку
    sys.path.append(os.path.dirname(dll))
    clr.AddReference(dll)

    from LibreHardwareMonitor.Hardware import Computer, HardwareType

    _computer = Computer()
    _computer.IsCpuEnabled         = True
    _computer.IsGpuEnabled         = True
    _computer.IsMemoryEnabled      = True
    _computer.IsMotherboardEnabled = True
    _computer.IsStorageEnabled     = True
    _computer.IsNetworkEnabled     = True

    _computer.Open()

    _initialized = True
    print("[LHM] initialized")

    # Обновляем всё один раз при старте
    update_all()


def close():
    """Закрытие LHM (при выходе)."""
    global _computer, _initialized
    if _computer is not None:
        try:
            _computer.Close()
        except Exception:
            pass
    _computer = None
    _initialized = False


def update_all():
    """Обновить все датчики (обязательно перед чтением)."""
    if _computer is None:
        return
    for hw in _computer.Hardware:
        try:
            hw.Update()
            for sub in hw.SubHardware:
                sub.Update()
        except Exception as e:
            if config.DEBUG:
                print(f"[LHM] update error: {e}")

#  Доступ к железу
def get_hardware(hw_type_name):
    """
    Вернуть список железа по типу.
    hw_type_name: 'Cpu', 'GpuNvidia', 'GpuAmd', 'GpuIntel', 'Memory',
                  'Motherboard', 'Storage', 'Network'
    """
    if _computer is None:
        return []

    from LibreHardwareMonitor.Hardware import HardwareType
    target = getattr(HardwareType, hw_type_name, None)
    if target is None:
        return []

    return [hw for hw in _computer.Hardware if hw.HardwareType == target]


def get_first_hardware(hw_type_name):
    """Первое железо заданного типа (или None)."""
    lst = get_hardware(hw_type_name)
    return lst[0] if lst else None


def get_all_hardware():
    """Всё железо (включая суб-железо)."""
    if _computer is None:
        return []
    result = []
    for hw in _computer.Hardware:
        result.append(hw)
        for sub in hw.SubHardware:
            result.append(sub)
    return result

#  Поиск сенсоров
def find_sensor(hardware, sensor_type, name_contains, exact=False):
    """
    Найти сенсор по типу и подстроке в имени.

    hardware:      объект железа
    sensor_type:   'Temperature', 'Load', 'Clock', 'Power', 'Fan',
                   'Voltage', 'Data', 'Level', 'Throughput', 'SmallData'
    name_contains: подстрока в имени сенсора
    exact:         если True — точное совпадение имени

    Возврат: значение float или None
    """
    if hardware is None:
        return None

    from LibreHardwareMonitor.Hardware import SensorType
    st = getattr(SensorType, sensor_type, None)
    if st is None:
        return None

    for sensor in hardware.Sensors:
        if sensor.SensorType != st:
            continue

        name = sensor.Name or ""

        match = (name == name_contains) if exact else (name_contains.lower() in name.lower())
        if match:
            try:
                v = sensor.Value
                return float(v) if v is not None else None
            except Exception:
                return None

    return None


def find_all_sensors(hardware, sensor_type, name_contains=None):
    """
    Вернуть список значений всех подходящих сенсоров.
    Если name_contains=None — все сенсоры этого типа.
    """
    if hardware is None:
        return []

    from LibreHardwareMonitor.Hardware import SensorType
    st = getattr(SensorType, sensor_type, None)
    if st is None:
        return []

    result = []
    for sensor in hardware.Sensors:
        if sensor.SensorType != st:
            continue
        if name_contains is not None:
            if name_contains.lower() not in (sensor.Name or "").lower():
                continue
        try:
            v = sensor.Value
            if v is not None:
                result.append({
                    "name":  sensor.Name,
                    "value": float(v),
                })
        except Exception:
            pass

    return result


def list_sensors(hardware, sensor_type=None):
    """
    Отладочная функция — вывести все сенсоры железа.
    Если sensor_type=None — все типы.
    """
    if hardware is None:
        return []

    from LibreHardwareMonitor.Hardware import SensorType

    result = []
    for sensor in hardware.Sensors:
        if sensor_type is not None:
            st = getattr(SensorType, sensor_type, None)
            if st is None or sensor.SensorType != st:
                continue
        try:
            v = sensor.Value
            result.append({
                "type":  str(sensor.SensorType),
                "name":  sensor.Name,
                "value": float(v) if v is not None else None,
            })
        except Exception:
            pass

    return result

#  Хелперы для конкретного железа
def get_gpu():
    """
    Первая дискретная GPU. Порядок: NVIDIA → AMD → Intel.
    Возврат: объект железа или None.
    """
    for t in ("GpuNvidia", "GpuAmd", "GpuIntel"):
        gpus = get_hardware(t)
        if gpus:
            return gpus[0]
    return None


def get_gpu_name(hardware):
    """Имя GPU (из свойства Name или из первого сенсора)."""
    if hardware is None:
        return ""
    # У Hardware есть свойство Name
    try:
        return str(hardware.Name)
    except Exception:
        return ""


def get_cpu():
    """Первый CPU."""
    return get_first_hardware("Cpu")


def get_cpu_name(hardware):
    if hardware is None:
        return ""
    try:
        return str(hardware.Name)
    except Exception:
        return ""


def get_memory():
    """Первый модуль памяти (Memory)."""
    return get_first_hardware("Memory")


def get_storage_devices():
    """Все накопители."""
    return get_hardware("Storage")


def get_motherboard():
    """Материнская плата."""
    return get_first_hardware("Motherboard")
