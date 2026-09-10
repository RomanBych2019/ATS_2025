#include "ModbusTable.h"

#include <SimpleModbusSlave_DUE.h>

#include "main.h"

DateMod datemod;

namespace
{
const long FUEL_TOTAL_UPDATE_DELAY_MS = 12000;
const long MODBUS_BAUD_RATE = 19200;
const byte MODBUS_SLAVE_ID = 1;
const byte MODBUS_TX_ENABLE_PIN = 0;

void clearLlsRegisters()
{
    datemod.typells = ILEVEL_SENSOR::type::NO_LLS;
    datemod.adress = 0;
    datemod.k_adress = 0;
    datemod.resultN = 0;
    datemod.resultNProliv = 0;
    datemod.rssi = 0;
}

void updateLlsRegisters()
{
    if (lls->getType() == ILEVEL_SENSOR::NO_LLS)
    {
        clearLlsRegisters();
        return;
    }

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
}

void configureModbusTable()
{
    modbus_configure(&serialMB, MODBUS_BAUD_RATE, MODBUS_SLAVE_ID, MODBUS_TX_ENABLE_PIN, MODBUS_REGISTER_COUNT, datemod.au16data);
    // modbus_update_comms(MODBUS_BAUD_RATE, MODBUS_SLAVE_ID);
}

/*  ---------- Обновление данных таблицы modbus ---------- */
void updateModbusRegisters()
{
    modbus_update();
    if (tar->getNumRefill() >= tar->getVRefill()->size())
        datemod.kRefillNum = tar->getVRefill()->size();
    if (millis() > time_start_refill + FUEL_TOTAL_UPDATE_DELAY_MS) // задержка в обновлении объема топлива для корректного составления отчета в Виалоне
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
    updateLlsRegisters();

#ifdef verATP
    datemod.llsATP = lls_ATP->getLevel();
#endif
}
