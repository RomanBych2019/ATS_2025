#pragma once


// #define PRINTDEBUG
// #define verATP
#define verAnalogInput

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>
// #include <AsyncElegantOTA.h>
#include <freertos/task.h>
#include <FreeRTOSConfig.h>
#include <SPIFFS.h>
// #include <SPIFFSEditor.h>
#include "Preferences.h"
#include <SoftwareSerial.h>
#include <SimpleModbusSlave_DUE.h>
#include <RtcDS3231.h>
#include <GyverPortal.h>
#include <EEPROM.h>

#ifdef verAnalogInput
#include <Adafruit_ADS1X15.h>
#include "LS_ANALOG_U.h"
#include "LS_ANALOG_F.h"
#endif
#include "COUNTER.h"
#include "TARRING.h"
#include "TANK.h"
#include "NEXTION.h"
#include "Out.h"
#include "LS_RS485.h"
#include "LS_BLE.h"
#include "LS_EMPTY.h"

#define LittleFS SPIFFS

class LS_ANALOG_F;

#define serialLS Serial1
#define serialMB Serial2

extern const char *ssid;
extern const char *password;
extern const char *VER;

struct LoginPass
{
  char ssid[20];
  char pass[20];
};

extern LoginPass lp;

#define PLATE_v2 // PLATE_v1 - плата вер1,  PLATE_v2 - плата вер2 (2023)

// #ifdef PLATE_v1
// static const uint8_t INDI_F_PIN_ = GPIO_NUM_2;     // (2) индикатор включения частотного ДУТа
// static const uint8_t OUT_PUMP = GPIO_NUM_12;        // вывод управления насосом
// static const uint8_t INIDICATE_COUNT = GPIO_NUM_13; // вывод индикатора входных импульсов
// static const uint8_t IN_KCOUNT = GPIO_NUM_5;        // вход счетчика топлива
// static const uint8_t RXDNEX = GPIO_NUM_23;          //
// static const uint8_t TXDNEX = GPIO_NUM_19;          //
// static const uint8_t RXLS = GPIO_NUM_27;
// static const uint8_t TXLS = GPIO_NUM_14;
// #endif

#ifdef PLATE_v2
static const uint8_t INDI_F_PIN_ = GPIO_NUM_25;     // (25) индикатор включения частотного ДУТа
static const uint8_t OUT_PUMP = GPIO_NUM_12;        // вывод управления насосом
static const uint8_t INIDICATE_COUNT = GPIO_NUM_13; // вывод индикатора входных импульсов
static const uint8_t IN_KCOUNT = GPIO_NUM_5;        // вход счетчика топлива
static const uint8_t RXDNEX = GPIO_NUM_23;          //
static const uint8_t TXDNEX = GPIO_NUM_19;          //
static const uint8_t RXLS = GPIO_NUM_27;
static const uint8_t TXLS = GPIO_NUM_14;
#endif


// режимы работы
enum type
{
  CALIBR,      // Калибровка счетчика
  PUMPINGAUTO, // Откачка топлива автоматом
  TAR,         // Тарировка
  SETTING,     // Настройка тарировки
  MENU,        // Меню
  COUNT,       // Счетчик
  PUMPINGOUT,  // Выдача топлива
  END_TAR,
  END_TAR_HMI,
  MESSAGE,
  SEARCH_BLE,
  PAUSE,
  SET_DATE,
  SET_WEB
};

const uint8_t SIZE = 22;
union DateMod
{
  struct
  {
    unsigned int mode;                          //  1 режим работы станции
    unsigned int kRefillNum;                    //  2 номер пролива
    unsigned int v_full;                        //  3 объем залитого топлива всего в 0,1 литра 1-2 байт
    unsigned int k_v_full;                      //  3.1 объем залитого топлива всего в 0,1 литра 3-4 байт
    unsigned int resultNProliv;                 //  5 N тарируемого ДУТа зафиксированный станцией в проливах
    unsigned int id1;                           //  6 номер автомобиля 1-2 байт
    unsigned int id2;                           //  6.1 номер автомобиля 3-4 байт
    unsigned int resultN;                       //  8 N тарируемого ДУТА постоянно получаемы данные
    unsigned int adress;                        //  9 сетевой адресс ДУТа 1-2 байт
    unsigned int k_adress;                      //  9.1 сетевой адресс ДУТа 3-4 байт
    unsigned int vtank;                         //  11 объем тарируемого бака, литр
    unsigned int kRefill;                       //  12  количество проливов
    unsigned int flowRate;                      //  13  скорость потока, литр/мин
    unsigned int pause;                         //  14  длительность паузы между проливами, сек
    unsigned int k_in_Litr;                     //  15  количество импульсов на 10 литров
    unsigned int timetarring;                   //  16  время выполнения тарировки
    unsigned int typells;                       //  17  тип ДУТ , 0 - аналоговый_U , 1 - аналоговый_F , 2 - цифровой по rs485 , 3 - цифровой BLE
    unsigned int typetarring;                   //  18  режим тарировки 1 - автоматический, 0 - ручной
    unsigned int error;                         //  19  код ошибки
    unsigned int rssi;                          //  20  RSSI ДУТ BLE
    unsigned int llsATP;                        //  21  N ДУТа емкости АПТ (lls adr=100)
    bool controlFlowrate;                       //  22  Флаг контроля скорости потока
  };
  unsigned int au16data[SIZE];
};

extern DateMod datemod;
extern String errorStringWeb;                       //  строка ошибки
extern int counter_display_resetring;
extern volatile unsigned time_counter_imp;
const long MIN_DURATION = 500;
const uint16_t TIME_UPDATE_LLS = 10000;         // период обновления данных ДУТ
const uint16_t TIME_UPDATE_HMI = 300;           // период обновления данных на дисплее, мсек
const uint16_t TIME_UPDATE_SPEED_PUMP = 2000;   // период обновления скорости потока
const uint16_t TIME_PAUSE_END_TAR = 20000;      // пауза в конце тарировки для передаче данных в систему мониторинга

extern unsigned long start_pause, worktime, time_start_refill, time_LLS_update, time_stop_flow_rate;
extern bool autostop;
extern bool flag_HMI_send;
extern bool flag_conect_ok;                     // флаг удачного получения данных от ДУТ

struct WebData {
  int w_vtank_pumpingout = 0;
};

extern WebData web_data;

extern const char *LOG_FILE_NAME;

void rpmFun();
void modeMenu();
void modePumpOut();
void modeTarring();
void modePumpAuto();
void endTarring();
void endRefill();
void proceedTarring();
void errors();
void modbus();
void digitalpause();
void startPump();
void stopPump();
String makeLlsDateToDisplay(ILEVEL_SENSOR *_lls);
void onHMIEvent(String messege, String data, String response);
void exitTarring();
String saveLog();

void wifiInit();
void buildLoginPage();
void buildLoginPage(String wifi);
void loginPortal();

// void build();
void actionDownload();
// void action();
void action(GyverPortal &p);

void buildPage();
void actionPage();

void delete_lls();
String saveLog();
String deleteLog();

String listDir(fs::FS &fs, const char *dirname, uint8_t levels);

void updateLS(void *pvParameters);
void updateLS();
void readNextion(void *pvParameters);
void calculate_speedPump(void *pvParameters);
void onHMIEvent(String messege, String data, String response);
#ifdef PRINTDEBUG
void printDebugLog(void *pvParameters);
#endif

extern hw_timer_t *My_timer;
void IRAM_ATTR onTimer();

extern RtcDS3231<TwoWire> Rtc;

extern Preferences flash;
extern EspSoftwareSerial::UART serialHMI;

extern GyverPortal ui;

// ДУТ
extern ILEVEL_SENSOR *lls;
extern LS_RS485 *lls_RS485;
extern LS_BLE *lls_Ble;
extern LS_EMPTY *lls_Empty;
#ifdef verAnalogInput
extern Adafruit_ADS1115 ads; /* Use this for the 16-bit version */
extern LS_ANALOG_F *lls_analog_f;
extern LS_ANALOG_U *lls_analog_u;
#endif

#ifdef verATP
// ДУТ в емкости АТП
extern LS_RS485 *lls_ATP;
#endif

// насос
extern Out *pump;

// запасной выход
extern Out *out_tmp;

// счетчик
extern COUNTER *countV;

// бак
extern TANK *tank;

// тарировка
extern TARRING *tar;

// дисплей
extern NEXTION hmi;
