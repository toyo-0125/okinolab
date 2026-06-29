#include "temprature.h"
#include <stdint.h>

extern uint8_t temprature_sens_read(void);

float get_cpu_temprature(void)
{
    return (float)(temprature_sens_read() - 32) / 1.8;
}