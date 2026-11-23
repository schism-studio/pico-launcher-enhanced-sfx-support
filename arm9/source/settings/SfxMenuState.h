#pragma once
#include "services/settings/IAppSettingsService.h"
class SfxMenuState {
public:
    enum class Phase { Reading, Ready, Saving, Locked };
    Phase phase=Phase::Reading;
    bool enabled=true,savedEnabled=true,discardArmed=false;
    char bank[256]="default",savedBank[256]="default";
    void Begin(bool value,const char* name);
    bool Dirty() const;
    bool Choose(bool value,const char* name);
    bool StartSave();
    bool FinishSave(SfxSaveResult result);
    bool Back(bool confirmed);
};
