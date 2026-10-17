# Распиновка ESP32

## Занятые пины

| GPIO | Компонент | Примечание |
|---|---|---|
| 4 | BTN1 | INPUT_PULLUP |
| 5 | BTN4 | INPUT_PULLUP |
| 13 | LED White | OUTPUT |
| 14 | BTN3 | INPUT_PULLUP |
| 16 | LED Blue | OUTPUT |
| 17 | LED Green | OUTPUT |
| 18 | LED Yellow | OUTPUT |
| 19 | LED Red | OUTPUT |
| 21 | I2C SDA (аппаратный) | OLED32 + BME280 |
| 22 | I2C SCL (аппаратный) | OLED32 + BME280 |
| 23 | BTN5 | INPUT_PULLUP |
| 25 | Soft I2C SDA | OLED64 |
| 26 | Soft I2C SCL | OLED64 |
| 27 | BTN2 | INPUT_PULLUP |
| 34 | LDR | ADC, input only |

## Свободные пины

GPIO 0, 2, 12, 15, 32, 33, 35 (input only), 36 (input only), 39 (input only)

**Не использовать:**

- GPIO 6–11 — flash
- GPIO 1, 3 — UART (если нужен Serial Monitor)

## Уровни

- **LED:** HIGH = горит, LOW = выключен
- **Кнопки:** LOW = нажата (INPUT_PULLUP)
- **LDR:** больше АЦП = ярче