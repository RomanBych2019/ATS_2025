#include "ModbusTable.h"

#include <SimpleModbusSlave_DUE.h>

#include "main.h"

DateMod datemod;

void configureModbusTable()
{
    modbus_configure(&serialMB, 19200, 1, 0, SIZE, datemod.au16data);
    // modbus_update_comms(19200, 1);
}

/*  ---------- Обновление данных таблицы modbus ---------- */
void updateModbusRegisters()
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
