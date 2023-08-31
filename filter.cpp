#include "filter.h"

float a[FILTER_ORDER_NUM+1] = {1.0, -3.89738598, 5.69739, -3.70246715, 0.90246539}; // replace with your values
float b[FILTER_ORDER_NUM+1] = {1.41272654e-07, 5.65090614e-07, 8.47635922e-07, 5.65090614e-07, 1.41272654e-07}; // replace with your values

// Buffer to hold past input and output values
float x[FILTER_ORDER_NUM+1] = {0};
float y[FILTER_ORDER_NUM+1] = {0};

// Function to filter input sample
int16_t filter(int16_t input) {
    float output;

    // Shift old values
    for (int i = FILTER_ORDER_NUM; i > 0; i--) {
        x[i] = x[i-1];
        y[i] = y[i-1];
    }

    // Store new input (convert from int16_t to float)
    x[0] = input / 32768.0f;

    // Compute new output
    output = 0;
    for (int i = 0; i <= FILTER_ORDER_NUM; i++) {
        output += b[i] * x[i];
    }
    for (int i = 1; i <= FILTER_ORDER_NUM; i++) {
        output -= a[i] * y[i];
    }

    // Store new output
    y[0] = output;

    // Convert output from float to int16_t
    return (int16_t)(output * 32768);
}
