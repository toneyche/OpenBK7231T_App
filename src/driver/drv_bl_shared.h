#pragma once

#include "../httpserver/new_http.h"

void BL_Shared_Init(void);
void BL_ProcessUpdate(float voltage, float current, float power,
                      float frequency);
void BL09XX_AppendInformationToHTTPIndexPage(http_request_t *request);

extern float g_apparentPower;
extern float g_powerFactor;
extern float g_reactivePower;

