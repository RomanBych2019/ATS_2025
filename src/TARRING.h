#pragma once

#include <Arduino.h>
#include <RtcDS3231.h>

#include "LEVEL_SENSOR.h"
#include "COUNTER.h"
#include "TANK.h"

namespace tarring
{
    enum mode
    {
        MANUAL,
        AUTO
    };
};

class TARRING
{
private:
    uint32_t refill_ = 0; // залито топлива в проливе
    static const int MAX_SIZE = 30;

    RtcDateTime t_start_; // время старта тарировки

    std::vector<uint32_t> v_ref_; // вектор объемов проливов
    std::vector<uint32_t> n_ref_; // вектор значений ДУТ проливов

    std::vector<String> v_total_; // вектор итогов тарировкиnum_reffil_

    uint32_t vtank_refill_ = 0; // объем пролива
    String id_ = "";             // id номер тарируемого объкта

    uint time_pause_ = 3; // пауза между проливами, мин

    uint num_reffil_ = 0;  // плановое кол-во проливов

    const int PUMPSPEED = 440; // скорость потока 10*литр/минута для вычисления времени тарировки
    COUNTER *countV_;
    TANK *tank_;

public:

    TARRING(COUNTER *count, TANK *tank) : countV_(count), tank_(tank)
    {
        v_ref_.reserve(MAX_SIZE);
        n_ref_.reserve(MAX_SIZE);
        v_total_.reserve(MAX_SIZE);
    }
    

    const std::vector<uint32_t> *getNRefill() const
    {
        return &n_ref_;
    }

    uint32_t getNRefill(uint i) const
    {
        if (i < n_ref_.size())
            return n_ref_.at(i);
        else
            return 0;
    }

    uint32_t getBackNRefill() const
    {
        if (n_ref_.size())
            return n_ref_.back();
        else
            return 0;
    }

    const std::vector<uint32_t> *getVRefill() const
    {
        return &v_ref_;
    }

    uint32_t getBackRefill() const
    {
        if (v_ref_.size())
            return v_ref_.back();
        else
            return 0;
    }

    uint32_t getRefill(uint i) const
    {
        if (i < v_ref_.size())
            return v_ref_.at(i);
        else
            return 0;
    }

    void setId(const String &id)
    {
        id_ = id;
    }
    const String getId() const
    {
        return id_;
    }

    const int getId_int() const
    {
        String result{};
        for (auto s : id_)
        {
            if (s >= 0x30 && s <= 0x39)
                result += s;
        }
        return result.toInt();
    }

//запись результатов пролива
    void saveResultRefuil(ILEVEL_SENSOR *lls)
    {
        if (lls == nullptr || v_ref_.size() >= MAX_SIZE)
            return;

        const uint32_t level = lls->getLevel();
        const uint32_t v_fuel = getVfuel();
        String str = String(getCountReffil()) + "," + String(level) + "," + String(v_fuel / 10.0, 1);
        v_total_.push_back(str);
        v_ref_.push_back(v_fuel);
        n_ref_.push_back(level);

        // Serial.printf("Тип ДУТ: %d\n", lls->getType()); 
        // Serial.printf("Размер вектора до: %d\n", lls->getVecLevel()->size());   

        lls->resetVecLevel();
        
        // Serial.printf("Размер вектора после: %d\n", lls->getVecLevel()->size());    
        // Serial.printf("Пролив: %s\n", str);
    }

    //удаление результатов пролива
    void deleteResultRefuil(ILEVEL_SENSOR *lls)
    {
        if (v_total_.size())
            v_total_.pop_back();
        if (v_ref_.size())
            v_ref_.pop_back();
        if (n_ref_.size())    
            n_ref_.pop_back();

        // Serial.printf("Удаление последнего результата\n");
        if (lls != nullptr)
            lls->resetVecLevel();
    }

// выдача результатов пролива i
    String getResultRefill(int i) const
    {
        if (i >= 0 && static_cast<size_t>(i) < v_total_.size())
            return v_total_.at(i);
        return {};
    }

    //  выдача объема топлива в баке
    const uint32_t getVfuel() const
    {
        return countV_->getVFuel();
    }

    // выдача объема бака
    const uint32_t getVTank() const
    {
        return tank_->getVTank();
    }

    // установка объема бака и вычисление объема проливов
    void setVTank(uint32_t v)
    {
        tank_->setVTank(v);
        if (num_reffil_)
            vtank_refill_ = v / num_reffil_;
        else
            vtank_refill_ = v;
    }

    // выдача объема одного пролива
    const uint32_t getVTankRefill() const
    {
        return vtank_refill_;
    }

    // выдача времени паузы между проливами
    const uint getTimePause() const
    {
        return time_pause_;
    }

    // установка вемени паузы между проливами
    void setTimePause(uint t)
    {
        time_pause_ = t;
    }

    // получить общее время тарировки
    const uint32_t getTimeTarring() const
    {
        const uint32_t v_tank = getVTank();
        const uint32_t v_fuel = getVfuel();
        const uint refill_count = getCountReffil();

        if (v_tank > v_fuel && num_reffil_ >= refill_count)
        {
            const uint remaining_refills = num_reffil_ - refill_count;
            const uint32_t remaining_fuel = v_tank - v_fuel;
            // if (mode_ == tarring::MANUAL)
            if (time_pause_ != 0)
                return time_pause_ * remaining_refills + remaining_fuel / PUMPSPEED;
            else
                return 2 * remaining_refills + remaining_fuel / PUMPSPEED;
        }

        return 0;
    }

    // сброс настроек тарировки
    void reset()
    {
        refill_ = 0;
        num_reffil_ = 12;
        tank_->reset();
        vtank_refill_ = tank_->getVTank() / num_reffil_;
        time_pause_ = 3;
        n_ref_.clear();
        v_ref_.clear();
        v_total_.clear();
        countV_->reset();
        id_ = "";
    }

    // установка кол-ва проливов
    void setNumRefill(uint count)
    {
        num_reffil_ = count;
        if (count)
            vtank_refill_ = getVTank() / count;
        else
            vtank_refill_ = getVTank();
    }

    // выдача кол-ва проливов
    const uint getNumRefill() const
    {
        return num_reffil_;
    }

    // выдача номера пролива
    uint getCountReffil() const
    {
        return v_ref_.size();
    }

    // сохранения времени начала тарировки
    void setTStart(RtcDateTime t)
    {
        t_start_ = t;
    }

    const RtcDateTime getTStart() const
    {
        return t_start_;
    }
};
