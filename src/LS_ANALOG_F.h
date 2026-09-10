#pragma once

#include <Arduino.h>

#include "LEVEL_SENSOR.h"

void IRAM_ATTR rpm();
void update_Frequence(void *pvParameters);
float getAnalogFrequency();

class LS_ANALOG_F : public ILEVEL_SENSOR
{
private:
    const uint inputPin_;
    uint32_t median_ = 0, newest_ = 0, recent_ = 0, oldest_ = 0, val_ = 0;

    TaskHandle_t update_Frequence_ = NULL;

    // ошибки
    void set_error_()
    {
        if (level_ < MIN_ANALOGE_F)
        {
            counter_errror_++;
            if (counter_errror_ > COUNT_ERROR)
                error_ = error::NOT_FOUND; // обрыв датчика
        }
        else if (level_ > MAX_ANALOGE_F)
        {
            counter_errror_++;
            if (counter_errror_ > COUNT_ERROR)
                error_ = error::CLOSURE; // показания датчика выше нормы (неисправность датчика)
        }
        else
        {
            counter_errror_ = 0;
            error_ = error::NO_ERROR;
        }
    }

public:
    explicit LS_ANALOG_F(uint inputPin) : inputPin_(inputPin)
    {
        type_ = ILEVEL_SENSOR::ANALOGE_F;
        pinMode(inputPin_, INPUT);
        attachInterrupt(inputPin_, rpm, CHANGE);
        xTaskCreatePinnedToCore(
            update_Frequence,
            "update_Frequence",
            10000,
            NULL,
            10,
            &update_Frequence_,
            1);
        level_start_ = MAX_ANALOGE_F_START;
    }

    // обновление показаний
    bool update() override
    {
        val_ = getAnalogFrequency();
        oldest_ = recent_;
        recent_ = newest_;
        newest_ = val_;
        median_ = median_of_3(oldest_, recent_, newest_);
        level_ = median_;
        setVLevel();
        set_error_();
        return  level_ > MIN_ANALOGE_F? true: false; 
        // Serial.printf("\nAnalogeF: %d Hz", median_);
    }

    const bool search() override
    {
        // Serial.print("\nSearch AnalogeF\n");
        for (int i = 0; i < 8; i++)
        {
            update();
            delay(1100);
        }
        return !error_;
    }

    ~LS_ANALOG_F()
    {
        // Serial.print("\n  - Kill analogeF");
        if (update_Frequence_ != NULL)
            vTaskDelete(update_Frequence_);
        detachInterrupt(inputPin_);
    };
};
