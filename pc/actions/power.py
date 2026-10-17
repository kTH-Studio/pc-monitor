#  Power actions — сон / перезагрузка / выключение
#  by KTH-Studio
import subprocess
import sys
import time
import config

#  Команды Windows
CMD_SLEEP  = "rundll32.exe powrprof.dll,SetSuspendState 0,1,0"
CMD_REBOOT = "shutdown /r /t 0"
CMD_OFF    = "shutdown /s /t 0"

# Флаг CREATE_NO_WINDOW — чтобы не мигало окно консоли
NO_WINDOW = 0x08000000

#  Действия
def sleep():
    """Сон (S3). На некоторых системах может не сработать — fallback ниже."""
    print("[POWER] sleep")
    try:
        # Вариант 1 — rundll32
        r = subprocess.run(
            CMD_SLEEP.split(),
            capture_output=True,
            creationflags=NO_WINDOW,
            timeout=5,
        )
        if r.returncode == 0:
            return True
    except Exception as e:
        print(f"[POWER] rundll32 error: {e}")

    # Fallback — PowerShell
    try:
        ps = "Add-Type -AssemblyName System.Windows.Forms; " \
             "[System.Windows.Forms.Application]::SetSuspendState('Suspend',$false,$false)"
        subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps],
            capture_output=True,
            creationflags=NO_WINDOW,
            timeout=5,
        )
        return True
    except Exception as e:
        print(f"[POWER] PS sleep error: {e}")
        return False


def reboot(delay_sec=0):
    """Перезагрузка. delay_sec — задержка перед перезагрузкой."""
    print(f"[POWER] reboot in {delay_sec}s")
    try:
        cmd = f"shutdown /r /t {delay_sec}"
        subprocess.Popen(
            cmd.split(),
            creationflags=NO_WINDOW,
        )
        return True
    except Exception as e:
        print(f"[POWER] reboot error: {e}")
        return False


def shutdown(delay_sec=0):
    """Выключение. delay_sec — задержка."""
    print(f"[POWER] shutdown in {delay_sec}s")
    try:
        cmd = f"shutdown /s /t {delay_sec}"
        subprocess.Popen(
            cmd.split(),
            creationflags=NO_WINDOW,
        )
        return True
    except Exception as e:
        print(f"[POWER] shutdown error: {e}")
        return False


def cancel():
    """Отмена запланированного shutdown."""
    print("[POWER] cancel")
    try:
        subprocess.Popen(
            ["shutdown", "/a"],
            creationflags=NO_WINDOW,
        )
        return True
    except Exception as e:
        print(f"[POWER] cancel error: {e}")
        return False

#  Роутер команд
def execute(action):
    """
    action: 'sleep' | 'reboot' | 'off' | 'cancel'
    """
    if action == "sleep":
        return sleep()
    elif action == "reboot":
        return reboot(0)
    elif action == "off":
        return shutdown(0)
    elif action == "cancel":
        return cancel()
    else:
        print(f"[POWER] unknown action: {action}")
        return False
