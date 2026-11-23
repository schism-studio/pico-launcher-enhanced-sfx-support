#pragma once
#include "lidSfxConfig.h"
void lidSfxConfigure(const LidSfxConfig* config);
u32 lidSfxBeginSleep();
void lidSfxCancelSleep();
void lidSfxWake();
void lidSfxStop();
bool lidSfxAllowUi();
