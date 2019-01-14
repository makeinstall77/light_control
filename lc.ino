//15-01-2019

#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "ds3231.h"
#include <EEPROM.h>

#define OLED_RESET 4
#define BUTTON 4
#define OPTICALSENSOR A2
#define ENCODER A3
#define LIGHTRELAY 10
#define BUFF_MAX 128

Adafruit_SSD1306 display(OLED_RESET);

int pinA = 3;                       // Пины прерываний
int pinB = 2;                       // Пины прерываний
volatile long pause    = 10;        // Пауза для борьбы с дребезгом
volatile long lastTurn = 0;         // Переменная для хранения времени последнего изменения
volatile int count = 0;             // Счетчик оборотов
volatile int state = 0;             // Статус одного шага - от 0 до 4 в одну сторону, от 0 до -4 - в другую
volatile int pinAValue = 0;         // Переменные хранящие состояние пина, для экономии времени
volatile int pinBValue = 0;         // Переменные хранящие состояние пина, для экономии времени
bool pressButton = false;           //
int encoderVal = 0;                 //
int hysVal = 10;                    // гистерезис
int menuLevel = 0;                  // вложеное меню, горизонтальный уровень
int menuItem = 0;                   // вложеное меню, вертикальный уровень
char cur[9] = ">       ";           // курсор меню
bool buttonRelease = false;         // чтобы при нажатии кнопки команда срабатывала только один раз
struct ts t;                        // часы
byte value;                         // для EEPROM
int address = 0;                    // для EEPROM
int h1 = 8;                         // начальное время, часы
int m1 = 0;                         // начальное время, минуты
int h2 = 22;                        // конечное время, часы
int m2 = 0;                         // конечное время, минуты
unsigned long relayStart = 0;       // время последнего переключания реле
char status[8];                     // строка статуса
unsigned long relayBorder = 900000; // 15 минутная задержка между переключениями реле
const byte averageFactor = 5;       // показатель сглаживания значений с датчика освещённости
unsigned long sensorTime = 0;       // для считывания показаний освещённости
int sensorValue = 0;                // показания с датчика освещённости

void setup() {
  value = EEPROM.read(address);
  if (value == 255) { //reset
    while (address < EEPROM.length()) {
      EEPROM.update(address, 0);
    }
  } else if (value == 1) { // если настройки сохранились, то в первой ячейке 1
    address++;
    EEPROM.get(address, hysVal);
    address += sizeof(int);                 // Корректируем адрес на уже записанное значение
    EEPROM.get(address, h1);
    address += sizeof(int);
    EEPROM.get(address, m1);
    address += sizeof(int);
    EEPROM.get(address, h2);
    address += sizeof(int);
    EEPROM.get(address, m2);
    address += sizeof(int);
  }
  pinMode(BUTTON, INPUT);
  pinMode(LIGHTRELAY, OUTPUT);
  digitalWrite(BUTTON, HIGH);              //pullup
  digitalWrite(LIGHTRELAY, LOW);
  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.clearDisplay();
  display.display();
  delay(500);
  attachInterrupt(1, A, CHANGE);  // Настраиваем обработчик прерываний по изменению сигнала
  attachInterrupt(0, B, CHANGE);  // Настраиваем обработчик прерываний по изменению сигнала
  Wire.begin();
  DS3231_init(DS3231_INTCN);
  sensorValue = map(analogRead(OPTICALSENSOR), 20, 900, 0, 100);
}

void loop() {
  DS3231_get(&t);
  encoderVal = map(analogRead(ENCODER), 0, 1023, 0, 100);
  menu();
  checkButtonPress();
  relayCheck();
}

int opticalSensor() {
  if (millis() - sensorTime > 5000) {
    if (averageFactor > 0) { //сглаживание данных с датчика
      int oldsensorValue = sensorValue;
      sensorValue = (oldsensorValue * (averageFactor - 1) + map(analogRead(OPTICALSENSOR), 20, 900, 0, 100)) / averageFactor;
    }
    sensorTime = millis();
  }
  return sensorValue;
}

void menu() {
  if (menuLevel == 0) {         // первый уровень
    if (count < 0) {
      count = 0;
    } else if (count == 0) {
      strcpy(cur, ">       ");
    } else if (count == 1) {
      strcpy(cur, " >      ");
    } else if (count == 2) {
      strcpy(cur, "  >     ");
    } else if (count == 3) {
      strcpy(cur, "   >    ");
    } else if (count == 4) {
      strcpy(cur, "    >   ");
    } else if (count == 5) {
      strcpy(cur, "     >  ");
    } else if (count == 6) {
      strcpy(cur, "      > ");
    } else if (count == 7) {
      strcpy(cur, "       >");
    } else if (count > 7) {
      count = 7;
    }
  } else if (menuLevel == 1) {  // второй уровень
    if (menuItem < 0) {
      menuItem = 0;
    } else if (menuItem == 0) {
      strcpy(cur, "H       ");
    } else if (menuItem == 1) {
      strcpy(cur, " H      ");
    } else if (menuItem == 2) {
      strcpy(cur, "  E     ");
    } else if (menuItem == 3) {
      strcpy(cur, "   E    ");
    } else if (menuItem == 4) {
      strcpy(cur, "    E   ");
    } else if (menuItem == 5) {
      strcpy(cur, "     E  ");
    } else if (menuItem == 6) {
      strcpy(cur, "      E ");
    } else if (menuItem == 7) {
      strcpy(cur, "       E");
    } else if (menuItem > 7) {
      count = 7;
    }
  } else if (menuLevel == 2) {  // третий уровень
    if (menuItem < 0) {
      menuItem = 0;
    } else if (menuItem == 0) {
      strcpy(cur, "M       ");
    } else if (menuItem == 1) {
      strcpy(cur, " M      ");
    } else if (menuItem == 2) {
      strcpy(cur, "  E     ");
    } else if (menuItem == 3) {
      strcpy(cur, "   E    ");
    } else if (menuItem == 4) {
      strcpy(cur, "    E   ");
    } else if (menuItem == 5) {
      strcpy(cur, "     E  ");
    } else if (menuItem == 6) {
      strcpy(cur, "      E ");
    } else if (menuItem == 7) {
      strcpy(cur, "       E");
    } else if (menuItem > 7) {
      count = 7;
    }
  } else if (menuLevel == 3) {  // четвертый уровень
    if (menuItem < 0) {
      menuItem = 0;
    } else if (menuItem == 0) {
      strcpy(cur, "E       ");
    } else if (menuItem == 1) {
      strcpy(cur, " H      ");
    } else if (menuItem == 2) {
      strcpy(cur, "  E     ");
    } else if (menuItem == 3) {
      strcpy(cur, "   E    ");
    } else if (menuItem == 4) {
      strcpy(cur, "    E   ");
    } else if (menuItem == 5) {
      strcpy(cur, "     E  ");
    } else if (menuItem == 6) {
      strcpy(cur, "      E ");
    } else if (menuItem == 7) {
      strcpy(cur, "       E");
    } else if (menuItem > 7) {
      count = 7;
    }
  } else if (menuLevel == 4) {  // пятый уровень
    if (menuItem < 0) {
      menuItem = 0;
    } else if (menuItem == 0) {
      strcpy(cur, "M       ");
    } else if (menuItem == 1) {
      strcpy(cur, " M      ");
    } else if (menuItem == 2) {
      strcpy(cur, "  E     ");
    } else if (menuItem == 3) {
      strcpy(cur, "   E    ");
    } else if (menuItem == 4) {
      strcpy(cur, "    E   ");
    } else if (menuItem == 5) {
      strcpy(cur, "     E  ");
    } else if (menuItem == 6) {
      strcpy(cur, "      E ");
    } else if (menuItem == 7) {
      strcpy(cur, "       E");
    } else if (menuItem > 7) {
      count = 7;
    }
  }
  // Вывод меню, нужно делать проверки пункта меню по горизонтали (menuLevelmenuLevel) и по вертикали (menuItemmenuItem)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  // --------------------------- Item 0, текущее время
  display.print(cur[0]);
  display.print(" Time: ");
  if (menuLevel == 1 && menuItem == 0) {
    if (count < 0) {
      count = 23;
    } else if (count > 23) {
      count = 0;
    } else {
      display.print(count);
    }
  } else {
    display.print(t.hour);
  }
  display.print(":");
  if (menuLevel == 2 && menuItem == 0) {
    if (count < 0) {
      count = 59;
    } else if (count > 59) {
      count = 0;
    } else {
      display.println(count);
    }
  } else {
    display.println(t.min);
  }

  // --------------------------- Item 1, границы светового дня
  display.print(cur[1]);
  display.print(" SET: ");
  if (menuLevel == 1 && menuItem == 1) {
    if (count < 0) {
      count = 23;
    } else if (count > 23) {
      count = 0;
    } else {
      display.print(count);
    }
  } else {
    display.print(h1);
  }
  display.print(":");
  if (menuLevel == 2 && menuItem == 1) {
    if (count < 0) {
      count = 59;
    } else if (count > 59) {
      count = 0;
    } else {
      display.print(count);
    }
  } else {
    display.print(m1);
  }
  display.print(" - ");
  if (menuLevel == 3 && menuItem == 1) {
    if (count < 0) {
      count = 23;
    } else if (count > 23) {
      count = 0;
    } else {
      display.print(count);
    }
  } else {
    display.print(h2);
  }
  display.print(":");
  if (menuLevel == 4 && menuItem == 1) {
    if (count < 0) {
      count = 59;
    } else if (count > 59) {
      count = 0;
    } else {
      display.println(count);
    }
  } else {
    display.println(m2);
  }

  // --------------------------- Item 2, данные с датчика освещённости
  display.print(cur[2]);
  display.print(" Sensor: ");
  display.println(opticalSensor());

  // --------------------------- Item 3, граница включения реле
  display.print(cur[3]);
  display.print(" Border = ");
  display.println(encoderVal);

  // --------------------------- Item 4, статус реле
  display.print(cur[4]);
  if (digitalRead(LIGHTRELAY) == HIGH) {
    display.println(" Relay: on");
  } else {
    display.println(" Relay: off");
  }

  // --------------------------- Item 5, прошло времени с момента переключения реле
  display.print(cur[5]);
  display.print(" RTime: ");
  display.print(round((millis() - relayStart) / 60000));
  display.println(" min");

  // --------------------------- Item 6, гистерезис
  display.print(cur[6]);
  display.print(" hyst: ");
  if (menuLevel == 1 && menuItem == 6) {
    display.println(count);
  } else {
    display.println(hysVal);
  }

  // --------------------------- Item 7, строка статуса
  display.print(cur[7]);
  display.print(" status: ");
  display.println(status);
  display.display();
}

void checkButtonPress() {
  if (digitalRead(BUTTON)) {
    buttonRelease = true;
  } else if (buttonRelease) {
    if (menuLevel == 0) {        // верхнее меню, движение по горизонтали ->
      menuLevel = 1;
      menuItem = count;
      if (menuItem == 0) {
        count = t.hour;
      } else if (menuItem == 1) {
        count = h1;
      } else if (menuItem == 6) {
        count = hysVal;
      }
    } else if (menuLevel == 1) { // меню редактирования 1
      if (menuItem == 0) {            // текущие часы
        t.hour = count;
        DS3231_set(t);
        menuLevel = 2;
        count = t.min;
      } else if (menuItem == 1) {     // часы начала светового дня
        h1 = count;
        menuLevel = 2;
        count = m1;
      } else if (menuItem == 6) {     // гистерезис
        hysVal = count;
        menuLevel = 0;
        count = menuItem;
      } else {                        // все остальные пункты, просто возврат обратно
        menuLevel = 0;
        count = menuItem;
      }
    } else if (menuLevel == 2) { // меню редактирования 2
      if (menuItem == 0) {            // текущие минуты
        t.min = count;
        DS3231_set(t);
        menuLevel = 0;
        count = menuItem;
      } else if (menuItem == 1) {     // минуты начала светового дня
        m1 = count;
        menuLevel = 3;
        count = h2;
      }
    } else if (menuLevel == 3) { // меню редактирования 3
      if (menuItem == 1) {            // часы конца светового дня
        h2 = count;
        menuLevel = 4;
        count = m2;
      }
    } else if (menuLevel == 4) { // меню редактирования 4
      if (menuItem == 1) {            // минуты конца светового дня
        m2 = count;
        menuLevel = 0;
        count = menuItem;
      }
    }

    address = 0;                // сохранение настроек
    EEPROM.update(address, 1);
    address ++;
    EEPROM.put(address, hysVal);
    address += sizeof(int);     // Корректируем адрес на уже записанное значение
    EEPROM.put(address, h1);
    address += sizeof(int);
    EEPROM.put(address, m1);
    address += sizeof(int);
    EEPROM.put(address, h2);
    address += sizeof(int);
    EEPROM.put(address, m2);
    address += sizeof(int);
    buttonRelease = false;
  }
}

void relayCheck() { //управление реле
  if ((t.hour > h1 && t.hour < h2) || (t.hour == h1 && t.min > m1) || (t.hour == h2 && t.min < m2)) {
    if (millis() - relayStart > relayBorder) {
      if (opticalSensor() > encoderVal + hysVal) { //больше
        digitalWrite(LIGHTRELAY, LOW); //off
        relayStart = millis();
      } else if (opticalSensor() < encoderVal - hysVal) { //меньше
        digitalWrite(LIGHTRELAY, HIGH); //on
        relayStart = millis();
      } else {
        // none
      }
    }
    strcpy(status, "working");
  } else {
    digitalWrite(LIGHTRELAY, LOW); //off, когда закончился световой день, нужно отключить
    strcpy(status, "waiting");
  }
}

void A() {
  if (micros() - lastTurn < pause) return;  // Если с момента последнего изменения состояния не прошло
  // достаточно времени - выходим из прерывания
  pinAValue = digitalRead(pinA);            // Получаем состояние пинов A и B
  pinBValue = digitalRead(pinB);

  cli();    // Запрещаем обработку прерываний, чтобы не отвлекаться
  if (state  == 0  && !pinAValue &&  pinBValue || state == 2  && pinAValue && !pinBValue) {
    state += 1; // Если выполняется условие, наращиваем переменную state
    lastTurn = micros();
  }
  if (state == -1 && !pinAValue && !pinBValue || state == -3 && pinAValue &&  pinBValue) {
    state -= 1; // Если выполняется условие, наращиваем в минус переменную state
    lastTurn = micros();
  }
  setCount(state); // Проверяем не было ли полного шага из 4 изменений сигналов (2 импульсов)
  sei(); // Разрешаем обработку прерываний

  if (pinAValue && pinBValue && state != 0) state = 0; // Если что-то пошло не так, возвращаем статус в исходное состояние
}

void B() {
  if (micros() - lastTurn < pause) return;
  pinAValue = digitalRead(pinA);
  pinBValue = digitalRead(pinB);

  cli();
  if (state == 1 && !pinAValue && !pinBValue || state == 3 && pinAValue && pinBValue) {
    state += 1; // Если выполняется условие, наращиваем переменную state
    lastTurn = micros();
  }
  if (state == 0 && pinAValue && !pinBValue || state == -2 && !pinAValue && pinBValue) {
    state -= 1; // Если выполняется условие, наращиваем в минус переменную state
    lastTurn = micros();
  }
  setCount(state); // Проверяем не было ли полного шага из 4 изменений сигналов (2 импульсов)
  sei();

  if (pinAValue && pinBValue && state != 0) state = 0; // Если что-то пошло не так, возвращаем статус в исходное состояние
}

void setCount(int state) {          // Устанавливаем значение счетчика
  if (state == 4 || state == -4) {  // Если переменная state приняла заданное значение приращения
    count += (int)(state / 4);      // Увеличиваем/уменьшаем счетчик
    lastTurn = micros();            // Запоминаем последнее изменение
  }
}
