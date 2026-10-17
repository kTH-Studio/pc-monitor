#  Медиа — WinRT GlobalSystemMediaTransportControlsSessionManager
#  by KTH-Studio
import asyncio
import threading
import time
import config

#  Состояние
_initialized = False
_loop        = None
_thread      = None
_manager     = None

# Текущее состояние (обновляется в отдельном потоке)
_state = {
    "present":  False,
    "playing":  False,
    "app":      "",
    "artist":   "",
    "track":    "",
    "album":    "",
    "pos":      0,
    "dur":      0,
    "updated":  False,
}

# Слежение за сменой трека
_last_track_key = ""

#  Инициализация — запускаем asyncio-loop в отдельном потоке
def _ensure():
    global _initialized, _loop, _thread
    if _initialized:
        return True

    try:
        # Проверяем, что winsdk доступен
        from winsdk.windows.media.control import (
            GlobalSystemMediaTransportControlsSessionManager as GSMTCSM
        )
    except ImportError:
        if config.DEBUG:
            print("[MEDIA] winsdk not installed")
        return False

    _loop = asyncio.new_event_loop()
    _thread = threading.Thread(target=_run_loop, daemon=True)
    _thread.start()
    _initialized = True
    if config.DEBUG:
        print("[MEDIA] async loop started")
    return True


def _run_loop():
    asyncio.set_event_loop(_loop)
    _loop.run_until_complete(_poll_forever())

#  Основной цикл опроса
async def _poll_forever():
    global _manager
    from winsdk.windows.media.control import (
        GlobalSystemMediaTransportControlsSessionManager as GSMTCSM
    )

    # Получаем менеджер
    try:
        _manager = await GSMTCSM.request_async()
    except Exception as e:
        if config.DEBUG:
            print(f"[MEDIA] request_async error: {e}")
        return

    while True:
        try:
            await _poll_once()
        except Exception as e:
            if config.DEBUG:
                print(f"[MEDIA] poll error: {e}")

        await asyncio.sleep(config.INTERVAL_MEDIA)


async def _poll_once():
    """Один цикл: получить активную сессию и обновить _state."""
    global _last_track_key

    if _manager is None:
        return

    # Активная сессия (та, что сейчас играет)
    try:
        session = _manager.get_current_session()
    except Exception:
        session = None

    # Если нет активной — попробуем найти любую играющую
    if session is None:
        try:
            sessions = _manager.get_sessions()
            for s in sessions:
                info = s.get_playback_info()
                if info and info.playback_status == 4:  # PLAYING
                    session = s
                    break
            if session is None and sessions:
                session = sessions[0]
        except Exception:
            pass

    if session is None:
        _state["present"] = False
        _state["playing"] = False
        return

    # App ID
    try:
        app_id = session.source_app_user_model_id or ""
    except Exception:
        app_id = ""

    app_name = _pretty_app_name(app_id)

    # Медиа-свойства
    try:
        props = await session.try_get_media_properties_async()
    except Exception:
        props = None

    if props is None:
        _state["present"] = False
        return

    title  = props.title  or ""
    artist = props.artist or ""
    album  = props.album_title or ""

    # Состояние воспроизведения
    try:
        playback = session.get_playback_info()
        playing = (playback.playback_status == 4)  # PLAYING
    except Exception:
        playing = False

    # Позиция / длительность
    pos_sec = 0
    dur_sec = 0
    try:
        timeline = session.get_timeline_properties()
        if timeline:
            # start_time — это .NET TimeSpan
            pos_sec = int(timeline.position.total_seconds())
            dur_sec = int(timeline.end_time.total_seconds())
    except Exception:
        pass

    # Определяем, сменился ли трек
    track_key = f"{app_name}|{artist}|{title}"
    if track_key != _last_track_key:
        _last_track_key = track_key
        _state["updated"] = True
    else:
        _state["updated"] = False

    # Обновляем _state
    _state["present"] = True
    _state["playing"] = playing
    _state["app"]     = app_name
    _state["artist"]  = artist
    _state["track"]   = title
    _state["album"]   = album
    _state["pos"]     = pos_sec
    _state["dur"]     = dur_sec

#  Чтение (синхронное — из основного потока)
def read():
    """
    Возврат: dict для JSON.
    Данные обновляются асинхронным потоком, тут просто читаем.
    """
    if not _ensure():
        return {"present": False}

    # Если поток ещё не успел инициализироваться — ждём
    if _manager is None:
        time.sleep(0.1)

    result = {
        "present":  _state["present"],
        "playing":  _state["playing"],
        "app":      _state["app"],
        "artist":   _state["artist"],
        "track":    _state["track"],
        "album":    _state["album"],
        "pos":      _state["pos"],
        "dur":      _state["dur"],
        "updated":  _state["updated"],
    }

    if config.DEBUG and result["present"]:
        print(f"[MEDIA] {result['app']}: "
              f"{'▶' if result['playing'] else '⏸'} "
              f"{result['artist']} — {result['track']} "
              f"({result['pos']}/{result['dur']})")

    return result

#  Управление (вызывается из listener/commands.py)
def control(action):
    """
    action: 'next' | 'prev' | 'playpause'
    Возврат: True если сработало.
    """
    if not _ensure():
        return False

    try:
        future = asyncio.run_coroutine_threadsafe(
            _control_async(action), _loop
        )
        return future.result(timeout=2.0)
    except Exception as e:
        if config.DEBUG:
            print(f"[MEDIA] control error: {e}")
        return False


async def _control_async(action):
    if _manager is None:
        return False

    try:
        session = _manager.get_current_session()
        if session is None:
            return False

        if action == "next":
            return await session.try_skip_next_async()
        elif action == "prev":
            return await session.try_skip_previous_async()
        elif action == "playpause":
            info = session.get_playback_info()
            if info.playback_status == 4:  # PLAYING
                return await session.try_pause_async()
            else:
                return await session.try_play_async()
        return False
    except Exception as e:
        if config.DEBUG:
            print(f"[MEDIA] control async error: {e}")
        return False
        
#  Хелперы
_APP_PRETTY = {
    "spotify.exe":                    "Spotify",
    "spotifyab.spotifymusic":         "Spotify",
    "chrome.exe":                     "Chrome",
    "msedge.exe":                     "Edge",
    "firefox.exe":                    "Firefox",
    "vlc.exe":                        "VLC",
    "wmplayer.exe":                   "WMP",
    "music.ui.exe":                   "Groove",
    "foobar2000.exe":                 "foobar2000",
    "aimp.exe":                       "AIMP",
    "youtube.exe":                    "YouTube",
    "applemusic.exe":                 "Apple Music",
    "deezer.exe":                     "Deezer",
    "tidal.exe":                      "Tidal",
}

def _pretty_app_name(app_id):
    """'Spotify.exe' → 'Spotify', 'chrome.exe' → 'Chrome'."""
    if not app_id:
        return "Media"

    key = app_id.lower()

    # Точное совпадение
    if key in _APP_PRETTY:
        return _APP_PRETTY[key]

    # Частичное (например, "SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify")
    for k, v in _APP_PRETTY.items():
        if k.split(".")[0] in key:
            return v

    # Fallback — первое слово до .exe
    name = app_id.split("!")[-1] if "!" in app_id else app_id
    if name.lower().endswith(".exe"):
        name = name[:-4]
    if len(name) > 16:
        name = name[:16]
    return name

#  Закрытие
def close():
    global _loop, _initialized
    if _loop is not None:
        try:
            _loop.call_soon_threadsafe(_loop.stop)
        except Exception:
            pass
    _initialized = False

#  Отладка
def dump():
    if not _ensure():
        print("[MEDIA] winsdk не установлен")
        return

    print("=== Current session ===")
    time.sleep(0.5)   # дать потоку время
    r = read()
    for k, v in r.items():
        print(f"  {k:10s}: {v}")

    print("\n=== All sessions ===")
    try:
        sessions = _manager.get_sessions()
        for s in sessions:
            try:
                app = s.source_app_user_model_id
                info = s.get_playback_info()
                status = info.playback_status if info else "?"
                print(f"  {app}  status={status}")
            except Exception as e:
                print(f"  <error: {e}>")
    except Exception as e:
        print(f"  error: {e}")
