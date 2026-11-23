#include "common.h"
#include "SfxMenuState.h"
#include "sfx/SfxAssets.h"
#include <string.h>
void SfxMenuState::Begin(bool value,const char* name) {
    enabled=savedEnabled=value;
    strlcpy(bank,SfxAssets::IsDefault(name)?"default":name,sizeof(bank));
    strlcpy(savedBank,bank,sizeof(savedBank)); discardArmed=false;phase=Phase::Reading;
}
bool SfxMenuState::Dirty() const {return enabled!=savedEnabled || strcasecmp(bank,savedBank)!=0;}
bool SfxMenuState::Choose(bool value,const char* name) {
    if (phase!=Phase::Ready || !name || (!SfxAssets::IsDefault(name)&&!SfxAssets::SafeFolder(name))) return false;
    enabled=value;
    if (SfxAssets::IsDefault(name)) strlcpy(bank,"default",sizeof(bank));
    else if (name!=bank) strlcpy(bank,name,sizeof(bank));
    discardArmed=false;return true;
}
bool SfxMenuState::StartSave() {
    if (phase!=Phase::Ready || !Dirty()) return false;
    phase=Phase::Saving;discardArmed=false;return true;
}
bool SfxMenuState::FinishSave(SfxSaveResult result) {
    if (phase!=Phase::Saving) return false;
    if (result==SfxSaveResult::Ok) {savedEnabled=enabled;strlcpy(savedBank,bank,sizeof(savedBank));phase=Phase::Ready;return true;}
    phase=result==SfxSaveResult::Recovery || result==SfxSaveResult::Unsupported ? Phase::Locked:Phase::Ready;
    return false;
}
bool SfxMenuState::Back(bool confirmed) {
    if (phase==Phase::Saving) return false;
    if (confirmed || !Dirty() || discardArmed || phase==Phase::Locked) return true;
    discardArmed=true;return false;
}
