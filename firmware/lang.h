#pragma once
#include <Arduino.h>

//  Языки и строки UI

enum Lang : uint8_t {
    LANG_EN = 0,
    LANG_RU = 1
};

// Все ключи строк
enum StrKey : uint8_t {
    // Boot
    S_BOOT_TITLE,       // PC MONITOR / PC Монитор
    S_INIT,             // Initializing / Инициализация
    S_WAIT_DATA,        // waiting for data... / ожидание данных...
    S_READY,            // ready / готов
    S_NO_LINK,          // no link / нет связи

    // Статусы проверки
    S_OK,               // OK
    S_FAIL,             // FAIL
    S_WAIT,             // WAIT

    // Заголовки страниц
    S_GPU,              // GPU
    S_CPU,              // CPU
    S_RAM,              // RAM / ОЗУ
    S_DISK,             // DISK / ДИСК
    S_NET,              // NET / СЕТЬ
    S_PING,             // PING / ПИНГ
    S_LEAK,             // LEAK / УТЕЧКА
    S_CLIMATE,          // CLIMATE / КЛИМАТ
    S_FPS,              // FPS

    // Питание
    S_POWER_OFF,        // SHUTDOWN / ВЫКЛЮЧЕНИЕ
    S_POWER_REBOOT,     // REBOOT / ПЕРЕЗАГРУЗКА
    S_POWER_SLEEP,      // SLEEP / СОН
    S_CANCEL,           // CANCEL / ОТМЕНА
    S_CONFIRM,          // CONFIRM / ПОДТВЕРДИТЬ
    S_POWER_COUNTDOWN,  // Через / In

    // Ночной режим
    S_NIGHT_MODE,       // NIGHT MODE / НОЧНОЙ РЕЖИМ

    // Дни недели
    S_DAY_MON, S_DAY_TUE, S_DAY_WED, S_DAY_THU,
    S_DAY_FRI, S_DAY_SAT, S_DAY_SUN,

    // Месяцы (родительный падеж для "8 октября")
    S_MON_JAN, S_MON_FEB, S_MON_MAR, S_MON_APR,
    S_MON_MAY, S_MON_JUN, S_MON_JUL, S_MON_AUG,
    S_MON_SEP, S_MON_OCT, S_MON_NOV, S_MON_DEC,

    // Веб (заголовки разделов)
    S_WEB_DASHBOARD,    // Dashboard / Панель
    S_WEB_SETTINGS,     // Settings / Настройки
    S_WEB_CONTROL,      // Control / Управление
    S_WEB_PAGES,        // Pages / Страницы
    S_WEB_BUTTONS,      // Buttons / Кнопки
    S_WEB_OTA,          // OTA
    S_WEB_LOGS,         // Logs / Логи
    S_WEB_SETUP,        // WiFi Setup / Настройка WiFi

    S_COUNT             // счётчик — всегда последний
};

//  Таблица строк

const char* const STRINGS[2][S_COUNT] = {

    // EN 
    {
        // Boot
        "PC MONITOR",
        "Initializing",
        "waiting for data...",
        "ready",
        "no link",

        // Статусы
        "OK",
        "FAIL",
        "WAIT",

        // Заголовки
        "GPU",
        "CPU",
        "RAM",
        "DISK",
        "NET",
        "PING",
        "LEAK",
        "CLIMATE",
        "FPS",

        // Питание
        "SHUTDOWN",
        "REBOOT",
        "SLEEP",
        "CANCEL",
        "CONFIRM",
        "In",

        // Ночной
        "NIGHT MODE",

        // Дни недели
        "Monday", "Tuesday", "Wednesday", "Thursday",
        "Friday", "Saturday", "Sunday",

        // Месяцы
        "January", "February", "March", "April",
        "May", "June", "July", "August",
        "September", "October", "November", "December",

        // Веб
        "Dashboard",
        "Settings",
        "Control",
        "Pages",
        "Buttons",
        "OTA",
        "Logs",
        "WiFi Setup"
    },

    // RU 
    {
        // Boot
        "PC Монитор",
        "Инициализация",
        "ожидание данных...",
        "готов",
        "нет связи",

        // Статусы
        "OK",
        "FAIL",
        "WAIT",

        // Заголовки
        "GPU",
        "CPU",
        "ОЗУ",
        "ДИСК",
        "СЕТЬ",
        "ПИНГ",
        "УТЕЧКА",
        "КЛИМАТ",
        "ФПС",

        // Питание
        "ВЫКЛЮЧЕНИЕ",
        "ПЕРЕЗАГРУЗКА",
        "СОН",
        "ОТМЕНА",
        "ПОДТВЕРДИТЬ",
        "Через",

        // Ночной
        "НОЧНОЙ РЕЖИМ",

        // Дни недели
        "понедельник", "вторник", "среда", "четверг",
        "пятница", "суббота", "воскресенье",

        // Месяцы
        "января", "февраля", "марта", "апреля",
        "мая", "июня", "июля", "августа",
        "сентября", "октября", "ноября", "декабря",

        // Веб
        "Панель",
        "Настройки",
        "Управление",
        "Страницы",
        "Кнопки",
        "OTA",
        "Логи",
        "Настройка WiFi"
    }
};

//  Текущий язык (определяется в settings.cpp)

extern Lang currentLang;

// Быстрый доступ к строке
inline const char* T(StrKey k) {
    return STRINGS[currentLang][k];
}

// Дни недели: 0=Пн ... 6=Вс
inline const char* T_Day(uint8_t d) {
    if (d > 6) d = 0;
    return STRINGS[currentLang][S_DAY_MON + d];
}

// Месяцы: 0=янв ... 11=дек
inline const char* T_Month(uint8_t m) {
    if (m > 11) m = 0;
    return STRINGS[currentLang][S_MON_JAN + m];
}
