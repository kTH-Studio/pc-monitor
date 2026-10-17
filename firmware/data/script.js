/* PC Monitor v.5.0
   by KTH-Studio
   Общий скрипт для всех страниц */

'use strict';

//  Конфигурация
const CFG = {
    wsReconnectMs: 3000,     // переподключение WebSocket
    apiTimeoutMs:  5000,     // таймаут API-запросов
    pushIntervalMs: 1000,    // как часто обновлять страницу
};

//  Состояние
const State = {
    ws: null,
    connected: false,
    lang: 'ru',        // ru / en
    data: {},          // последние данные от ESP
    listeners: [],     // колбэки на новые данные
    apiBase: '',       // префикс (пусто — корень)
};

//  WebSocket
function wsConnect() {
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    const url = `${proto}://${location.host}/ws`;

    try {
        State.ws = new WebSocket(url);
    } catch (e) {
        console.error('[WS] create error:', e);
        setTimeout(wsConnect, CFG.wsReconnectMs);
        return;
    }

    State.ws.onopen = () => {
        State.connected = true;
        console.log('[WS] connected');
        setStatusIndicator(true);
        // Подписка на состояние
        wsSend({ type: 'hello' });
    };

    State.ws.onclose = () => {
        State.connected = false;
        console.log('[WS] disconnected');
        setStatusIndicator(false);
        setTimeout(wsConnect, CFG.wsReconnectMs);
    };

    State.ws.onerror = (e) => {
        console.error('[WS] error:', e);
    };

    State.ws.onmessage = (evt) => {
        try {
            const msg = JSON.parse(evt.data);
            handleWsMessage(msg);
        } catch (e) {
            console.error('[WS] parse error:', e);
        }
    };
}

function wsSend(obj) {
    if (State.ws && State.ws.readyState === WebSocket.OPEN) {
        State.ws.send(JSON.stringify(obj));
    }
}

function handleWsMessage(msg) {
    const type = msg.type || '';

    if (type === 'state') {
        // Общее состояние
        Object.assign(State.data, msg);
        notifyListeners(msg);
    } else if (type === 'log') {
        // Строка лога
        notifyLog(msg.line);
    } else if (type === 'notify') {
        // Нотификация — можно показать в браузере
        if (msg.text) {
            showToast(msg.text);
        }
    } else if (type === 'data') {
        // Полный пакет данных (если шлём)
        State.data = msg;
        notifyListeners(msg);
    }
}

//  Подписки на данные
function onData(fn) {
    State.listeners.push(fn);
}

function notifyListeners(msg) {
    for (const fn of State.listeners) {
        try { fn(msg); } catch (e) { console.error(e); }
    }
}

//  API-хелперы
async function apiGet(path) {
    return apiRequest('GET', path, null);
}

async function apiPost(path, data) {
    return apiRequest('POST', path, data);
}

async function apiRequest(method, path, data) {
    const url = State.apiBase + path;
    const opts = {
        method,
        headers: { 'Content-Type': 'application/json' },
        signal: AbortSignal.timeout(CFG.apiTimeoutMs),
    };
    if (data !== null && data !== undefined) {
        opts.body = typeof data === 'string' ? data : JSON.stringify(data);
    }

    try {
        const r = await fetch(url, opts);
        const text = await r.text();
        if (!r.ok) {
            console.error(`[API] ${method} ${path} → ${r.status}`, text);
            return { ok: false, status: r.status, error: text };
        }
        try {
            return { ok: true, data: JSON.parse(text) };
        } catch {
            return { ok: true, data: text };
        }
    } catch (e) {
        console.error(`[API] ${method} ${path} error:`, e);
        return { ok: false, error: String(e) };
    }
}

//  Утилиты
// Форматирование байтов
function fmtBytes(bytes) {
    if (bytes == null) return '--';
    const units = ['B', 'KB', 'MB', 'GB', 'TB'];
    let i = 0;
    let v = bytes;
    while (v >= 1024 && i < units.length - 1) { v /= 1024; i++; }
    return v.toFixed(v >= 100 ? 0 : 1) + units[i];
}

// Форматирование GB
function fmtGB(v) {
    if (v == null) return '--';
    if (v >= 10) return Math.round(v) + 'GB';
    return v.toFixed(1) + 'GB';
}

// Форматирование uptime
function fmtUptime(sec) {
    if (sec == null) return '--';
    const d = Math.floor(sec / 86400);
    const h = Math.floor((sec % 86400) / 3600);
    const m = Math.floor((sec % 3600) / 60);
    const s = sec % 60;
    if (d > 0) return `${d}d ${h}h ${m}m`;
    if (h > 0) return `${h}h ${m}m`;
    if (m > 0) return `${m}m ${s}s`;
    return `${s}s`;
}

// Форматирование времени ЧЧ:ММ
function fmtTime(sec) {
    if (sec == null) return '--:--';
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

// Экранирование HTML
function esc(str) {
    if (str == null) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

// Класс для процента (для цветовой индикации)
function pctClass(v, warn = 70, crit = 90) {
    if (v >= crit) return 'text-red';
    if (v >= warn) return 'text-orange';
    return 'text-green';
}

// Установка текста элемента
function setText(id, text) {
    const el = document.getElementById(id);
    if (el) el.textContent = text;
}

// Установка HTML
function setHTML(id, html) {
    const el = document.getElementById(id);
    if (el) el.innerHTML = html;
}

//  Индикатор связи (в навигации)
function setStatusIndicator(online) {
    const el = document.getElementById('conn-status');
    if (!el) return;
    if (online) {
        el.textContent = '●';
        el.style.color = 'var(--green)';
        el.title = 'Connected';
    } else {
        el.textContent = '○';
        el.style.color = 'var(--red)';
        el.title = 'Disconnected';
    }
}

//  Модальное окно (подтверждение)
function showConfirm(title, text, onConfirm, timeoutSec = 0) {
    // Убираем предыдущее, если есть
    closeModal();

    const backdrop = document.createElement('div');
    backdrop.className = 'modal-backdrop';
    backdrop.id = 'modal-backdrop';

    const modal = document.createElement('div');
    modal.className = 'modal';

    const h = document.createElement('h3');
    h.className = 'modal__title';
    h.textContent = title;

    const p = document.createElement('p');
    p.className = 'modal__text';
    p.textContent = text;

    // Таймер, если задан
    let timerEl = null;
    let countdownTimer = null;
    let remaining = timeoutSec;

    if (timeoutSec > 0) {
        timerEl = document.createElement('p');
        timerEl.className = 'modal__text text-orange';
        timerEl.textContent = `Через ${remaining}...`;

        countdownTimer = setInterval(() => {
            remaining--;
            if (remaining <= 0) {
                clearInterval(countdownTimer);
                closeModal();
                onConfirm();
                return;
            }
            timerEl.textContent = `Через ${remaining}...`;
        }, 1000);
    }

    const btnRow = document.createElement('div');
    btnRow.className = 'modal__buttons';

    const btnCancel = document.createElement('button');
    btnCancel.className = 'btn';
    btnCancel.textContent = 'ОТМЕНА';
    btnCancel.onclick = () => {
        if (countdownTimer) clearInterval(countdownTimer);
        closeModal();
    };

    const btnOk = document.createElement('button');
    btnOk.className = 'btn btn--red';
    btnOk.textContent = 'ПОДТВЕРДИТЬ';
    btnOk.onclick = () => {
        if (countdownTimer) clearInterval(countdownTimer);
        closeModal();
        onConfirm();
    };

    btnRow.appendChild(btnCancel);
    btnRow.appendChild(btnOk);

    modal.appendChild(h);
    modal.appendChild(p);
    if (timerEl) modal.appendChild(timerEl);
    modal.appendChild(btnRow);
    backdrop.appendChild(modal);
    document.body.appendChild(backdrop);
}

function closeModal() {
    const el = document.getElementById('modal-backdrop');
    if (el) el.remove();
}

//  Тост (всплывающее уведомление)
function showToast(text, durationMs = 3000) {
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.style.cssText = `
            position: fixed;
            top: 60px;
            right: 16px;
            z-index: 9999;
            display: flex;
            flex-direction: column;
            gap: 8px;
            pointer-events: none;
        `;
        document.body.appendChild(container);
    }

    const toast = document.createElement('div');
    toast.style.cssText = `
        background: #000;
        border: 1px solid var(--fg);
        color: var(--fg);
        padding: 10px 16px;
        font-family: var(--font-mono);
        font-size: 13px;
        max-width: 320px;
        box-shadow: 0 0 12px rgba(0,255,0,0.3);
        pointer-events: auto;
    `;
    toast.textContent = text;
    container.appendChild(toast);

    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transition = 'opacity 0.3s';
        setTimeout(() => toast.remove(), 300);
    }, durationMs);
}

//  Логи (для logs.html)
const LogBuffer = {
    lines: [],
    max: 500,

    add(line, cls = '') {
        this.lines.push({ line, cls, t: Date.now() });
        if (this.lines.length > this.max) {
            this.lines.shift();
        }
        this.render();
    },

    clear() {
        this.lines = [];
        this.render();
    },

    render() {
        const el = document.getElementById('logs');
        if (!el) return;

        const html = this.lines.map(l => {
            const cls = l.cls ? ` logs__line--${l.cls}` : '';
            return `<div class="logs__line${cls}">${esc(l.line)}</div>`;
        }).join('');

        el.innerHTML = html;
        el.scrollTop = el.scrollHeight;
    }
};

function notifyLog(line) {
    let cls = '';
    const l = line.toLowerCase();
    if (l.includes('error') || l.includes('fail')) cls = 'error';
    else if (l.includes('warn')) cls = 'warn';
    else if (l.includes('debug')) cls = 'dim';
    LogBuffer.add(line, cls);
}

//  Навигация — гамбургер
function initNav() {
    const btn = document.getElementById('nav-hamburger');
    const links = document.getElementById('nav-links');
    if (btn && links) {
        btn.onclick = () => links.classList.toggle('open');
    }
}

//  Подсветка активной ссылки в навигации
function initActiveLink() {
    const path = location.pathname.split('/').pop() || 'index.html';
    document.querySelectorAll('.nav__links a').forEach(a => {
        const href = a.getAttribute('href');
        if (href === path) a.classList.add('active');
    });
}

//  Мобильное меню — закрыть при клике
document.addEventListener('click', (e) => {
    const links = document.getElementById('nav-links');
    const btn = document.getElementById('nav-hamburger');
    if (!links || !btn) return;
    if (!links.classList.contains('open')) return;
    if (links.contains(e.target) || btn.contains(e.target)) return;
    links.classList.remove('open');
});

//  Универсальный слайдер (яркость)
function bindSlider(id, onInput) {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('input', () => onInput(Number(el.value)));
}

//  Поллинг API (для страниц без WebSocket-данных)
function startPolling(fn, intervalMs = CFG.pushIntervalMs) {
    fn();
    return setInterval(fn, intervalMs);
}

//  Drag-and-drop (для pages.html)
function initDragDrop(listEl, onReorder) {
    if (!listEl) return;

    let dragItem = null;

    listEl.querySelectorAll('.dnd-item').forEach(item => {
        item.draggable = true;

        item.addEventListener('dragstart', (e) => {
            dragItem = item;
            item.classList.add('dragging');
        });

        item.addEventListener('dragend', () => {
            item.classList.remove('dragging');
            dragItem = null;
            // Обновить номера
            renumber(listEl);
            if (onReorder) onReorder(getOrder(listEl));
        });

        item.addEventListener('dragover', (e) => {
            e.preventDefault();
            if (!dragItem || dragItem === item) return;
            item.classList.add('drag-over');
        });

        item.addEventListener('dragleave', () => {
            item.classList.remove('drag-over');
        });

        item.addEventListener('drop', (e) => {
            e.preventDefault();
            item.classList.remove('drag-over');
            if (!dragItem || dragItem === item) return;

            const items = Array.from(listEl.children);
            const dragIdx = items.indexOf(dragItem);
            const dropIdx = items.indexOf(item);

            if (dragIdx < dropIdx) {
                item.after(dragItem);
            } else {
                item.before(dragItem);
            }
        });
    });
}

function renumber(listEl) {
    listEl.querySelectorAll('.dnd-item').forEach((item, i) => {
        const num = item.querySelector('.dnd-item__num');
        if (num) num.textContent = (i + 1) + '.';
    });
}

function getOrder(listEl) {
    const ids = [];
    listEl.querySelectorAll('.dnd-item').forEach(item => {
        const id = item.getAttribute('data-id');
        if (id !== null) ids.push(Number(id));
    });
    return ids;
}

//  Хелпер: язык
function setLang(lang) {
    State.lang = lang;
    document.documentElement.lang = lang;
}

//  Инициализация при загрузке
document.addEventListener('DOMContentLoaded', () => {
    initNav();
    initActiveLink();
    wsConnect();

    // Начальный статус индикатора
    setStatusIndicator(false);

    console.log('[PC Monitor] frontend ready');
});

//  Экспорт (для использования в inline-скриптах страниц)
window.PCM = {
    CFG,
    State,
    // WebSocket
    wsSend,
    onData,
    // API
    apiGet,
    apiPost,
    // Утилиты
    fmtBytes,
    fmtGB,
    fmtUptime,
    fmtTime,
    esc,
    pctClass,
    setText,
    setHTML,
    // UI
    showConfirm,
    closeModal,
    showToast,
    showLog: notifyLog,
    LogBuffer,
    bindSlider,
    startPolling,
    // DnD
    initDragDrop,
    getOrder,
    // Прочее
    setLang,
};