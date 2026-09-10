#include "LS_ANALOG_F.h"

static portMUX_TYPE frequencyMux = portMUX_INITIALIZER_UNLOCKED;
static float f_ = 0;
static unsigned long t_start_ = 0;
static volatile uint32_t count_ = 0;
static uint32_t count_old_ = 0;

void IRAM_ATTR rpm()
{
    portENTER_CRITICAL_ISR(&frequencyMux);
    count_ = count_ + 1;
    portEXIT_CRITICAL_ISR(&frequencyMux);
}

float getAnalogFrequency()
{
    portENTER_CRITICAL(&frequencyMux);
    const float frequency = f_;
    portEXIT_CRITICAL(&frequencyMux);
    return frequency;
}

void update_Frequence(void *pvParameters)
{
    for (;;)
    {
        const unsigned long now = micros();
        const unsigned long period_ = now - t_start_;

        portENTER_CRITICAL(&frequencyMux);
        const uint32_t count = count_;
        portEXIT_CRITICAL(&frequencyMux);

        const uint32_t d_ = count - count_old_;
        t_start_ = now;
        count_old_ = count;

        const float frequency = period_ > 0 ? d_ * 500000.0 / period_ : 0;
        portENTER_CRITICAL(&frequencyMux);
        f_ = frequency;
        portEXIT_CRITICAL(&frequencyMux);
        // Serial.printf("\nCoout %d | Period: %d | AnalogeF: %f Hz", d_, period_, f_);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
