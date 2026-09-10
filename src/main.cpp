#include "main.h"
#ifdef verAnalogInput
#include "LS_ANALOG_U.h"
#include "LS_ANALOG_F.h"
#endif
#include "LS_RS485.h"
#include "LS_BLE.h"
#include "LS_EMPTY.h"
#include "HmiRenderer.h"
// #include <TimeUtil.h>

const char *ssid = "WiFi ATS";
const char *password = "00000001";
const char *VER = "2025_3.0 web 2.0";

LoginPass lp;
DateMod datemod;

String errorStringWeb {};
int counter_display_resetring = 0;
volatile unsigned time_counter_imp = 0;

unsigned long start_pause, worktime, time_start_refill, time_LLS_update, time_stop_flow_rate;
bool autostop = false;
bool flag_HMI_send = false;
bool flag_conect_ok = true;

WebData web_data;

const char *LOG_FILE_NAME = "log.csv";

hw_timer_t *My_timer = NULL;
RtcDS3231<TwoWire> Rtc(Wire);
Preferences flash;
EspSoftwareSerial::UART serialHMI;
GyverPortal ui(&LittleFS);

ILEVEL_SENSOR *lls;
LS_RS485 *lls_RS485;
LS_BLE *lls_Ble;
LS_EMPTY *lls_Empty;
#ifdef verAnalogInput
Adafruit_ADS1115 ads;
LS_ANALOG_F *lls_analog_f;
LS_ANALOG_U *lls_analog_u;
#endif

#ifdef verATP
LS_RS485 *lls_ATP;
#endif

Out *pump;
Out *out_tmp;
COUNTER *countV;
TANK *tank;
TARRING *tar;
NEXTION hmi(serialHMI);

static volatile bool countIndicatorTimerActive = false;

static void setupCountIndicatorTimer()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    My_timer = timerBegin(1000000);
    timerAttachInterrupt(My_timer, &onTimer);
#else
    My_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(My_timer, &onTimer, true);
    timerAlarmWrite(My_timer, 1000000, false);
#endif
}

static bool isCountIndicatorTimerActive()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return countIndicatorTimerActive;
#else
    return timerAlarmEnabled(My_timer);
#endif
}

static void startCountIndicatorTimer()
{
    countIndicatorTimerActive = true;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    timerWrite(My_timer, 0);
    timerAlarm(My_timer, 1000000, false, 0);
#else
    timerAlarmEnable(My_timer);
#endif
}

void setup()
{
    pinMode(GPIO_NUM_2, OUTPUT);
    pinMode(INDI_F_PIN_, OUTPUT);
    pinMode(INIDICATE_COUNT, OUTPUT);

    // индикация начала загрузки
    digitalWrite(INDI_F_PIN_, HIGH);
    digitalWrite(INIDICATE_COUNT, HIGH);
    digitalWrite(GPIO_NUM_2, HIGH);

    Serial.begin(115200);
    serialLS.begin(19200, SERIAL_8N1, RXLS, TXLS);
    serialHMI.begin(19200, SWSERIAL_8N1, RXDNEX, TXDNEX, false, 256);
    serialMB.begin(19200);

    hmi.echoEnabled(false);
    hmi.hmiCallBack(onHMIEvent);
    hmi("rest");

    Rtc.Begin();

#ifdef verAnalogInput
    ads.setGain(GAIN_ONE);
    ads.begin();
    lls_analog_u = new LS_ANALOG_U(ads, 2);
    lls_analog_f = new LS_ANALOG_F();
#endif

    flash.begin("eerom", false);
    int k = flash.getInt("impulse_count", 1680); // чтение из eerom значения K счетчика
    countV = new COUNTER(k);

    lls_Empty = new LS_EMPTY();
    lls = lls_Empty;

    tank = new TANK(countV);
    tar = new TARRING(countV, tank);
    pump = new Out(OUT_PUMP);

    lls_RS485 = new LS_RS485(&serialLS, 1);
    lls_Ble = new LS_BLE();
    lls_Ble->echoEnabled(false);

    pinMode(IN_KCOUNT, INPUT_PULLUP);           // инициализация входа импульсов ДАРТ
    attachInterrupt(IN_KCOUNT, rpmFun, CHANGE); // функция прерывания

    modbus_configure(&serialMB, 19200, 1, 0, SIZE, datemod.au16data);
    // modbus_update_comms(19200, 1);

#ifdef verATP
    lls_ATP = new LS_RS485(&serialLS, 100);
#endif

    // // страница  списка файлов
    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send(200, "text/html", "<p>ATS 2023</p><p>" + String(__DATE__) + "</p><p>" + listDir(SPIFFS, "/", 0) + "</p>"); });

    // // скачивание лог-файла
    // server.on("/log.csv", HTTP_GET, [file = "/log.csv"](AsyncWebServerRequest *request)
    //           { getDataLog(request, file); });

    // // удаление файла логов
    // server.on("/delete", [](AsyncWebServerRequest *request)
    //           { request->send(200, "text/html", "<p>ATS - delete log file</p>" + deleteLog()); });

    wifiInit();

    if (!LittleFS.begin())
        Serial.println("FS Error");

    log_e("Ver: %s", VER);

    ui.attachBuild(buildPage);
    ui.attach(actionDownload);
    ui.attach(actionPage);
    ui.start("ATS");
    ui.downloadAuto(0); // отключить авто скачивание
    ui.enableOTA();

    xTaskCreatePinnedToCore(
        calculate_speedPump,        /* Вычисление скорости потока */
        "Task_calculate_speedPump", /* Название задачи */
        4096,                       /* Размер стека задачи */
        NULL,                       /* Параметр задачи */
        1,                          /* Приоритет задачи */
        NULL,                       /* Идентификатор задачи, чтобы ее можно было отслеживать */
        1);                         /* Ядро для выполнения задачи (0) */

    xTaskCreatePinnedToCore(
        updateLS,        /* */
        "Task_updateLS", /* Обновление данных от ДУТ */
        16364,           /* Размер стека задачи */
        NULL,            /* Параметр задачи */
        1,               /* Приоритет задачи */
        NULL,            /* Идентификатор задачи, чтобы ее можно было отслеживать */
        0);              /* Ядро для выполнения задачи (0) */

#ifdef PRINTDEBUG
    xTaskCreatePinnedToCore(
        printDebugLog,        /*  */
        "Task_printDebugLog", /* Печать отладочной информации*/
        4096,                 /* Размер стека задачи */
        NULL,                 /* Параметр задачи */
        3,                    /* Приоритет задачи */
        NULL,                 /* Идентификатор задачи, чтобы ее можно было отслеживать */
        tskNO_AFFINITY);      /* Ядро для выполнения задачи (0) */
#endif

    xTaskCreatePinnedToCore(
        sendNextion,        /* обновление данных HMI */
        "Task_sendNextion", /* Название задачи */
        8192,               /* Размер стека задачи */
        NULL,               /* Параметр задачи */
        4,                  /* Приоритет задачи */
        NULL,               /* Идентификатор задачи, чтобы ее можно было отслеживать */
        1);

    xTaskCreatePinnedToCore(
        readNextion,        /* чтение данных от HMI */
        "Task_readNextion", /* Название задачи */
        8192,               /* Размер стека задачи */
        NULL,               /* Параметр задачи */
        5,                  /* Приоритет задачи */
        NULL,               /* Идентификатор задачи, чтобы ее можно было отслеживать */
        1);

    datemod.controlFlowrate = true;
    datemod.mode = MENU;

    // индикация окончания загрузки
    digitalWrite(INDI_F_PIN_, LOW);
    digitalWrite(INIDICATE_COUNT, LOW);
    digitalWrite(GPIO_NUM_2, LOW);

    // таймер для светодиода индикации счетчика
    setupCountIndicatorTimer();
}

void loop()
{
    errors();
    modbus();
    hmi.listen();
    ui.tick();

    switch (datemod.mode)
    {
    case MENU:
        modeMenu();
        break;
    case PUMPINGAUTO:
        modePumpAuto();
        break;
    case PAUSE:;
    case TAR:
        modeTarring();
        break;
    case PUMPINGOUT:
        modePumpOut();
        break;
    case END_TAR:
        exitTarring();
        break;
    case MESSAGE:
        stopPump();
        break;
    case SEARCH_BLE:
        break;
    }

    if (lls->getType() == ILEVEL_SENSOR::ANALOGE_F)
        digitalWrite(INDI_F_PIN_, HIGH);
    else
        digitalWrite(INDI_F_PIN_, LOW);
}

// void test()
// {
//   if (pump->get() == ON)
//   {
//     rpmFun();
//   }
// }

/*  ---------- Режим Меню ---------- */
void modeMenu()
{
    tar->reset();
    pump->off();
    datemod.id1 = 0;
    datemod.id2 = 0;
    counter_display_resetring = 0;
    autostop = false;
    flag_conect_ok = true;
}

/*  ---------- Режим Автоматическая выдача топлива ---------- */
void modePumpOut()
{
    if (tar->getVTank() <= tar->getVfuel())
        pump->off();
}

/*  ---------- Режим Тарировка ---------- */
void modeTarring()
{
    if (tar->getCountReffil() == 0)
    {
        if (tar->getVTank() > 200 && tar->getNumRefill() > 4)
            tar->saveResultRefuil(lls);
        else
            hmi("page menu"); //   возврат в меню при некорректных данных
    }

    if (tar->getVTank() <= tar->getVfuel()) // условие окончания тарировки
        endTarring();

    if (tar->getVTankRefill() * tar->getCountReffil() <= tar->getVfuel() && tar->getCountReffil() != tar->getNumRefill()) // условие окончания очередного пролива
        endRefill();
}

/*  ---------- Режим Автоматическое выкачивание ---------- */
void modePumpAuto()
{
    if (millis() - worktime > 30000)
    {
        if (pump->get() == ON && countV->getFlowRate() < 5) // скорость потока менее 5 л/мин
            pump->off();
    }
    if (pump->get() == OFF)
        worktime = millis();
}

/*  ---------- Продолжение тарировки  ---------- */
void proceedTarring()
{
    tar->saveResultRefuil(lls); // запись результата пролива
    errors();
    if (datemod.error == 32)
        return;
    autostop = false;
    time_start_refill = millis();
    datemod.mode = TAR;
    pump->on();
}

/*  ---------- Окончание очередного пролива  ---------- */
void endRefill()
{
    stopPump();
    if (datemod.mode == TAR && autostop == false)
    {
        autostop = true;
        start_pause = millis();
        datemod.mode = PAUSE;
    }

    if (millis() - start_pause < 5000)
        return;

    if (countV->getFlowRate() > 0) // проверка, что топливо больше не поступает в бак
    {
        delay(100);
        return;
    }

    if (tar->getTimePause() == 0) // пауза если ДУТ цифровой
        digitalpause();
    else if (millis() - start_pause > tar->getTimePause() * 60000) // пауза если ДУТ аналоговый
        proceedTarring();
}

/*  ---------- Финал тарировки  ---------- */
void endTarring()
{
    autostop = true;
    stopPump();

    while (countV->getFlowRate() > 0) // проверка, что топливо больше не поступает в бак
        delay(1000);
    exitTarring();
}

/*  ---------- Выход из тарировки  ---------- */
void exitTarring()
{
    if (datemod.mode != END_TAR)
    {
        datemod.mode = END_TAR;
        if (tar->getTimePause() == 0)
            if (lls->getVecLevel()->size() == 0)
            {
                delay(100);
                return;
            }
        // start_pause = millis() + TIME_PAUSE_END_TAR / 4;
        tar->saveResultRefuil(lls);
    }
    // if (start_pause - millis() > 50)
    //     return;
    hmi("page t_end");
    // delay(200);
    datemod.mode = END_TAR_HMI;
}

/*  ---------- Считывание данных с ДУТ  ---------- */
void updateLS(void *pvParameters)
{
    for (;;)
    {
#ifdef verATP
        if (datemod.mode == MENU)
            lls_ATP->update();
#endif
        if (datemod.mode == TAR || datemod.mode == PAUSE || datemod.mode == COUNT)
            if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
            {
                lls->update();
                // test();
                digitalWrite(GPIO_NUM_2, ON);
                delay(10);
                digitalWrite(GPIO_NUM_2, OFF);
            }

        vTaskDelay(pdMS_TO_TICKS(TIME_UPDATE_LLS));
    }
    vTaskDelete(NULL);
}

/*  ---------- Считывание данных с ДУТ  ---------- */
void updateLS()
{
#ifdef verATP
    if (datemod.mode == MENU)
        lls_ATP->update();
#endif

    if (datemod.mode == TAR || datemod.mode == PAUSE || datemod.mode == COUNT)
        if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
        {
            lls->update();
            // test();
        }
}

/*  ---------- Вычисление скорости потока  ---------- */
void calculate_speedPump(void *pvParameters)
{
    for (;;)
    {
        countV->updateFlowRate();
        vTaskDelay(pdMS_TO_TICKS(TIME_UPDATE_SPEED_PUMP));
    }
    vTaskDelete(NULL);
}

/*  ---------- Ошибки ДУТ  ---------- */
void errors()
{
    int error = 0;
    if (datemod.mode == MESSAGE || datemod.mode == END_TAR || datemod.mode == CALIBR || datemod.mode == MENU)
        return;

    if (lls->getType() != ILEVEL_SENSOR::NO_LLS) // если ДУТ подключен
    {
        if (datemod.mode == SEARCH_BLE) // ДУТ BLE и режим поиск
            return;

        // if (tar->getType() == tarring::AUTO) //  если тарировка в автоматическом режиме
        if (tar->getTimePause() == 0)
        {
            error = lls->getError(); // чтение ошибок ДУТ

            if (datemod.mode == TAR)
                if (tar->getCountReffil() > 2) // проверка увеличения данных с ДУТа в проливах
                    if (tar->getNRefill(tar->getCountReffil() - 1) < 10 + tar->getNRefill(tar->getCountReffil() - 2))
                        error |= 1 << 5;

            if (datemod.mode == SETTING) // проверка, что тарировка начинается с приемлемого уровня ДУТ
                if (lls->getLevel() > lls->getLevelStart())
                    error |= 1 << 7;
        }
    }

    // проверка, что 30 секунд скорость пролива меньше 2л/мин
    if (datemod.controlFlowrate)
    {
        if (pump->get() == ON)
        {
            if (countV->getFlowRate() < 2)
            {
                if (millis() > pump->getTimeStart() + 30000)
                    error |= 1 << 6;
            }
            else
                pump->setTimeStart();
        }
    }
    if (error)
    {
        // Serial.printf("\nErr: %d", error);
        if (datemod.mode != MESSAGE)
            hmi("page message");
        datemod.mode = MESSAGE;
        pump->off();
        datemod.error = error;
    }
}

/*  ---------- Функция подсчета импульсов с ДАРТ  ---------- */
void rpmFun()
{
    if (micros() - time_counter_imp > MIN_DURATION)
    {
        countV->setKcount();
        if (countV->getK() % 20 == 0 && !isCountIndicatorTimerActive())
        { 
            // digitalWrite(INIDICATE_COUNT, !digitalRead(INIDICATE_COUNT));
            digitalWrite(INIDICATE_COUNT, HIGH);
            startCountIndicatorTimer();
        }
    }
    time_counter_imp = micros();
}

/*  ---------- Парсинг полученых данных от дисплея Nextion  ---------- */
void onHMIEvent(String messege, String data, String response)
{
    if (messege.isEmpty())
        return;

    switch (messege.toInt())
    {
    case 1:
        datemod.mode = MENU;
        break;
    case 2:
        datemod.mode = CALIBR;
        break;
    case 3:
        datemod.mode = COUNT;
        break;
    case 7:
        datemod.mode = SEARCH_BLE;
        break;
    case 8:
        if (!autostop)
            datemod.mode = TAR;
        else if (datemod.mode != END_TAR)
            datemod.mode = PAUSE;
        break;
    case 9:
        datemod.mode = PUMPINGAUTO;
        break;
    case 10:
        datemod.mode = PUMPINGOUT;
        break;
    case 11:
        datemod.mode = END_TAR_HMI;
        break;
    case 12:
        datemod.mode = MESSAGE;
        break;
    case 13:
        datemod.mode = SETTING;
        break;
    default:
        break;
    }

    /*  ----------  Экран Меню  ---------- */
    if (messege == "menu")
    {
        int k = flash.getInt("impulse_count", 1680); // чтение из eerom значения K счетчика
        countV->setKinLitr(k);
        datemod.error = 0;
        if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
        {
            // сброс ДУТа BLE без номера и не найденного в поиске
            if (lls->getNameBLE() == "" || lls->getError() == ILEVEL_SENSOR::error::NOT_FOUND)
                delete_lls();
        }
    }

#ifdef verAnalogInput
    else if (messege == "au!")
    {
        lls = lls_analog_u;
        lls->search();
    }

    else if (messege == "ag!")
    {
        lls = lls_analog_f;
        digitalWrite(INDI_F_PIN_, HIGH);
        lls->search();
    }
#endif

    else if (messege == "rs485!")
    {
        lls = lls_RS485;
        if (lls->search())
            tar->setTimePause(0);
    }

    else if (messege == "ble!")
    {
        lls = lls_Ble;
        lls->newBLE("");
    }

    else if (messege == "no_lls!")
    {
        delete_lls();
    }

    else if (messege == "TD_") // получение имени ДУТа BLE Эскорт
    {
        lls->newBLE(messege + data);
        lls->search();
    }

    else if (messege == "pump")
    {
        pump->get() ? stopPump() : startPump();
    }

    else if (messege == "endtarr") // окончание тарировки
    {
        exitTarring();
    }

    else if (messege == "save") // сохранение тарировки
    {
        saveLog();
    }

    else if (messege == "resetring")
    {
        if (tar->getVfuel())
            tar->saveResultRefuil(lls);
        return;
    }
    else if (messege == "reset")
    {
        tar->reset();
        counter_display_resetring = -1;
    }

    else if (messege == "CALIBR")
    {
        data.toInt() ? countV->reset() : stopPump();
    }

    else if (messege == "k_count") // получение промежуточного значения k_in_Litr
    {
        countV->setKinLitrCalibr(data.toInt());
    }

    else if (messege == "save_k") // получение нового К, запись в память
    {
        flash.putInt("impulse_count", data.toInt()); // запись в eerom значения K счетчика
        countV->setKinLitr(data.toInt());
    }
    else if (messege == "id") // получение номера тарируемого объекта
    {
        tar->setId(data);
        tar->setTStart(Rtc.GetDateTime());
    }
    else if (messege == "vtank") // получение объема бака
    {
        tank->setVTank(data.toInt() * 10);
    }
    else if (messege == "qt") // получение количества проливов
    {
        tar->setNumRefill(data.toInt());
    }

    /*  ----------  Получениее времени  ---------- */
    if (messege == "date")
    {
        auto index_sym = data.indexOf(';');
        auto DATE = data.substring(0, index_sym);
        auto TIME = data.substring(index_sym + 1, data.length());
        // Serial.printf("Date %s Time %s\n", DATE, TIME);

        // Для установки  времени
        RtcDateTime compiled = RtcDateTime(DATE.c_str(), TIME.c_str());
        Rtc.SetDateTime(compiled);
    }

    if (messege == "pause")
    {
        if (data.toInt())
        {
            tar->setTimePause(data.toInt());
        }
        else
        {
            // tar->setType(tarring::AUTO);
            if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
                tar->setTimePause(0);
            else
                hmi("select0.val", tar->getTimePause());
        }
    }

    if (messege == "clear_err")
    {
        // switch (datemod.error)
        // {
        // case ILEVEL_SENSOR::NOT_FOUND:
        //   datemod.mode = TAR;
        //   if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
        //     if (lls->getType() == ILEVEL_SENSOR::RS485)
        //     {
        //       Serial.println("Дут RS485 не найден. Ищем...");
        //       lls->search();
        //     }
        //     if (lls->getType() == ILEVEL_SENSOR::BLE_ESKORT)
        //     {
        //       Serial.println("Дут BLE не найден. Ищем...");
        //       lls->search();
        //     }
        //   datemod.error = 0;
        //   autostop = false;
        //   break;
        // case ILEVEL_SENSOR::LOST:
        //   datemod.mode = TAR;
        //   if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
        //     if (lls->getType() == ILEVEL_SENSOR::RS485)
        //     {
        //       Serial.println("Дут RS485 потерян. Ищем...");
        //       lls->searchLost();
        //     }
        //     if (lls->getType() == ILEVEL_SENSOR::BLE_ESKORT)
        //     {
        //       Serial.println("Дут BLE потерян. Ищем...");
        //       lls->search();
        //     }
        //   datemod.error = 0;
        //   autostop = false;
        //   break;

        // default:
        //   break;
        // }
        if (datemod.error == 32)
            tar->deleteResultRefuil(lls);

        datemod.error = 0;
        lls->clearError();
    }

    if (messege == "contrFlowrate")
    {
        if (data == "1")
            datemod.controlFlowrate = true;
        if (data == "0")
            datemod.controlFlowrate = false;
    }
}

/*  ---------- данные с Nextion ---------- */
void readNextion(void *pvParameters)
{
    for (;;)
    {
        hmi.listen();
        // vPrintString("readNextion");
        // vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TIME_UPDATE_HMI));
        vTaskDelay(pdMS_TO_TICKS(TIME_UPDATE_HMI));
    }
    vTaskDelete(NULL);
}

#ifdef PRINTDEBUG
void printDebugLog(void *pvParameters)
{
    for (;;)
    {
        Serial.println();
        Serial.printf("Mem: %d", ESP.getFreeHeap());
        Serial.println("   _____________");
        Serial.printf("\nРежим работы\t\t");
        switch (datemod.mode)
        {
        case PUMPINGAUTO:
            Serial.print("Откачка топлива автоматом\n");
            break;
        case CALIBR:
            Serial.print("Калибровка счетчика\n");
            break;
        case MENU:
            Serial.print("Меню\n");
            break;
        case TAR:
            Serial.print("Тарировка\n");
            break;
        case SETTING:
            Serial.print("Настройка тарировки\n");
            break;
        case COUNT:
            Serial.print("Счетчик\n");
            break;
        case PUMPINGOUT:
            Serial.print("Выдача топлива\n");
            break;
        case END_TAR:
            Serial.print("Конец тарировки\n");
            break;
        case PAUSE:
            Serial.print("Пауза\n");
            break;
        case MESSAGE:
            Serial.print("Собщения\n");
            break;
        case SEARCH_BLE:
            Serial.print("Поиск BLE\n");
            break;
        default:
            Serial.print(datemod.mode + "\n");
        }
        // Serial.printf("\nТекущее значение К\t%d", countV->getKinLitr());
        Serial.printf("Объем бака\t\t%d\n", tar->getVTank());
        Serial.printf("Залито топлива\t\t%d\n", tar->getVfuel());
        Serial.printf("Номер автомобиля\t%s\n", tar->getId());
        Serial.printf("Объем проливов\t\t%d\n", tar->getVTankRefill());
        Serial.printf("Количество проливов\t%d\n", tar->getNumRefill());
        Serial.printf("Пауза между проливами\t%d\n", tar->getTimePause());
        Serial.print("Тип тарировки\t\t");
        if (tar->getTimePause())
            Serial.println("ручная");
        else
            Serial.println("автомат.");
        Serial.print("Тип ДУТа\t\t");
        if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
        {
            switch (lls->getType())
            {
            case ILEVEL_SENSOR::ANALOGE_U:
                Serial.print("аналоговый U\n");
                break;
            case ILEVEL_SENSOR::ANALOGE_F:
                Serial.print("аналоговый F\n");
                break;
            case ILEVEL_SENSOR::RS485:
                Serial.print("цифровой RS485\n");
                break;
            case ILEVEL_SENSOR::BLE_ESKORT:
                Serial.print("цифровой BLE\n");
                break;
            }

            Serial.printf("Адрес ДУТа\t\t%d\n", lls->getNetadres());
            Serial.printf("Значение N ДУТа\t\t%d\n", lls->getLevel());
            Serial.printf("Уровень сигнала ble\t%d\n", lls->getRSSI());
        }
        else
            Serial.printf(("не выбран\n"));
        // Serial.printf("\nНомер пролива\t\t%d", tar->getCountReffil());
        Serial.printf("Ошибки\t\t\t%d\n", datemod.error);
        Serial.printf("ДУТ АТП\t\t\t%d\n", lls_ATP->getLevel());

#ifdef verATP
        Serial.printf("Ошибки LLS АТП \t\t%d", lls_ATP->getError());
#endif
        Serial.println();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelete(NULL);
}
#endif

/*  ---------- Обновление данных таблицы modbus ---------- */
void modbus()
{
    modbus_update();
    if (tar->getNumRefill() >= tar->getVRefill()->size())
        datemod.kRefillNum = tar->getVRefill()->size();
    if (millis() > time_start_refill + 12000) // задержка в обновлении объема топлива для корректного составления отчета в Виалоне
    {
        datemod.v_full = tank->getFuelInTank();
        datemod.k_v_full = (tank->getFuelInTank() >> 16);
    }
    datemod.id1 = tar->getId_int();
    datemod.id2 = tar->getId_int() >> 16;
    datemod.k_in_Litr = countV->getKinLitr();
    datemod.vtank = tar->getVTank() / 10;
    datemod.kRefill = tar->getNumRefill();
    datemod.flowRate = countV->getFlowRate();
    datemod.pause = tar->getTimePause();
    datemod.timetarring = tar->getTimeTarring();
    // datemod.typetarring = tar->getType();
    if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
    {
        datemod.typells = lls->getType();
        if (lls->getType() == ILEVEL_SENSOR::type::RS485)
        {
            datemod.adress = lls->getNetadres();
            datemod.k_adress = lls->getNetadres() >> 16;
        }
        if (lls->getType() == ILEVEL_SENSOR::type::BLE_ESKORT)
        {
            datemod.adress = lls->getNameBLE_int();
            datemod.k_adress = lls->getNameBLE_int() >> 16;
            datemod.rssi = lls->getRSSI();
        }
        datemod.resultN = lls->getLevel();
        datemod.resultNProliv = tar->getBackNRefill();
    }
    else
    {
        datemod.typells = ILEVEL_SENSOR::type::NO_LLS;
        datemod.adress = 0;
        datemod.k_adress = 0;
        datemod.resultN = 0;
        datemod.resultNProliv = 0;
        datemod.adress = 0;
        datemod.k_adress = 0;
        datemod.rssi = 0;
    }

#ifdef verATP
    datemod.llsATP = lls_ATP->getLevel();
#endif
}

void digitalpause()
{
    if (lls->getVecLevel()->size() < ILEVEL_SENSOR::MAX_SIZE)
        return;

    if (!flag_conect_ok)
        return;

    uint32_t res = 0;
    for (auto vol : *lls->getVecLevel())
        res += vol;

    if (abs(static_cast<uint16_t>(res / lls->getVecLevel()->size()) - lls->getLevel()) < 3) // условие проверки уровня топлива по ДУТ (топливо перестало перетекать из других секций)
    {
        proceedTarring();
    }
}

void startPump()
{
    pump->on();
    if (datemod.mode != TAR && datemod.mode != PAUSE)
    {
        countV->reset();
    }
    else if (tar->getTimePause() != 0 && autostop)
        proceedTarring();
}

void stopPump()
{
    pump->off();
}

String makeLlsDateToDisplay(ILEVEL_SENSOR *_lls)
{
    String ch{};
    if (datemod.mode == TAR || datemod.mode == PAUSE)
        ch = "\\r";
    else
        ch = " ";
    if (_lls->getType() != ILEVEL_SENSOR::NO_LLS)
    {
        if (_lls->getType() == ILEVEL_SENSOR::RS485)
        {
            if (_lls->getError() == ILEVEL_SENSOR::error::NOT_FOUND)
                return "ДУТ не найден!";
            else if (_lls->getError() == ILEVEL_SENSOR::error::LOST)
                return "ДУТ (RS485)" + ch + "Adr: " + String(_lls->getNetadres()) + ch + "Потерян!";
            else
            {
                String str = "---";
                if (_lls->getLevel() <= ILEVEL_SENSOR::MAX_DIGITAL_N && _lls->getLevel() >= ILEVEL_SENSOR::MIN_DIGITAL_N)
                    str = String(_lls->getLevel());
                return "ДУТ (RS485)" + ch + "Adr: " + String(_lls->getNetadres()) + ch + "N= " + str;
            }
        }
        else if (_lls->getType() == ILEVEL_SENSOR::BLE_ESKORT)
        {
            if (_lls->getError() == ILEVEL_SENSOR::error::NOT_FOUND)
                return "ДУТ не найден!";
            else
                return _lls->getNameBLE() + ch + "RSSI: " + String(_lls->getRSSI()) + ch + "N=" + String(_lls->getLevel());
        }
#ifdef verAnalogInput
        else if (_lls->getType() == ILEVEL_SENSOR::ANALOGE_U)
        {
            if (_lls->getError() == ILEVEL_SENSOR::error::NOT_FOUND)
                return "ДУТ не найден!";
            else
                return "ДУТ (U)" + ch + "U= " + String(_lls->getLevel() / 100.0, 2) + " V";
        }
        else if (_lls->getType() == ILEVEL_SENSOR::ANALOGE_F)
        {
            if (_lls->getError() == ILEVEL_SENSOR::error::NOT_FOUND)
                return "ДУТ не найден!";
            return "ДУТ (F)" + ch + "F= " + String(_lls->getLevel()) + " Hz";
        }
#endif
    }
    return {};
}

String saveLog()
{
    // if (!SPIFFS.begin(true))
    // {
    //   return "not mounting SPIFFS";
    // }
    String patch = "/" + String(LOG_FILE_NAME);

    if (!SPIFFS.exists(patch))
    {
        File file = SPIFFS.open(patch, FILE_WRITE);
        file.printf("Log tar\n ");
    }

    File file = SPIFFS.open(patch, FILE_APPEND);
    if (!file)
        return "failed to open log file";

    file.printf("\n\nID: %s\n", tar->getId());
    file.printf("Vtank: %d L | Ref: %d | k: %d imp/L\n", tar->getVTank() / 10, tar->getNumRefill(), countV->getKinLitr());

    if (lls == nullptr)
        file.printf("No LLS | Time pause: %d min.\n", tar->getTimePause());
    else
    {
        String type = "";
        String adr = "";
        switch (lls->getType())
        {
        case ILEVEL_SENSOR::ANALOGE_U:
            type = "Analoge U";
            break;
        case ILEVEL_SENSOR::ANALOGE_F:
            type = "Analoge F";
            break;
        case ILEVEL_SENSOR::RS485:
            type = "RS 485";
            adr = String(lls->getNetadres());
            break;
        case ILEVEL_SENSOR::BLE_ESKORT:
            type = "BLE ESCORT";
            adr = lls->getNameBLE();
            break;

        default:
            break;
        }
        file.printf("Type LLS: %s | Adr LLS: %s | Time pause: %d min.\n", type, adr, tar->getTimePause());
    }
    RtcDateTime dt = tar->getTStart();
    const uint SIZE = 20;
    char datestring[SIZE];

    snprintf_P(datestring,
               SIZE,
               PSTR("%02u.%02u.%04u %02u:%02u"),
               dt.Day(),
               dt.Month(),
               dt.Year(),
               dt.Hour(),
               dt.Minute());
    file.printf("Start: %s\n", datestring);

    dt = Rtc.GetDateTime();
    snprintf_P(datestring,
               SIZE,
               PSTR("%02u.%02u.%04u %02u:%02u"),
               dt.Day(),
               dt.Month(),
               dt.Year(),
               dt.Hour(),
               dt.Minute());
    file.printf("End: %s\n", datestring);
    file.printf("N,LLS,V\n");

    for (int i = 0; i < tar->getCountReffil(); i++)
        file.println(tar->getResultRefill(i));

    file.close();

    if ((SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1024 < 300)
        return "no free space SPIFFS";

    return "";
}

String deleteLog()
{
    String patch = "/" + String(LOG_FILE_NAME);

    if (SPIFFS.exists(patch))
    {
        SPIFFS.remove(patch);
        if (!SPIFFS.exists(patch))
            return "<p><b>File delete successfully</b></p>";
        else
            return "<p><b>Deletion error. Try again...</b></p>";
    }
    return "<p><b>Deletion error. No files...</b></p>";
}

void wifiInit()
{
    EEPROM.begin(100);
    EEPROM.get(0, lp);

    // пытаемся подключиться
    Serial.print("Connect to: ");
    Serial.println(lp.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(lp.ssid, lp.pass);
    while (WiFi.status() != WL_CONNECTED && millis() < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.print("Connected! Local IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        loginPortal();
    }
}

void loginPortal()
{
    Serial.println("\nStart html Connect Wi-Fi");
    String res{};
    {
        WiFi.disconnect();
        int counterWiFi = WiFi.scanNetworks();
        for (auto i = 0; i < counterWiFi; i++)
        {
            res += WiFi.SSID(i).c_str();
            res += ',';
        }
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    // запускаем портал
    GyverPortal ui;
    ui.attachBuild(buildLoginPage);
    ui.start();
    ui.attach(action);
    // работа портала
    while (ui.tick())
    {
        if (millis() > 60000)
            return;
    }
    Serial.println();
    Serial.println("Close connect Wi-Fi");
}
void buildLoginPage()
{
    GP.BUILD_BEGIN();
    GP.THEME(GP_DARK);
    GP.FORM_BEGIN("/login");
    GP.TEXT("lg", "Login", lp.ssid);
    GP.BREAK();
    GP.PASS("ps", "Password", lp.pass);
    GP.SUBMIT("Submit");
    GP.FORM_END();
    GP.BUILD_END();
}

void buildLoginPage(String wifi)
{
    int wifinumder = 0;
    GP.BUILD_BEGIN();
    GP.THEME(GP_DARK);
    GP.FORM_BEGIN("/login");
    GP.SELECT("lg", wifi, wifinumder);
    for (auto i = 0; i < 20; i++)
    {
        lp.ssid[i] = 0;
    }
    for (auto i = 0; i < WiFi.SSID(wifinumder).length(); i++)
    {
        lp.ssid[i] = WiFi.SSID(wifinumder)[i];
    }
    GP.BREAK();
    GP.PASS("ps", "Password", lp.pass);
    GP.SUBMIT("Submit");
    GP.FORM_END();
    GP.BUILD_END();
}

void action(GyverPortal &p)
{
    if (p.form("/login")) // кнопка нажата
    {
        p.copyStr("lg", lp.ssid); // копируем себе
        p.copyStr("ps", lp.pass);
        EEPROM.put(0, lp);       // сохраняем
        EEPROM.commit();         // записываем
        WiFi.softAPdisconnect(); // отключаем AP
    }
}

void buildPage()
{
    GP.BUILD_BEGIN(1024);
    GP.THEME(GP_DARK);

    // позволяет "отключить" таблицу при ширине экрана меньше 600px
    GP.GRID_RESPONSIVE(600);

    GP.UPDATE("V_full,kRefillNum,error,flowrate,llsLevel,typells,id,adress,vtank,vtank2,kRefill,pause,timetaring");
    M_GRID(
        M_BLOCK_TAB(
            "Настройка",
            M_BOX(GP.LABEL("Тип ДУТ"); GP.SELECT("typells", "Без ДУТ, RS485, BLE, Analoge U, Analoge F"););
            M_BOX(GP.BUTTON_MINI("search_lls", "Поиск ДУТ"););
            M_BOX(GP.LABEL("Выбор ДУТ"); GP.SELECT("adresslls", ""););
            M_BOX(GP.LABEL("ID объекта"); GP.TEXT("id", ""););
            M_BOX(GP.LABEL("Объём бака, л"); GP.NUMBER("vtank", String(tar->getVTank() / 10)););
            M_BOX(GP.LABEL("Кол-во проливов"); GP.SLIDER("kRefill", tar->getNumRefill(), 5, 20););
            M_BOX(GP.LABEL("Пауза, мин"); GP.SLIDER("pause", tar->getTimePause(), 0, 10););
            M_BOX(GP.LABEL("Длительность, час:мин"); GP.LABEL(String(datemod.timetarring), "timetaring");););
        M_BLOCK_TAB(
            "Тарировка",
            M_BOX(GP.LABEL("Залито топлива"); GP.LABEL(String(datemod.v_full), "V_full"););
            M_BOX(GP.LABEL("Пролив"); GP.LABEL(String(datemod.kRefillNum), "kRefillNum"););
            M_BOX(GP.BUTTON("start_tar", "Старт"); GP.BUTTON("stop_tar", "Стоп"););
            M_BOX(GP.LABEL("Выполнено"); GP.SLIDER("tar", 0, 0, 100, 1, 0, GP_RED_B, 1, 0););
            M_BOX(GP.BUTTON_MINI("end_tarring", "Закончить тарировку"););););
    M_GRID(
        M_BLOCK_TAB(
            "Счетчик",
            M_BOX(GP.LABEL("Залито топлива"); GP.LABEL(String(datemod.v_full), "v_full"););
            M_BOX(GP.BUTTON_MINI("start1", "Старт"); GP.BUTTON_MINI("stop1", "Стоп"););
            M_BOX(GP.BUTTON_MINI("menu", "Возврат в меню");););
        M_BLOCK_TAB(
            "Выдача топлива",
            M_BOX(GP.LABEL("Объем топлива"); GP.NUMBER("vtank2", "0"););
            M_BOX(GP.LABEL("Выдано топлива"); GP.LABEL(String(datemod.v_full), "v_full"););
            M_BOX(GP.BUTTON_MINI("start2", "Старт"); GP.BUTTON_MINI("stop2", "Стоп"););
            M_BOX(GP.BUTTON_MINI("menu", "Возврат в меню");););
        M_BLOCK_TAB(
            "Автоматическое выкачивание",
            M_BOX(GP.LABEL("Выкачено топлива топлива"); GP.LABEL(String(datemod.v_full), "v_full"););
            M_BOX(GP.BUTTON_MINI("start3", "Старт"); GP.BUTTON_MINI("stop3", "Стоп"););
            M_BOX(GP.BUTTON_MINI("menu", "Возврат в меню"););););
    M_BLOCK_TAB(
        "Дополнительно",
        M_BOX(GP.LABEL("Ошибки"); GP.LABEL(errorStringWeb, "error"););
        M_BOX(GP.LABEL("Скорость"); GP.LABEL(String(datemod.flowRate), "flowrate"););
        M_BOX(GP.LABEL("Уровень ДУТ"); GP.LABEL(String(lls->getLevel()), "llsLevel"););
        M_BOX(GP.BUTTON_MINI_DOWNLOAD("/log.csv", "Скачать log");););
    GP.BUILD_END();
}

void actionDownload()
{
    if (ui.download())
    {
        ui.sendFile(LittleFS.open(ui.uri(), "r"));
    }
}

void actionPage()
{
    if (ui.update())
    {
        ui.updateString("error", errorStringWeb);
        ui.updateInt("flowrate", datemod.flowRate);
        ui.updateInt("llsLevel", lls->getLevel());
        ui.updateInt("v_full", datemod.v_full);
        if (datemod.mode == SETTING || datemod.mode == TAR)
        {
            ui.updateInt("kRefill", tar->getNumRefill());
            ui.updateInt("tar", datemod.kRefillNum);
            ui.updateInt("vtank", tar->getVTank() / 10);
            ui.updateInt("typells", lls->getType());
        }
        String str = tar->getId();
        ui.updateString("id", str);
        if (datemod.mode == PUMPINGOUT)
            if (pump->get())
                ui.updateInt("vtank2", tank->getVTank() / 10);
        str = hmi.convertStringTime_(tar->getTimeTarring());
        ui.updateString("timetaring", str);

        if (datemod.mode == SETTING)
            ui.updateInt("pause", tar->getTimePause());
    }
    if (ui.click())
    {
        if (ui.click())
        {
            if (datemod.mode == MENU)
            {
                if (ui.clickInt("vtank2", web_data.w_vtank_pumpingout))
                {
                    hmi("page 10");
                }
            }
            if (datemod.mode == PUMPINGOUT)
                ui.clickInt("vtank2", web_data.w_vtank_pumpingout);

            int res = 0;
            if (ui.clickBool("menu", res))
                if (datemod.mode != TAR)
                    hmi("page 1");
        }

        // режим счетчик
        bool res = false;
        if (ui.clickBool("start1", res))
        {
            if (datemod.mode == MENU)
            {
                hmi("page 3");
            }

            if (datemod.mode == COUNT)
                pump->on();
        }
        if (ui.clickBool("stop1", res))
        {
            if (datemod.mode == COUNT)
                pump->off();
        }

        // режим выдачи топлива
        if (ui.clickBool("start2", res))
        {
            if (datemod.mode == PUMPINGOUT)
            {
                tank->setVTank(web_data.w_vtank_pumpingout * 10);
                Serial.println("Start: " + String(tank->getVTank()));
                pump->on();
            }
        }
        if (ui.clickBool("stop2", res))
        {
            if (datemod.mode == PUMPINGOUT)
            {
                pump->off();
            }
        }

        // режим автоматического выкачивания
        if (ui.clickBool("start3", res))
        {
            if (datemod.mode == PUMPINGAUTO)
            {
                pump->on();
            }
            else
            {
                if (datemod.mode == MENU)
                    hmi("page 9");
            }
        }
        if (ui.clickBool("stop3", res))
        {
            if (datemod.mode == PUMPINGAUTO)
            {
                pump->off();
            }
        }

        // режим настройки тарировки
        String id{};
        if (ui.clickString("id", id))
        {
            if (datemod.mode == MENU)
            {
                hmi("page 13");
                tar->setId(id);
                hmi("page 6");
                datemod.mode = SET_WEB;
            }
        }

        int data{};
        if (ui.clickInt("vtank", data))
            if (datemod.mode == SET_WEB)
                if (data)
                    tar->setVTank(data * 10);
                else
                    tar->setVTank(150 * 10);

        if (ui.clickInt("kRefill", data))
            if (datemod.mode == SET_WEB)
                if (data)
                    tar->setNumRefill(data);
                else
                    tar->setNumRefill(12);

        if (ui.clickInt("pause", data))
        {
            if (datemod.mode == SET_WEB)
            {
                tar->setTimePause(data);
                hmi("tar.n3.val", tar->getVTank() / 10);
                hmi("tar.x2.val", tar->getVTankRefill());
                hmi("tar.n0.val", tar->getNumRefill());
                hmi("tar.select0.val", tar->getTimePause());

                hmi("tar.select0.val", data);
                hmi("page 8");
            }
            if (datemod.mode == TAR)
                if (data)
                    tar->setTimePause(data);
                else if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
                    tar->setTimePause(data);
            hmi("tar.select0.val", tar->getTimePause());
        }

        if (datemod.mode == TAR)
        {
            if (ui.clickBool("start_tar", res))
                pump->on();

            if (ui.clickBool("stop_tar", res))
                pump->off();

            if (ui.clickBool("end_tarring", res))
                if (!pump->get())
                    if (!tar->getVfuel())
                        hmi("page 1");
                    else
                        endTarring();
        }
    }
}

String listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    String response{};
    // Serial.print"Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        // Serial.println("- failed to open directory");
        return response;
    }
    if (!root.isDirectory())
    {
        // Serial.println(" - not a directory");
        return response;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            // Serial.print("  DIR : ");
            // Serial.println(file.name());
            if (levels)
                listDir(fs, file.name(), levels - 1);
        }
        else
        {
            // Serial.print("  FILE: ");
            // Serial.print(file.name());
            // Serial.print("\t\tSIZE: ");
            // Serial.println(file.size());
            response += "<p><a href='" + String(file.name()) + "'>" + file.name() + "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + file.size() + " byte" + "</a>" + "</p>";
        }
        file = root.openNextFile();
    }
    return response;
}

// void getDataLog(AsyncWebServerRequest *request, String file)
// {
//     AsyncWebServerResponse *response = request->beginResponse(SPIFFS, file, String(), true);
//     response->addHeader("Content-Type", "application/octet-stream");
//     response->addHeader("Content-Description", "File Transfer");
//     // response->addHeader("Content-Disposition", "attachment; filename='data.csv'");
//     response->addHeader("Pragma", "public");
//     response->addHeader("Cache-Control", "no-cache");
//     request->send(response);
// }

void delete_lls()
{
    // Serial.println("Delete LLS");
    if (lls->getType() == ILEVEL_SENSOR::BLE_ESKORT)
        lls->newBLE("");
    lls = lls_Empty;
}

void IRAM_ATTR onTimer()
{
    countIndicatorTimerActive = false;
    digitalWrite(INIDICATE_COUNT, OFF);
}
