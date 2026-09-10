#include "LS_ANALOG_F.h"

float f_ = 0;
unsigned long t_start_ = 0;
volatile uint32_t count_ = 0;
uint32_t count_old_ = 0;

void IRAM_ATTR rpm()
{
    count_++;
}

void update_Frequence(void *pvParameters)
{
    for (;;)
    {
        uint long period_ = micros() - t_start_;
        uint32_t d_ = count_ - count_old_;
        t_start_ = micros();
        count_old_ = count_;
        f_ = d_ * 500000.0 / period_;
        // Serial.printf("\nCoout %d | Period: %d | AnalogeF: %f Hz", d_, period_, f_);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
