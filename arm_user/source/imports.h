#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "types.h"

int UhsServerGet(uint32_t idx, UhsServer** out_server);

int devFsm_send_event(UhsServer* server, UhsDevice* device, uint32_t event, void* arg);
