#  Reset — сброс счётчиков
#  by KTH-Studio
from sensors import net, leak, fps


def counters():
    """Сбросить счётчики сети и утечки."""
    print("[RESET] counters")

    # Сеть — сброс счётчиков трафика
    try:
        net._start_rx = net._last_rx
        net._start_tx = net._last_tx
    except Exception as e:
        print(f"[RESET] net error: {e}")

    # Утечка — очистка истории
    try:
        leak._tracked.clear()
    except Exception as e:
        print(f"[RESET] leak error: {e}")

    # FPS — очистка истории
    try:
        fps._history.clear()
    except Exception as e:
        print(f"[RESET] fps error: {e}")

    return True


def all():
    """Полный сброс (всё, что можно)."""
    counters()
    return True
