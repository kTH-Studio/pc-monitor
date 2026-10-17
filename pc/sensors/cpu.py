#  CPU — чтение через LHM
#  by KTH-Studio
import config
from sensors import lhm

#  Состояние
_cpu = None
_cpu_name = ""

#  Инициализация
def _ensure():
    global _cpu, _cpu_name
    if _cpu is not None:
        return True

    _cpu = lhm.get_cpu()
    if _cpu is None:
        return False

    _cpu_name = lhm.get_cpu_name(_cpu)
    # Убираем лишнее из имени (Intel часто пишет "Intel Core i7-..." — ок)
    return True

#  Чтение
def read():
    """
    Вернуть dict с данными CPU.
    Если CPU нет — {'present': False}.
    """
    if not _ensure():
        return {"present": False}

    try:
        _cpu.Update()
    except Exception as e:
        if config.DEBUG:
            print(f"[CPU] update error: {e}")

    # Загрузка
    # "CPU Total" — общая загрузка
    load = lhm.find_sensor(_cpu, "Load", "CPU Total")
    if load is None:
        load = lhm.find_sensor(_cpu, "Load", "Total")
    load = int(round(load)) if load is not None else 0

    # Температура
    # "CPU Package" — общая температура пакета (Intel/AMD)
    # "Core Average" / "Core Max" — альтернативы
    temp = lhm.find_sensor(_cpu, "Temperature", "CPU Package")
    if temp is None:
        temp = lhm.find_sensor(_cpu, "Temperature", "Core Average")
    if temp is None:
        temp = lhm.find_sensor(_cpu, "Temperature", "Core Max")
    temp = int(round(temp)) if temp is not None else 0

    # Максимальная температура среди ядер
    core_max = lhm.find_sensor(_cpu, "Temperature", "Core Max")
    if core_max is None:
        # Ищем максимум среди "CPU Core #N"
        cores = lhm.find_all_sensors(_cpu, "Temperature", "CPU Core #")
        if cores:
            core_max = max(c["value"] for c in cores)
        else:
            core_max = temp
    core_max = int(round(core_max)) if core_max is not None else 0

    # Частота
    # У LHM частота CPU ядра — "CPU Core #1" ... "CPU Core #N"
    # Средняя по всем ядрам
    clocks = lhm.find_all_sensors(_cpu, "Clock", "CPU Core #")
    if clocks:
        freq = int(round(sum(c["value"] for c in clocks) / len(clocks)))
    else:
        # Fallback — "Bus Speed" × множитель, но проще 0
        freq = 0

    # Питание
    # "CPU Package" — потребление всего пакета
    # У AMD может быть "CPU Cores" и "CPU SoC" — суммируем
    power = lhm.find_sensor(_cpu, "Power", "CPU Package")
    if power is None:
        p_cores = lhm.find_sensor(_cpu, "Power", "CPU Cores")
        p_soc   = lhm.find_sensor(_cpu, "Power", "CPU SoC")
        if p_cores is not None or p_soc is not None:
            power = (p_cores or 0) + (p_soc or 0)
    power = int(round(power)) if power is not None else 0

    # Кулер CPU
    # У LHM материнская плата отдаёт "Fan #1", "Fan #2" и т.д.
    # Ищем на материнке сенсор с "CPU" в имени
    fan = 0
    mb = lhm.get_motherboard()
    if mb is not None:
        try:
            mb.Update()
        except Exception:
            pass

        # Сначала — явный "CPU Fan"
        f = lhm.find_sensor(mb, "Fan", "CPU")
        if f is not None:
            fan = int(round(f))
        else:
            # Иначе — первый Fan #1
            all_fans = lhm.find_all_sensors(mb, "Fan")
            if all_fans:
                fan = int(round(all_fans[0]["value"]))

    # Vcore
    # "CPU Core" (Voltage) или "Vcore"
    vcore = lhm.find_sensor(_cpu, "Voltage", "CPU Core")
    if vcore is None:
        vcore = lhm.find_sensor(_cpu, "Voltage", "Vcore")
    # Переводим в мВ (целое)
    vcore_mv = int(round(vcore * 1000)) if vcore is not None else 0

    result = {
        "present":  True,
        "name":     _cpu_name,
        "load":     load,
        "temp":     temp,
        "freq":     freq,        # МГц
        "power":    power,       # Вт
        "core_max": core_max,    # °C
        "fan":      fan,         # RPM
        "vcore":    vcore_mv,    # мВ
    }

    if config.DEBUG:
        print(f"[CPU] {_cpu_name}: {load}% {temp}C "
              f"{freq}MHz {power}W coremax={core_max} "
              f"fan={fan} vcore={vcore_mv}mV")

    return result

#  Отладка — вывести все сенсоры CPU
def dump():
    if not _ensure():
        print("[CPU] not found")
        return
    print(f"=== CPU: {_cpu_name} ===")
    print("--- CPU ---")
    for s in lhm.list_sensors(_cpu):
        print(f"  {s['type']:12s}  {s['name']:30s}  {s['value']}")

    mb = lhm.get_motherboard()
    if mb is not None:
        mb.Update()
        print("--- Motherboard (кулеры) ---")
        for s in lhm.list_sensors(mb, "Fan"):
            print(f"  {s['type']:12s}  {s['name']:30s}  {s['value']}")
