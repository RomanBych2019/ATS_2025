#include "WebUi.h"

#include "main.h"

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
