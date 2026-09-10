#include "HmiRenderer.h"

#include "main.h"

namespace
{
const unsigned int ERROR_LLS_LEVEL_NOT_CHANGED = 32;
const unsigned int ERROR_LOW_FLOW_RATE = 64;
const unsigned int ERROR_HIGH_INITIAL_LLS_LEVEL = 128;

struct ErrorMessage
{
    unsigned int mask;
    const char *nextionText;
    const char *webText;
};

const ErrorMessage ERROR_MESSAGES[] = {
    {ILEVEL_SENSOR::error::CLIFF, "Данные с ДУТ ниже минимального значения\\rПроверте настройки ДУТ (min уровень))", "Данные с ДУТ ниже минимального значения<br>Проверте настройки ДУТ (min уровень))"},
    {ILEVEL_SENSOR::error::CLOSURE, "Данные с ДУТ выше максимального значения\\rПроверте настройки и подключение ДУТ", "Данные с ДУТ выше максимального значения<br>Проверте настройки и подключение ДУТ"},
    {ILEVEL_SENSOR::error::NOT_FOUND, "ДУТ не найден\\rПроверте настройки и подключение ДУТ", "ДУТ не найден<br>Проверте настройки и подключение ДУТ"},
    {ILEVEL_SENSOR::error::LOST, "ДУТ потерян\\rПроверте подключение ДУТ", "ДУТ потерян<br>Проверте подключение ДУТ"},
    {ERROR_LLS_LEVEL_NOT_CHANGED, "Нет изменения значений ДУТ\\rПроверте поступление топлива в бак", "Нет изменения значений ДУТ<br>Проверте поступление топлива в бак"},
    {ERROR_LOW_FLOW_RATE, "Низкая скорость потока топлива\\rПроверте прохождение топлива через счетчик", "Низкая скорость потока топлива<br>Проверте прохождение топлива через счетчик"},
    {ERROR_HIGH_INITIAL_LLS_LEVEL, "Высокие начальные показания ДУТ\\rПроверте калибровку ДУТ, убедитесь в отсутствии топлива в баке", "Высокие начальные показания ДУТ<br>Проверте калибровку ДУТ, убедитесь в отсутствии топлива в баке"},
};

bool buildErrorMessage(String &nextionText, String &webText)
{
    if (datemod.error == 0)
    {
        webText = "";
        return false;
    }

    for (const auto &message : ERROR_MESSAGES)
    {
        if (datemod.error & message.mask)
        {
            nextionText = message.nextionText;
            webText = message.webText;
            return true;
        }
    }

    return false;
}
}

/*  ---------- Отправка данных на дисплей Nextion  ---------- */
void sendNextion(void *pvParameters)
{
    for (;;)
    {
        String str = {};
        String res = {};
        uint level;

        RtcDateTime dt = Rtc.GetDateTime();
        const uint SIZE = 20;
        char datestring[SIZE];
        int bt = 0;
        int j0 = 0;

        if (flag_HMI_send)
            hmi("sendme");

        else
        {
            switch (datemod.mode)
            {
            case MENU:
                snprintf_P(datestring,
                           SIZE,
                           PSTR("%02u.%02u.%04u %02u:%02u"),
                           dt.Day(),
                           dt.Month(),
                           dt.Year(),
                           dt.Hour(),
                           dt.Minute());

                lls->getType() == ILEVEL_SENSOR::NO_LLS ? str = "" : str = makeLlsDateToDisplay(lls);

                bt = static_cast<int>(lls->getType());

#ifdef verATP
                if (lls_ATP->getError() == ILEVEL_SENSOR::NO_ERROR)
                {
                    if (lls_ATP->getTarLevel() != -1.0)
                    {
                        res = String(lls_ATP->getTarLevel(), 1);
                        res += " l";
                    }
                }
                if (lls_ATP->getError() == ILEVEL_SENSOR::CLOSURE)
                    res = "closure";
#endif

                hmi.sendScreenMenu(datestring, countV->getKinLitr(), res, str, bt);
                break;

            case PUMPINGOUT:
                hmi.sendScreenPump_Out(tar->getVfuel(), countV->getFlowRate());
                if (pump->get() == ON)
                    hmi("pump_out.t0.txt", String(datemod.vtank));
                hmi("pump_out.b1.picc", pump->get() == OFF ? 20 : 15);
                break;

            case PUMPINGAUTO:
                hmi.sendScreenPump_Auto(tar->getVfuel(), countV->getFlowRate(), str);
                hmi("pump_auto.b4.picc", pump->get() == OFF ? 14 : 15);
                break;

            case COUNT:
                lls->getType() == ILEVEL_SENSOR::NO_LLS ? str = "" : str = makeLlsDateToDisplay(lls);

                if (counter_display_resetring != tar->getCountReffil())
                {
                    for (int i = 0; i < tar->getCountReffil(); i++)
                        res += String(tar->getRefill(i) / 10.0, 1) + " l\\r";
                    hmi.sendScreenCounter(res);
                    counter_display_resetring++;
                }
                hmi.sendScreenCounter(tar->getVfuel() - tar->getBackRefill(), tar->getVfuel(), countV->getFlowRate(), str);
                hmi("counter.b4.picc", pump->get() == OFF ? 10 : 11);
                break;

            case CALIBR:
                hmi.sendScreenCalibration(countV->getVFuelCalibr(), countV->getK());
                hmi("calibr.b4.picc", pump->get() == OFF ? 2 : 3);
                break;

            case PAUSE:;

            case TAR:
                lls->getType() == ILEVEL_SENSOR::NO_LLS ? str = "ДУТ не подключен" : str = makeLlsDateToDisplay(lls);

                level = tar->getVTank() > 0 ? map(tar->getVfuel(), 0, tar->getVTank(), 0, 100) : 0;
                hmi("tar.b4.picc", pump->get() == OFF ? 23 : 24);
                uint tmp_time_pause;
                // if (autostop && tar->getType() == tarring::MANUAL)
                if (autostop && tar->getTimePause() != 0)
                {
                    tmp_time_pause = 60 * (tar->getTimePause()) - (millis() - start_pause) / 1000;
                    if (tmp_time_pause == 0)
                        tmp_time_pause = 1;
                }
                else
                    tmp_time_pause = tar->getTimePause() * 60;

                if (tmp_time_pause > tar->getTimePause() * 60)
                    tmp_time_pause = tar->getTimePause() * 60;

                hmi.sendScreenTarring(tar->getVfuel() - tar->getBackRefill(), tar->getVfuel(), tar->getCountReffil(), tar->getNumRefill() - tar->getCountReffil(), countV->getFlowRate(), str, tar->getTimeTarring(), level, tmp_time_pause, lls->getDoConnect());
                break;

            case MESSAGE:
                if (buildErrorMessage(str, errorStringWeb))
                    hmi.sendScreenMessage(str);
                break;

            case END_TAR_HMI:
                str = "ID: " + tar->getId() + "\\rN  | LLS   | V";
                for (int i = 0; i < tar->getNRefill()->size(); i++)
                {
                    uint n = tar->getNRefill()->at(i);
                    String res_n = "";
                    if (n < 10)
                        res_n = "       " + String(n);
                    else if (n < 100)
                        res_n = "    " + String(n);
                    else if (n < 1000)
                        res_n = "  " + String(n);
                    else
                        res_n = " " + String(n);
                    if (i < 10)
                        str += "\\r" + String(i) + "   |";
                    else
                        str += "\\r" + String(i) + " |";
                    str += res_n + "| " + String(tar->getVRefill()->at(i) / 10.0, 1);
                }
                if (start_pause > millis())
                    j0 = map(start_pause - millis(), TIME_PAUSE_END_TAR, 100, 100, 0);
                lls->getType() == ILEVEL_SENSOR::NO_LLS ? hmi.sendScreenEnd_Tar(str, j0) : hmi.sendScreenEnd_Tar(str, j0, tar->getVRefill(), tar->getNRefill());
                break;

            case SETTING:
                lls->getType() == ILEVEL_SENSOR::NO_LLS ? str = " не подключен" : str = makeLlsDateToDisplay(lls);

                hmi.sendScreenSetting(tar->getTimeTarring(), str);
                break;

            case SEARCH_BLE:
                if (lls->getType() != ILEVEL_SENSOR::NO_LLS)
                {
                    if (lls->getNameBLE() != "")
                    {
                        if (lls->getError() == ILEVEL_SENSOR::NOT_FOUND)
                            str = "ДУТ не найден";
                        else if (lls->getError() == ILEVEL_SENSOR::NO_ERROR)
                        {
                            str = "N " + String(lls->getLevel()) + " | RSSI " + String(lls->getRSSI()) + " | V " + String(lls->getDataBLE(1) / 10) + "." + String(lls->getDataBLE(1) % 10) + " | T " + String(lls->getDataBLE(2)) + " | D " + String(lls->getDataBLE(3));
                        }
                    }
                    else
                        str = "";
                    hmi.sendScreenSearch_BLE(str);
                }
                break;
            default:
                break;
            }
        }

        flag_HMI_send = !flag_HMI_send;
        vTaskDelay(pdMS_TO_TICKS(TIME_UPDATE_HMI));
    }
    vTaskDelete(NULL);
}
