#pragma once

#include <stdio.h>
#include <inttypes.h>
// Filter coefficients
#define FILTER_ORDER_NUM 4 // filter order

int16_t filter(int16_t input);
