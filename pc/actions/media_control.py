#  Media control — next / prev / playpause
#  by KTH-Studio
from sensors import media

def execute(action):
    """
    action: 'next' | 'prev' | 'playpause'
    """
    if action not in ("next", "prev", "playpause"):
        print(f"[MEDIA] unknown action: {action}")
        return False

    ok = media.control(action)
    print(f"[MEDIA] {action} → {'ok' if ok else 'fail'}")
    return ok
