#pragma once

#include "NimBLEDevice.h"
#include "LEVEL_SENSOR.h"

extern String nameBLE_ls;
extern bool bleSensorFound;
extern int16_t bleSensorRSSI;
extern std::string bleManufacturerData;

const uint16_t SCANTIME = 9; // 9 seconds
extern uint16_t scanTime_; // время непрерывного сканирования


class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice *advertisedDevice)
    {
        // Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
        if (advertisedDevice->getName() == nameBLE_ls.c_str())
        {
            bleSensorFound = true;
            bleSensorRSSI = advertisedDevice->getRSSI();
            bleManufacturerData = advertisedDevice->getManufacturerData();
            Serial.printf("Found our LLS: %s\n", advertisedDevice->toString().c_str());
            NimBLEDevice::getScan()->stop();
        }
    };
};

class LS_BLE : public ILEVEL_SENSOR
{
public:
private:
    uint16_t dataBLE_[4] = {};
    int16_t RSSI_ = {};
    BLEScan *BLEScan_;
    boolean _doConnect = false;
    bool _echo = false;

    bool buildData(const uint8_t *source, size_t length)
    {
        if (source == nullptr || length < 14)
        {
            clearData();
            return false;
        }

        dataBLE_[0] = source[3];
        dataBLE_[0] |= source[4] << 8;
        dataBLE_[1] = source[5];
        dataBLE_[2] = source[6];
        dataBLE_[3] = source[13] ? 4095 : 1024;
        return true;
    }

    void clearData()
    {
        for (auto &data : dataBLE_)
            data = 0;
    }

    // ошибки
    void set_error_()
    {
        if (!bleSensorFound)
        {
            counter_errror_++;
            if (counter_errror_ > COUNT_ERROR) // ДУТ не найден
            {
                error_ = error::LOST;
                return;
            }
        }
        else if (level_ < MIN_DIGITAL_B)
        {
            counter_errror_++;
            if (counter_errror_ > COUNT_ERROR)
                error_ = error::CLIFF;
        }
        else if (level_ > MAX_DIGITAL_B)
        {
            counter_errror_++;
            if (counter_errror_ > COUNT_ERROR)
                error_ = error::CLOSURE; // показания датчика выше нормы (неисправность датчика)
        }
        else
        {
            clearError();
        }
    }

public:
    LS_BLE()
    {
        // Serial.print("\n  - Create BLE\n");
        type_ = ILEVEL_SENSOR::BLE_ESKORT;
        level_start_ = MAX_ANALOGE_BLE_START;
        clearError();
        nameBLE_ls = "";

        //  --------------------BLE-------------------
        BLEDevice::init("");             // Инициализируем BLE-устройство:
        BLEScan_ = BLEDevice::getScan(); // create new scan
        BLEScan_->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
        BLEScan_->setActiveScan(true); // active scan uses more power, but get results faster
        BLEScan_->setInterval(97);
        BLEScan_->setWindow(37);    // less or equal setInterval value
        BLEScan_->setMaxResults(0); // do not store the scan results, use callback only.
    }

    void newBLE(const String &name = "") override
    {
        if (BLEScan_->isScanning())
        {
                NimBLEDevice::getScan()->stop();
                BLEScan_->clearResults(); // delete results fromBLEScan buffer to release memory
                // return false;
                delay(10);
        }
        nameBLE_ls = name;
        bleSensorFound = false;
        bleSensorRSSI = 0;
        bleManufacturerData.clear();
        RSSI_ = 0;
        level_ = 0;
        _doConnect = false;
        clearData();
        resetVecLevel();
        clearError();
    }

    const String getNameBLE() const override
    {
        return nameBLE_ls;
    }

    const int getNameBLE_int() const override
    {
        String result{};
        for (auto s : nameBLE_ls)
        {
            if (s >= 0x30 && s <= 0x39)
                result += s;
        }
        return result.toInt();
    }

    const int16_t getRSSI() const override
    {
        return RSSI_;
    }

    const uint16_t getDataBLE(uint i) const override
    {
        if (i >= 4)
            return 0;
        return dataBLE_[i];
    }

    const bool search() override
    {
        clearError();
        scanTime_ = 61;
        if (BLEScan_->isScanning())
        {
            if (_echo)
            {
                Serial.print(millis() / 1000);
                Serial.print(" | ");
                Serial.print("Stop scan\n");
            }
            NimBLEDevice::getScan()->stop();
        }
        else
            update();

        if (!bleSensorFound)
        {
            error_ = error::NOT_FOUND;
            scanTime_ = SCANTIME;
            return false;
        }

        scanTime_ = SCANTIME;
        return true;
    }

    bool update() override
    {
        if (nameBLE_ls == "")
            return false;

        if (BLEScan_->isScanning())
            return true;

        bleSensorFound = false;
        bleSensorRSSI = 0;
        bleManufacturerData.clear();
        if (_echo)
        {
            Serial.print(millis() / 1000);
            Serial.print(" | ");
            Serial.print("Start scan BLE " + nameBLE_ls + "\n");
        }

        BLEScan_->start(scanTime_, false);

        if (bleSensorFound)
        {
            RSSI_ = bleSensorRSSI;
            if (buildData(reinterpret_cast<const uint8_t *>(bleManufacturerData.data()), bleManufacturerData.length()))
            {
                clearError();
                level_ = getDataBLE(0);
                setVLevel();
                _doConnect = true;
            }
            else
            {
                level_ = 0;
                _doConnect = false;
            }
        }
        else
        {
            _doConnect = false;
        }
        set_error_();
        BLEScan_->clearResults(); // delete results fromBLEScan buffer to release memory
        bleManufacturerData.clear();
        if (_echo)
        {
            Serial.print(millis() / 1000);
            Serial.print(" | ");
            Serial.print("End scan BLE\n\n");
        }
        return _doConnect;
    }

    void echoEnabled(bool echoEnabled)
    {
        _echo = echoEnabled;
    }
    
    bool getDoConnect() const override
    {
        return _doConnect;
    }

    ~LS_BLE(){
        // Serial.print("\n  - Kill ble\n");
        //     BLEScan_->stop();
        //     BLEScan_->clearResults(); // delete results fromBLEScan buffer to release memory
    };
};
