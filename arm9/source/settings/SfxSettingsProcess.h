#pragma once
#include <memory>
#include "services/process/IProcess.h"
#include "services/settings/IAppSettingsService.h"
#include "core/task/TaskQueue.h"
#include "gui/IVramManager.h"
#include "gui/FocusManager.h"
#include "gui/materialDesign.h"
#include "core/math/Point.h"
#include "gui/OamManager.h"
#include "gui/AdvancedPaletteManager.h"
#include "gui/SimplePaletteManager.h"
#include "settings/SfxBankArtwork.h"
#include <stdlib.h>
#include "gui/VramContext.h"
#include "gui/input/PadInputSource.h"
#include "gui/input/TouchInputSource.h"
#include "gui/input/SampledInputProvider.h"
#include "gui/input/InputRepeater.h"
#include "gui/views/Label2DView.h"
#include "themes/DefaultFontRepository.h"
#include "themes/material/MaterialColorScheme.h"
#include "settings/SfxMenuState.h"
#include "settings/views/SfxSettingsView.h"

// Bounded 2D allocator retained from the tested screen. No audio or texture arena.
class SfxMenuVram:public IVramManager {
    vu16* _base;u32 _capacity,_used=0;bool _failed=false;
public:
    SfxMenuVram(vu16* base,u32 capacity):_base(base),_capacity(capacity){}
    u32 Alloc(u32 bytes) override {
        if(bytes>_capacity){_failed=true;return 0;}
        bytes=(bytes+127)&~127u;
        if(bytes>_capacity-_used){_failed=true;return 0;}
        u32 at=_used;_used+=bytes;return at;
    }
    vu16* GetVramAddress(u32 offset) const override {return reinterpret_cast<vu16*>(reinterpret_cast<vu8*>(_base)+offset);}
    bool Failed() const {return _failed;}
};
class alignas(32) SfxSettingsProcess:public IProcess,private ISfxMenuHost {
public:
    explicit SfxSettingsProcess(IAppSettingsService& settings);
    void Run() override;
    void Exit() override;
private:
    IAppSettingsService& _settings;
    OamManager _mainOam,_subOam;
    AdvancedPaletteManager<64> _mainPalette;
    SimplePaletteManager _subPalette;
    SfxMenuVram _mainVram,_subVram;
    VramContext _mainContext,_subContext;
    DefaultFontRepository _fonts;
    MaterialColorScheme _colors;
    SharedPtr<SfxSettingsView> _view;
    SharedPtr<Label2DView> _title;
    SharedPtr<Label2DView> _info[2];
    char _infoText[2][320]={};
    std::unique_ptr<SfxPackRepository> _banks;
    SfxMenuState _state;
    TaskQueue<4,sizeof(TaskBase)+32> _queue;
    u32 _ioStack[8192/4];
    QueueTask<void> _work;
    using ArtPtr=std::unique_ptr<SfxBankArtwork,decltype(&free)>;
    ArtPtr _defaultArt{nullptr,&free},_frontArt{nullptr,&free},_pendingArt{nullptr,&free};
    int _wantedArt=-1,_loadingArt=-1,_frontIndex=-1,_attemptedArt=-2;
    bool _imageDirty=true,_error=false,_saveRequested=false,_backAfterSave=false,_paletteIrq=false;
    bool _cancelArt=false,_backTouch=false;
    char _theme[64]={},_artBank[256]={};
    Rgb<8,8,8> _themePrimary=Rgb<8,8,8>(138,217,255);
    bool _themeDark=false;
    enum class Job {None,Scan,Save,Art};
    Job _job=Job::None;
    bool _done=false,_cancelScan=false,_closing=false,_exit=false,_armed=false,_initial=true;
    bool _saveEnabled=true;
    char _saveBank[256]={},_message[112]={},_detail[112]={};
    SfxSaveResult _result=SfxSaveResult::Unsupported;
    u32 _elapsed=0;
    int _fade=16;
    PadInputSource _keys;
    TouchInputSource _touch;
    SampledInputProvider _input;
    InputRepeater _repeat;
    FocusManager _focus;
    Point _lastTouch=Point(0,0);
    void MenuAction(int action) override;
    void StartScan();
    void StartSave();
    void QueueChoice(bool enabled,const char* bank);
    void TrackPreview();
    void VCountIrq();
    void Poll();
    void Refresh();
    void Draw();
    void VBlank();
    void HandleInput();
    void Info(int index,const char* text);
    void Message(const char* text,const char* detail="");
    void RequestLeave();
};
