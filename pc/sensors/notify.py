#  Нотификации Windows — winsdk UserNotificationListener
#  by KTH-Studio
import asyncio
import threading
import time
import config

#  Состояние
_initialized = False
_loop        = None
_thread      = None
_listener    = None

# Очередь новых нотификаций (для отправки на ESP)
# Каждая: {'app', 'count', 'title', 'body', 'time', 'id'}
_pending = []
_lock    = threading.Lock()

# Уже обработанные ID
_seen_ids = set()
_seen_order = []      # для обрезки старых

# Группировка по приложению: {app: [notify, notify, ...]}
_groups = {}

#  Инициализация
def _ensure():
    global _initialized, _loop, _thread
    if _initialized:
        return True

    try:
        from winsdk.windows.ui.notifications.management import (
            UserNotificationListener,
            UserNotificationListenerAccessStatus,
        )
    except ImportError:
        if config.DEBUG:
            print("[NOTIFY] winsdk not installed")
        return False

    _loop = asyncio.new_event_loop()
    _thread = threading.Thread(target=_run_loop, daemon=True)
    _thread.start()
    _initialized = True
    return True


def _run_loop():
    asyncio.set_event_loop(_loop)
    _loop.run_until_complete(_main_loop())


async def _main_loop():
    global _listener
    from winsdk.windows.ui.notifications.management import (
        UserNotificationListener,
        UserNotificationListenerAccessStatus,
    )

    # Запрос разрешения
    try:
        _listener = UserNotificationListener.current
        access = await _listener.request_access_async()
    except Exception as e:
        if config.DEBUG:
            print(f"[NOTIFY] access error: {e}")
        return

    if access != UserNotificationListenerAccessStatus.ALLOWED:
        if config.DEBUG:
            print(f"[NOTIFY] access denied (status={access}). "
                  "Включи доступ в Параметры → Конфиденциальность → Уведомления")
        return

    if config.DEBUG:
        print("[NOTIFY] listener active")

    # Цикл опроса
    while True:
        try:
            await _poll_once()
        except Exception as e:
            if config.DEBUG:
                print(f"[NOTIFY] poll error: {e}")
        await asyncio.sleep(config.INTERVAL_NOTIFY)


async def _poll_once():
    if _listener is None:
        return

    try:
        notifications = await _listener.get_notifications_async(0)  # 0 = все
    except Exception as e:
        if config.DEBUG:
            print(f"[NOTIFY] get error: {e}")
        return

    for n in notifications:
        try:
            nid = n.id
        except Exception:
            continue

        if nid in _seen_ids:
            continue

        # Новая нотификация
        _seen_ids.add(nid)
        _seen_order.append(nid)
        # Обрезаем историю ID
        if len(_seen_order) > 500:
            old = _seen_order.pop(0)
            _seen_ids.discard(old)

        # Парсим
        item = _parse_notification(n)
        if item is None:
            continue

        # Добавляем в очередь на отправку
        with _lock:
            _pending.append(item)

#  Разбор одной нотификации
def _parse_notification(n):
    try:
        app_id = n.app_info.app_user_model_id or ""
        app_name = _pretty_app_name(app_id)

        # Время
        try:
            ts = n.creation_time
            # ts — .NET DateTimeOffset (100-нс интервалы от 1601 года)
            # Конвертируем в Unix
            sec = ts / 10_000_000 - 11644473600
            dt = time.localtime(sec)
            today = time.localtime()
            if (dt.tm_year, dt.tm_yday) == (today.tm_year, today.tm_yday):
                time_str = time.strftime("%H:%M", dt)
            else:
                time_str = time.strftime("%d.%m %H:%M", dt)
        except Exception:
            time_str = time.strftime("%H:%M")

        # Текст
        title = ""
        body  = ""
        try:
            notification = n.notification
            visual = notification.visual
            bindings = visual.get_bindings()
            if bindings:
                binding = bindings[0]
                texts = binding.get_text_elements()
                if texts:
                    for i, t in enumerate(texts):
                        text = t.text or ""
                        if i == 0:
                            title = text
                        elif i == 1:
                            body = text
                        else:
                            # Многострочное тело — склеиваем
                            body += "\n" + text
        except Exception:
            pass

        if not title and not body:
            return None

        # Собираем
        item = {
            "app":    app_name,
            "title":  title[:48],       # максимум 48 символов на тему
            "body":   body[:500],       # тело обрезаем на 500, ESP сам порежет
            "time":   time_str,
            "raw_app": app_id,
        }
        return item

    except Exception as e:
        if config.DEBUG:
            print(f"[NOTIFY] parse error: {e}")
        return None

#  Чтение (для отправки на ESP)
def read():
    """
    Возврат: список новых нотификаций с момента последнего вызова.
    ESP получает их и ставит в очередь на показ.
    """
    if not _ensure():
        return []

    with _lock:
        items = list(_pending)
        _pending.clear()

    # Группировка: одна нотификация на приложение с полем count
    # (если приложение прислало несколько)
    grouped = {}
    order = []
    for it in items:
        app = it["app"]
        if app in grouped:
            grouped[app]["count"] += 1
        else:
            grouped[app] = {
                "app":   app,
                "count": 1,
                "title": it["title"],
                "body":  it["body"],
                "time":  it["time"],
            }
            order.append(app)

    # Возвращаем в порядке появления, берём самую свежую версию каждой группы
    result = [grouped[app] for app in order]

    if config.DEBUG and result:
        for r in result:
            print(f"[NOTIFY] {r['app']} +{r['count']}: {r['title']} — {r['body'][:40]}")

    return result

#  Хелперы
_APP_PRETTY = {
    "telegram":     "Telegram",
    "whatsapp":     "WhatsApp",
    "discord":      "Discord",
    "slack":        "Slack",
    "outlook":      "Outlook",
    "mail":         "Mail",
    "steam":        "Steam",
    "spotify":      "Spotify",
    "chrome":       "Chrome",
    "msedge":       "Edge",
    "firefox":      "Firefox",
    "windows":      "Windows",
    "windowsupdate":"Windows",
    "explorer":     "Explorer",
    "skype":        "Skype",
    "teams":        "Teams",
    "zoom":         "Zoom",
    "viber":        "Viber",
    "signal":       "Signal",
}


def _pretty_app_name(app_id):
    if not app_id:
        return "App"

    key = app_id.lower()

    # Точное
    for k, v in _APP_PRETTY.items():
        if k in key:
            return v

    # Fallback — часть после '!' или до .exe
    name = app_id.split("!")[-1] if "!" in app_id else app_id
    if name.lower().endswith(".exe"):
        name = name[:-4]
    # Убираем длинные префиксы (Microsoft.WindowsStore_...)
    if "." in name and "_" in name:
        name = name.split("_")[0].split(".")[-1]
    if len(name) > 16:
        name = name[:16]
    return name or "App"

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
        print("[NOTIFY] winsdk не установлен")
        return

    print("=== Access status ===")
    time.sleep(1)   # дать инициализации
    from winsdk.windows.ui.notifications.management import UserNotificationListener
    try:
        l = UserNotificationListener.current
        print(f"  listener: {l}")
        print(f"  access: {l.get_access_status()}")
    except Exception as e:
        print(f"  error: {e}")

    print("\n=== Current notifications ===")
    if _listener:
        try:
            import asyncio
            notifs = asyncio.run_coroutine_threadsafe(
                _listener.get_notifications_async(0), _loop
            ).result(timeout=3)
            print(f"  count: {len(notifs)}")
            for n in notifs[:10]:
                try:
                    print(f"    id={n.id}  app={n.app_info.app_user_model_id}")
                except Exception:
                    pass
        except Exception as e:
            print(f"  error: {e}")

    print("\n=== Pending queue ===")
    print(f"  {len(_pending)} items")
    for it in _pending[:5]:
        print(f"    {it['app']}: {it['title']}")

    print("\n=== read() ===")
    print(read())
