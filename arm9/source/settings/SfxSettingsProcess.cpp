#include "common.h"
#include "SfxSettingsProcess.h"
#include "App.h"
#include "sfx/SfxAssets.h"
#include "core/math/ColorConverter.h"
#include "core/mini-printf.h"
#include "themes/material/MaterialColorSchemeFactory.h"
#include "themes/ThemeInfoFactory.h"
#include "gui/GraphicsContext.h"
#include "gui/Alignment.h"
#include <libtwl/gfx/gfx.h>
#include <libtwl/gfx/gfxOam.h>
#include <libtwl/gfx/gfxPalette.h>
#include <libtwl/gfx/gfxBackground.h>
#include <libtwl/mem/memVram.h>
#include <libtwl/dma/dmaNitro.h>
#include <libtwl/rtos/rtosIrq.h>
#include <nds/arm9/cache.h>
#include <malloc.h>
#include <string.h>
SfxSettingsProcess::SfxSettingsProcess(IAppSettingsService& settings):_settings(settings),
    _mainPalette(GFX_PLTT_OBJ_MAIN),_mainVram(GFX_OBJ_MAIN,96*1024),_subVram(GFX_OBJ_SUB,16*1024),
    _mainContext(nullptr,&_mainVram,nullptr,nullptr),_subContext(nullptr,&_subVram,nullptr,nullptr),
    _input(&_keys,&_touch),_repeat(&_input,InputKey::DpadUp|InputKey::DpadDown|InputKey::DpadLeft|InputKey::DpadRight|InputKey::L|InputKey::R,25,8) {}
void SfxSettingsProcess::Run(){
    rtos_disableIrqMask(RTOS_IRQ_VCOUNT);rtos_setIrqFunc(RTOS_IRQ_VCOUNT,nullptr);dma_ntrStopDirect(0);
    mem_setVramBMapping(MEM_VRAM_AB_MAIN_OBJ_00000);mem_setVramIMapping(MEM_VRAM_I_SUB_OBJ_00000);
    mem_setVramCMapping(MEM_VRAM_C_SUB_BG_00000);
    MaterialColorSchemeFactory::FromPrimaryColor(_themePrimary,false,_colors);
    const auto& settings=_settings.GetAppSettings();_state.Begin(settings.sfxEnabled,settings.sfxPack.GetString());
    strlcpy(_theme,settings.theme.GetString(),sizeof(_theme));
    _defaultArt.reset(static_cast<SfxBankArtwork*>(memalign(32,sizeof(SfxBankArtwork))));
    if(!_defaultArt){LOG_ERROR("No sound-bank artwork buffer\n");gProcessManager.Goto<App>();return;}
    if(_state.savedEnabled)_defaultArt->Default(_colors);else _defaultArt->Off(_colors);
    _banks=std::make_unique<SfxPackRepository>();
    _view=SfxSettingsView::CreateShared(static_cast<ISfxMenuHost*>(this),&_colors,&_fonts);_view->InitVram(_mainContext);
    for(int i=0;i<2;++i){
        _info[i]=Label2DView::CreateShared(208,16,112,_fonts.GetFont(FontType::Regular10));
        _info[i]->SetPosition(44,72+i*22);_info[i]->SetEllipsisStyle(LabelView::EllipsisStyle::Ellipsis);_info[i]->InitVram(_mainContext);
    }
    _title=Label2DView::CreateShared(128,16,16,_fonts.GetFont(FontType::Medium10));
    _title->SetText("SFX Options");_title->SetHorizontalAlignment(Alignment::Center);
    _title->SetPosition(64,8);_title->InitVram(_subContext);
    if(_mainVram.Failed()||_subVram.Failed()){LOG_ERROR("Sound selector VRAM limit exceeded\n");gProcessManager.Goto<App>();return;}
    REG_DISPCNT=0x00211010;
    // Centered 128x128 BG2 package plus a small OBJ title above it; no bitmap geometry changes.
    REG_DISPCNT_SUB=0x00211415;REG_BG2CNT_SUB=0x0084;
    gfx_setSubBg2Affine(256,0,0,256,-64*256,-32*256);REG_BLDCNT=0;REG_BLDCNT_SUB=0;
    GFX_PLTT_BG_MAIN[0]=ColorConverter::ToGBGR565(_colors.inverseOnSurface);
    GFX_PLTT_BG_SUB[0]=ColorConverter::ToGBGR565(_colors.inverseOnSurface);
    rtos_setIrqFunc(RTOS_IRQ_VCOUNT,[](u32){static_cast<SfxSettingsProcess*>(gProcessManager.GetRunningProcess())->VCountIrq();});
    _queue.StartThread(1,_ioStack,sizeof(_ioStack));StartScan();
    while(true){
        Poll();Refresh();if(!_exit&&!_closing)HandleInput();
        Refresh();_view->Update();TrackPreview();for(auto& line:_info)line->Update();_title->Update();
        Draw();::VBlank::Wait();VBlank();
        if(_exit){if(_fade<16)++_fade;}else if(_fade>0)--_fade;
        REG_MASTER_BRIGHT=_fade?0x4000|_fade:0;REG_MASTER_BRIGHT_SUB=_fade?0x4000|_fade:0;
        if(_exit&&_fade>=16&&_job==Job::None)break;
    }
    rtos_disableIrqMask(RTOS_IRQ_VCOUNT);rtos_setIrqFunc(RTOS_IRQ_VCOUNT,nullptr);dma_ntrStopDirect(0);_queue.StopThread();
}
void SfxSettingsProcess::Exit(){_exit=true;__atomic_store_n(&_cancelScan,true,__ATOMIC_RELEASE);__atomic_store_n(&_cancelArt,true,__ATOMIC_RELEASE);}
void SfxSettingsProcess::Info(int index,const char* text){
    if(index<0||index>=2)return;
    if(strcmp(_infoText[index],text)){strlcpy(_infoText[index],text,sizeof(_infoText[index]));_info[index]->SetText(_infoText[index]);}
}
void SfxSettingsProcess::Message(const char* text,const char* detail){strlcpy(_message,text,sizeof(_message));strlcpy(_detail,detail,sizeof(_detail));}
void SfxSettingsProcess::StartScan(){
    _state.phase=SfxMenuState::Phase::Reading;_job=Job::Scan;_elapsed=0;
    __atomic_store_n(&_cancelScan,false,__ATOMIC_RELEASE);__atomic_store_n(&_done,false,__ATOMIC_RELEASE);
    Message("Reading sound banks...","B returns to the browser.");
    _work=_queue.Enqueue([this](const vu8&){
        _banks->Scan(&_cancelScan);
        if(!__atomic_load_n(&_cancelScan,__ATOMIC_ACQUIRE)){
            ThemeInfoFactory factory;auto theme=factory.CreateFromThemeFolder(_theme);
            if(theme){_themePrimary=theme->GetPrimaryColor();_themeDark=theme->GetIsDarkTheme();}
        }
        __atomic_store_n(&_done,true,__ATOMIC_RELEASE);return TaskResult<void>::Completed();
    });
}
void SfxSettingsProcess::StartSave(){
    _saveRequested=false;_job=Job::Save;_elapsed=0;__atomic_store_n(&_done,false,__ATOMIC_RELEASE);
    Message("Applying sound preferences...","Please wait; B returns after saving.");
    _work=_queue.Enqueue([this](const vu8&){
        _result=_settings.SaveSfx(_saveEnabled,_saveBank);
        __atomic_store_n(&_done,true,__ATOMIC_RELEASE);return TaskResult<void>::Completed();
    });
}
void SfxSettingsProcess::Poll(){
    if(_job!=Job::None){
        ++_elapsed;
        if(_elapsed==300&&_job!=Job::Art){Message("Card busy; please wait.","B requests a safe return.");_error=_job!=Job::Save;}
        if(!__atomic_load_n(&_done,__ATOMIC_ACQUIRE)||!_work.GetTask().IsCompleted())return;
        Job finished=_job;_job=Job::None;_work=QueueTask<void>();
        if(finished==Job::Scan){
            _state.phase=SfxMenuState::Phase::Ready;
            if(!_closing){
                MaterialColorSchemeFactory::FromPrimaryColor(_themePrimary,_themeDark,_colors);
                if(_state.savedEnabled)_defaultArt->Default(_colors);else _defaultArt->Off(_colors);
                _imageDirty=true;
                GFX_PLTT_BG_MAIN[0]=ColorConverter::ToGBGR565(_colors.inverseOnSurface);
                GFX_PLTT_BG_SUB[0]=ColorConverter::ToGBGR565(_colors.inverseOnSurface);
                int selected=_state.savedEnabled?SfxSettingsView::Default:SfxSettingsView::Off;
                if(_state.savedEnabled)for(u32 i=0;i<_banks->Count();++i)
                    if(!strcasecmp(_state.savedBank,_banks->Get(i)->folder))selected=i+SfxSettingsView::FirstBank;
                _view->SetEnabled(false,_state.savedEnabled);_view->SetBanks(_banks.get(),_state.savedBank,selected,_mainContext);
                if(_mainVram.Failed()){LOG_ERROR("Sound selector list VRAM limit exceeded\n");RequestLeave();}
                else _view->FocusFirst(_focus);
                _initial=false;_error=false;
                if(_banks->ReadError()||_banks->Truncated()){
                    Message(_banks->ReadError()?"Some banks could not be read.":"Bank list limit reached.","A continues. B returns.");_error=true;
                }
            }
        }else if(finished==Job::Art){
            if(!_closing&&!__atomic_load_n(&_cancelArt,__ATOMIC_ACQUIRE)&&_loadingArt==_wantedArt){
                _frontArt=std::move(_pendingArt);_frontIndex=_loadingArt;_imageDirty=true;
            }else{_pendingArt.reset();if(_attemptedArt==_loadingArt)_attemptedArt=-2;}
        }else if(_state.FinishSave(_result)){
            auto& settings=_settings.GetAppSettings();settings.sfxEnabled=_saveEnabled;settings.sfxPack=_saveBank;RequestLeave();
        }else{
            _error=true;
            switch(_result){
                case SfxSaveResult::Recovery:Message("Settings need recovery.","B returns. No choice was applied.");break;
                case SfxSaveResult::Unsupported:Message("Saving is unavailable.","B returns. No choice was applied.");break;
                case SfxSaveResult::Memory:Message("Not enough memory to save.","Y retries. B keeps the old choice.");break;
                default:Message("Could not save. Old choice kept.","Y retries. B returns.");break;
            }
            if(_backAfterSave)RequestLeave();
        }
    }
    if(_closing&&_job==Job::None&&!_exit){gProcessManager.Goto<App>();return;}
    if(_saveRequested&&_job==Job::None&&!_closing)StartSave();
}
void SfxSettingsProcess::RequestLeave(){
    _closing=true;_error=false;_state.discardArmed=false;
    __atomic_store_n(&_cancelScan,true,__ATOMIC_RELEASE);__atomic_store_n(&_cancelArt,true,__ATOMIC_RELEASE);
    if(_job==Job::None)gProcessManager.Goto<App>();else Message("Returning to the browser...","Waiting for the current card call.");
}
void SfxSettingsProcess::QueueChoice(bool enabled,const char* bank){
    if(!_state.Choose(enabled,bank))return;
    if(!_state.Dirty()){RequestLeave();return;}
    if(!_state.StartSave())return;
    _error=false;_backAfterSave=false;_saveEnabled=_state.enabled;strlcpy(_saveBank,_state.bank,sizeof(_saveBank));
    _saveRequested=true;__atomic_store_n(&_cancelArt,true,__ATOMIC_RELEASE);
    Message("Applying sound preferences...","Please wait; B returns after saving.");
    if(_job==Job::None)StartSave();
}
void SfxSettingsProcess::MenuAction(int action){
    if(_closing||_exit)return;
    if(action==SfxSettingsView::Back){
        if(_state.phase==SfxMenuState::Phase::Saving){_backAfterSave=true;Message("Returning after this save...","Please wait for the card.");return;}RequestLeave();return;
    }
    if(_state.phase!=SfxMenuState::Phase::Ready||_error)return;
    if(action==SfxSettingsView::Off){QueueChoice(false,_state.savedBank);return;}
    if(action==SfxSettingsView::Default){QueueChoice(true,"default");return;}
    const auto* bank=action>=SfxSettingsView::FirstBank?_banks->Get(action-SfxSettingsView::FirstBank):nullptr;
    if(bank)QueueChoice(true,bank->folder);
}
void SfxSettingsProcess::TrackPreview(){
    if(_initial||_closing||_exit)return;
    int selected=_view->Selected();
    if(selected!=_wantedArt){
        _wantedArt=selected;_imageDirty=true;__atomic_store_n(&_cancelArt,true,__ATOMIC_RELEASE);
        if(selected==SfxSettingsView::Off)_defaultArt->Off(_colors);else _defaultArt->Default(_colors);
    }
    // Off is a built-in option, never a directory or an artwork IO request.
    if(selected==SfxSettingsView::Off)return;
    if(_job!=Job::None||_state.phase!=SfxMenuState::Phase::Ready||selected<0||_attemptedArt==selected||_error)return;
    _attemptedArt=selected;_loadingArt=selected;
    const auto* bank=selected>=SfxSettingsView::FirstBank?_banks->Get(selected-SfxSettingsView::FirstBank):nullptr;
    if(selected>=SfxSettingsView::FirstBank&&!bank)return;
    strlcpy(_artBank,bank?bank->folder:"default",sizeof(_artBank));
    _pendingArt.reset(static_cast<SfxBankArtwork*>(memalign(32,sizeof(SfxBankArtwork))));if(!_pendingArt)return;
    __atomic_store_n(&_cancelArt,false,__ATOMIC_RELEASE);__atomic_store_n(&_done,false,__ATOMIC_RELEASE);
    _job=Job::Art;_elapsed=0;
    _work=_queue.Enqueue([this](const vu8&){
        _pendingArt->Load(_artBank,_theme,_colors,&_cancelArt);
        __atomic_store_n(&_done,true,__ATOMIC_RELEASE);return TaskResult<void>::Completed();
    });
}
void SfxSettingsProcess::HandleInput(){
    if(!_armed){if(!_repeat.Current(InputKey::A|InputKey::B|InputKey::Touch))_armed=true;return;}
    if(_repeat.Triggered(InputKey::B)){MenuAction(SfxSettingsView::Back);return;}
    // Keep the visible Back button usable while list activation is gated.
    if(_error||_state.phase==SfxMenuState::Phase::Saving){
        Point point;
        if(_repeat.Triggered(InputKey::Touch)&&_repeat.GetCurrentTouchPoint(point))
            _backTouch=point.x>=0&&point.x<32&&point.y>=0&&point.y<32;
        if(_backTouch&&_repeat.Current(InputKey::Touch)&&_repeat.GetCurrentTouchPoint(point)&&
            !(point.x>=0&&point.x<32&&point.y>=0&&point.y<32))_backTouch=false;
        if(_repeat.Released(InputKey::Touch)&&_backTouch){_backTouch=false;MenuAction(SfxSettingsView::Back);return;}
    }
    if(_error){
        if(_state.phase==SfxMenuState::Phase::Ready&&_repeat.Triggered(InputKey::Y)&&_state.Dirty()){
            _error=false;if(_state.StartSave()){_saveRequested=true;if(_job==Job::None)StartSave();}
        }else if(_state.phase==SfxMenuState::Phase::Ready&&_repeat.Triggered(InputKey::A)&&!_state.Dirty())_error=false;
        return;
    }
    if(_state.phase==SfxMenuState::Phase::Saving)return;
    _focus.Update(_repeat);Point point;
    if(_repeat.Triggered(InputKey::Touch)&&_repeat.GetCurrentTouchPoint(point)){_view->HandlePenDown(point,_focus);_lastTouch=point;}
    else if(_repeat.Released(InputKey::Touch))_view->HandlePenUp(_lastTouch,_focus);
    else if(_repeat.Current(InputKey::Touch)&&_repeat.GetCurrentTouchPoint(point)){_view->HandlePenMove(point,_focus);_lastTouch=point;}
}
void SfxSettingsProcess::Refresh(){
    _view->SetEnabled(_state.phase==SfxMenuState::Phase::Ready&&!_error&&!_closing,_state.savedEnabled);Info(0,_message);Info(1,_detail);
}
void SfxSettingsProcess::Draw(){
    _mainOam.Clear();_subOam.Clear();_subPalette.Reset();GraphicsContext main(&_mainOam,&_mainPalette,nullptr);_view->Draw(main);
    if(_error||_closing||_state.phase==SfxMenuState::Phase::Reading||_state.phase==SfxMenuState::Phase::Saving){
        for(auto& line:_info){line->SetBackgroundColor(_colors.surfaceBright);line->SetForegroundColor(_colors.onSurface);line->Draw(main);}
    }
    GraphicsContext sub(&_subOam,&_subPalette,nullptr);
    _title->SetBackgroundColor(_colors.inverseOnSurface);_title->SetForegroundColor(_colors.onSurface);_title->Draw(sub);
    _mainPalette.EndOfFrame();
}
void SfxSettingsProcess::VBlank(){
    dma_ntrStopDirect(0);_input.Sample();_repeat.Update();_mainOam.Apply(GFX_OAM_MAIN);
    _subOam.Apply(GFX_OAM_SUB);_subPalette.Apply(GFX_PLTT_OBJ_SUB);_title->VBlank();
    // Publish a valid first palette scheme before unmasking its interrupt handler.
    if(!_paletteIrq){_mainPalette.VBlank();rtos_ackIrqMask(RTOS_IRQ_VCOUNT);rtos_enableIrqMask(RTOS_IRQ_VCOUNT);_paletteIrq=true;}
    else _mainPalette.VBlank();
    _view->VBlank();for(auto& line:_info)line->VBlank();
    if(_imageDirty){
        const auto* art=_frontArt&&_frontIndex==_wantedArt?_frontArt.get():_defaultArt.get();
        DC_FlushRange(art->square,sizeof(art->square));dma_ntrCopy32(3,art->square,GFX_BG_SUB,sizeof(art->square));_imageDirty=false;
    }
}
void SfxSettingsProcess::VCountIrq(){_mainPalette.VCount();}
