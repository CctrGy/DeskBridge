#include "core/McuMonitor.h"

#include <Arduino.h>
#include <math.h>

#include "config/DeskConfig.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <esp_system.h>

namespace
{
    constexpr uint32_t monitorIntervalMs = MCU_MONITOR_INTERVAL_MS_DEFAULT;

    bool initialized = false;
    float chipTemperatureC = NAN;
    float chipVddaVolts = NAN;
    uint32_t lastReadMs = 0;
    const char *resetReasonText = "unknown";
    char uniqueIdText[25] = {};

    void captureUniqueId()
    {
        const uint64_t mac = ESP.getEfuseMac();
        snprintf(uniqueIdText, sizeof(uniqueIdText), "%016llX", static_cast<unsigned long long>(mac));
    }

    void captureResetReason()
    {
        switch (esp_reset_reason())
        {
            case ESP_RST_POWERON: resetReasonText = "power"; break;
            case ESP_RST_SW: resetReasonText = "software"; break;
            case ESP_RST_PANIC: resetReasonText = "panic"; break;
            case ESP_RST_INT_WDT:
            case ESP_RST_TASK_WDT:
            case ESP_RST_WDT: resetReasonText = "watchdog"; break;
            case ESP_RST_DEEPSLEEP: resetReasonText = "deep sleep"; break;
            case ESP_RST_BROWNOUT: resetReasonText = "brownout"; break;
            default: resetReasonText = "unknown"; break;
        }
    }
}

namespace McuMonitor
{
    void begin()
    {
        captureUniqueId();
        captureResetReason();
        initialized = true;
        update();
    }

    void update()
    {
        if (!initialized)
        {
            return;
        }

        const uint32_t now = millis();
        if (lastReadMs != 0 && now - lastReadMs < monitorIntervalMs)
        {
            return;
        }

        lastReadMs = now;
        chipTemperatureC = temperatureRead();
        chipVddaVolts = NAN;
    }

    bool ready()
    {
        return true;
    }

    float temperatureC()
    {
        return chipTemperatureC;
    }

    float vddaVolts()
    {
        return chipVddaVolts;
    }

    uint32_t coreClockHz()
    {
        return static_cast<uint32_t>(ESP.getCpuFreqMHz()) * 1000000UL;
    }

    uint32_t uptimeSeconds()
    {
        return millis() / 1000;
    }

    const char *chipName()
    {
        return "ESP32-S3";
    }

    const char *uniqueIdHex()
    {
        return uniqueIdText;
    }

    const char *resetReason()
    {
        return resetReasonText;
    }
}

#else

#include "stm32f4xx_hal.h"

namespace
{
    constexpr uint32_t monitorIntervalMs = MCU_MONITOR_INTERVAL_MS_DEFAULT;
    constexpr float adcMax = 4095.0f;
    constexpr float vrefintNominal = 1.21f;
    constexpr float tempV25 = 0.76f;
    constexpr float tempAverageSlope = 0.0025f;

    ADC_HandleTypeDef adcHandle = {};
    bool adcReady = false;
    bool initialized = false;
    float chipTemperatureC = NAN;
    float chipVddaVolts = NAN;
    uint32_t lastReadMs = 0;
    const char *resetReasonText = "unknown";
    char uniqueIdText[25] = {};

    uint16_t readAdcChannel(uint32_t channel)
    {
        ADC_ChannelConfTypeDef config = {};
        config.Channel = channel;
        config.Rank = 1;
        config.SamplingTime = ADC_SAMPLETIME_480CYCLES;

        if (HAL_ADC_ConfigChannel(&adcHandle, &config) != HAL_OK)
        {
            return 0;
        }

        if (HAL_ADC_Start(&adcHandle) != HAL_OK)
        {
            return 0;
        }

        if (HAL_ADC_PollForConversion(&adcHandle, 10) != HAL_OK)
        {
            HAL_ADC_Stop(&adcHandle);
            return 0;
        }

        const uint16_t value = static_cast<uint16_t>(HAL_ADC_GetValue(&adcHandle));
        HAL_ADC_Stop(&adcHandle);
        return value;
    }

    void captureResetReason()
    {
        if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
        {
            resetReasonText = "watchdog";
        }
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
        {
            resetReasonText = "window wdg";
        }
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
        {
            resetReasonText = "software";
        }
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
        {
            resetReasonText = "power";
        }
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
        {
            resetReasonText = "reset pin";
        }
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))
        {
            resetReasonText = "low power";
        }

        __HAL_RCC_CLEAR_RESET_FLAGS();
    }

    void initAdc()
    {
        __HAL_RCC_ADC1_CLK_ENABLE();

        adcHandle.Instance = ADC1;
        adcHandle.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
        adcHandle.Init.Resolution = ADC_RESOLUTION_12B;
        adcHandle.Init.ScanConvMode = DISABLE;
        adcHandle.Init.ContinuousConvMode = DISABLE;
        adcHandle.Init.DiscontinuousConvMode = DISABLE;
        adcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
        adcHandle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
        adcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
        adcHandle.Init.NbrOfConversion = 1;
        adcHandle.Init.DMAContinuousRequests = DISABLE;
        adcHandle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

        adcReady = HAL_ADC_Init(&adcHandle) == HAL_OK;
        if (adcReady)
        {
            ADC->CCR |= ADC_CCR_TSVREFE;
        }
    }

    void readInternalSensors()
    {
        if (!adcReady)
        {
            return;
        }

        const uint16_t vrefRaw = readAdcChannel(ADC_CHANNEL_VREFINT);
        const uint16_t tempRaw = readAdcChannel(ADC_CHANNEL_TEMPSENSOR);

        if (vrefRaw == 0 || tempRaw == 0)
        {
            chipVddaVolts = NAN;
            chipTemperatureC = NAN;
            return;
        }

        chipVddaVolts = (vrefintNominal * adcMax) / static_cast<float>(vrefRaw);
        const float tempVoltage = (static_cast<float>(tempRaw) * chipVddaVolts) / adcMax;
        chipTemperatureC = ((tempVoltage - tempV25) / tempAverageSlope) + 25.0f;
    }

    void captureUniqueId()
    {
        snprintf(uniqueIdText, sizeof(uniqueIdText), "%08lX%08lX%08lX",
                 static_cast<unsigned long>(HAL_GetUIDw0()),
                 static_cast<unsigned long>(HAL_GetUIDw1()),
                 static_cast<unsigned long>(HAL_GetUIDw2()));
    }
}

namespace McuMonitor
{
    void begin()
    {
        captureUniqueId();
        captureResetReason();
        initAdc();
        initialized = true;
        update();
    }

    void update()
    {
        if (!initialized)
        {
            return;
        }

        const uint32_t now = millis();
        if (lastReadMs != 0 && now - lastReadMs < monitorIntervalMs)
        {
            return;
        }

        lastReadMs = now;
        readInternalSensors();
    }

    bool ready()
    {
        return adcReady;
    }

    float temperatureC()
    {
        return chipTemperatureC;
    }

    float vddaVolts()
    {
        return chipVddaVolts;
    }

    uint32_t coreClockHz()
    {
        return SystemCoreClock;
    }

    uint32_t uptimeSeconds()
    {
        return millis() / 1000;
    }

    const char *chipName()
    {
#if defined(STM32F411xE)
        return "STM32F411CE";
#elif defined(STM32F401xC)
        return "STM32F401CC";
#else
        return "STM32F4";
#endif
    }

    const char *uniqueIdHex()
    {
        return uniqueIdText;
    }

    const char *resetReason()
    {
        return resetReasonText;
    }
}

#endif
