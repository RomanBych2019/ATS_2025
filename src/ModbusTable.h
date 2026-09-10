#pragma once

#include <Arduino.h>

const uint8_t MODBUS_REGISTER_COUNT = 22;
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
  unsigned int au16data[MODBUS_REGISTER_COUNT];
};

extern DateMod datemod;

void configureModbusTable();
void updateModbusRegisters();
