#pragma once

#include <Arduino.h>
class COUNTER
{
    mutable portMUX_TYPE counterMux_ = portMUX_INITIALIZER_UNLOCKED;
    unsigned long Kcount_ = 0;
    uint16_t k_in_Litr_ = 1000;
    uint16_t k_in_Litr_calibr_ = 1000;
    uint32_t flow_rate_ = 0; // скорость потока топлива
    float v_fuel_save_ = 0;
    unsigned long speedPumptime_ = 0;

    static float calculateVolume_(const unsigned long count, const uint16_t pulsesPerLiter)
    {
        if (pulsesPerLiter == 0)
            return 0;
        return round(100.0 * count / pulsesPerLiter); // вычисление объема пролитого топлива
    }

public:
    COUNTER(const uint16_t k_in_Litr = 1000)
    {
        if (k_in_Litr > 0)
        {
            k_in_Litr_ = k_in_Litr;
            k_in_Litr_calibr_ = k_in_Litr;
        }
    }

    unsigned long setKcount()
    {
        portENTER_CRITICAL_ISR(&counterMux_);
        Kcount_ = Kcount_ + 1;
        const unsigned long count = Kcount_;
        portEXIT_CRITICAL_ISR(&counterMux_);
        return count;
    }

    void setKinLitr(const uint16_t k_in_Litr)
    {
        portENTER_CRITICAL(&counterMux_);
        if (k_in_Litr > 0)
            k_in_Litr_ = k_in_Litr;
        portEXIT_CRITICAL(&counterMux_);
    }

    void setKinLitrCalibr(const uint16_t k_in_Litr)
    {
        portENTER_CRITICAL(&counterMux_);
        if (k_in_Litr > 0)
            k_in_Litr_calibr_ = k_in_Litr;
        portEXIT_CRITICAL(&counterMux_);
    }

    uint16_t getKinLitr() const
    {
        portENTER_CRITICAL(&counterMux_);
        const uint16_t k_in_Litr = k_in_Litr_;
        portEXIT_CRITICAL(&counterMux_);
        return k_in_Litr;
    }

    long getK() const
    {
        portENTER_CRITICAL(&counterMux_);
        const unsigned long count = Kcount_;
        portEXIT_CRITICAL(&counterMux_);
        return count;
    }

    void updateFlowRate()
    {
        const unsigned long now = millis();
        const uint32_t period_ = now - speedPumptime_;
        const float v_fuel = getVFuel();

        if (period_ > 0)
        {
            // flow_rate_ = 0.2 * flow_rate_ + 0.8 * (6000 * (getVFuel() - v_fuel_save_) / period);
            const float delta = v_fuel - v_fuel_save_;
            portENTER_CRITICAL(&counterMux_);
            flow_rate_ = delta > 0 ? 6000.0 * delta / period_ : 0;
            portEXIT_CRITICAL(&counterMux_);
        }
        // else if (v_fuel_save_ != getVFuel())
        //     flow_rate_ = 0;
        portENTER_CRITICAL(&counterMux_);
        v_fuel_save_ = v_fuel;
        speedPumptime_ = now;
        portEXIT_CRITICAL(&counterMux_);
    }

    const uint32_t getFlowRate() const
    {
        portENTER_CRITICAL(&counterMux_);
        const uint32_t flow_rate = flow_rate_;
        portEXIT_CRITICAL(&counterMux_);
        return flow_rate;
    }

    // protected:
    const float getVFuel() const
    {
        portENTER_CRITICAL(&counterMux_);
        const unsigned long count = Kcount_;
        const uint16_t k_in_Litr = k_in_Litr_;
        portEXIT_CRITICAL(&counterMux_);
        return calculateVolume_(count, k_in_Litr);
    }

    const float getVFuelCalibr() const
    {
        portENTER_CRITICAL(&counterMux_);
        const unsigned long count = Kcount_;
        const uint16_t k_in_Litr_calibr = k_in_Litr_calibr_;
        portEXIT_CRITICAL(&counterMux_);
        return calculateVolume_(count, k_in_Litr_calibr); // вычисление объема пролитого топлива для калибровки
    }

    void reset()
    {
        portENTER_CRITICAL(&counterMux_);
        Kcount_ = 0;
        flow_rate_ = 0;
        v_fuel_save_ = 0;
        speedPumptime_ = millis();
        portEXIT_CRITICAL(&counterMux_);
    }
};
