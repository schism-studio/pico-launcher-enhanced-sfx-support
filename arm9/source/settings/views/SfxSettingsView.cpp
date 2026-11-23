#include "common.h"
#include "SfxSettingsView.h"
#include "settings/SfxBankArtwork.h"
#include "romBrowser/Theme/Material/MaterialAppBarView.h"
#include "romBrowser/Theme/Material/MaterialBannerListItemView.h"
#include "romBrowser/viewModels/IRomBrowserItemViewModel.h"
#include "romBrowser/FileType/FileIcon.h"
#include "gui/GraphicsContext.h"
#include "gui/OamBuilder.h"
#include "gui/palette/DirectPalette.h"
#include "gui/input/InputProvider.h"
#include "gui/VramContext.h"
#include "themes/material/MaterialColorScheme.h"
#include <libtwl/dma/dmaNitro.h>
#include "backIcon.h"
#include <string.h>
namespace {
class BankIcon:public FileIcon {
    alignas(32)u8 _tiles[512];alignas(32)u16 _pal[16];
public:
    BankIcon(const MaterialColorScheme& colors,bool muted){
        if(muted)SfxBankArtwork::MutedSpeaker(_tiles,_pal,colors);else SfxBankArtwork::Speaker(_tiles,_pal,colors);
    }
    void UploadGraphics() override{if(_vramAddress)dma_ntrCopy32(3,_tiles,_vramAddress,sizeof(_tiles));}
    void Draw(GraphicsContext& context,const Rgb<8,8,8>& background) override{
        (void)background;if(!_vramAddress||!context.IsVisible(Rectangle(_position,32,32)))return;
        u32 pal=context.GetPaletteManager().AllocRow(DirectPalette(_pal),_position.y,_position.y+32);
        auto* oam=context.GetOamManager().AllocOams(1);
        OamBuilder::OamWithSize<32,32>(_position,_vramOffset>>7).WithPalette16(pal).WithPriority(context.GetPriority()).Build(oam[0]);
    }
};
class BankRowModel:public IRomBrowserItemViewModel {
    ISfxMenuHost* _host;int _index=-1;BannerListItemView* _row=nullptr;
    const MaterialColorScheme* _colors;bool _upload=false,_muted=false;
public:
    BankRowModel(ISfxMenuHost* host,const MaterialColorScheme* colors):_host(host),_colors(colors){}
    void Bound(BannerListItemView* row,bool muted){_row=row;_muted=muted;_upload=true;}
    void Activate() override{if(_index>=0)_host->MenuAction(_index);}
    void ShowGameInfo() override{}
    void ToggleFavorite() override{}
    void ToggleCompleted() override{}
    void SetIndex(int index) override{_index=index;}
    int GetIndex() const override{return _index;}
    void SetQueueTask(QueueTask<void> task) override{task.CancelTask();}
    void CancelQueueTask() override{_row=nullptr;_upload=false;}
    void DisposeQueueTaskWhenComplete() override{
        // The initial Bind happens before RecyclerView::InitVram. Upload in Update.
        if(_upload&&_row){_row->SetIcon(std::make_unique<BankIcon>(*_colors,_muted));_row->UploadIconGraphics();_upload=false;}
    }
};
}
class SfxBankAdapter:public RecyclerAdapter {
    ISfxMenuHost* _host;const MaterialColorScheme* _colors;const IFontRepository* _fonts;
    const SfxPackRepository* _banks=nullptr;char _saved[256]={};bool _soundEnabled=true;
    BannerListItemView::VramToken _graphics;
public:
    SfxBankAdapter(ISfxMenuHost* host,const MaterialColorScheme* colors,const IFontRepository* fonts):_host(host),_colors(colors),_fonts(fonts){}
    void SetBanks(const SfxPackRepository* banks,const char* saved,bool enabled){_banks=banks;_soundEnabled=enabled;strlcpy(_saved,saved,sizeof(_saved));}
    void InitVram(const VramContext& context){_graphics=MaterialBannerListItemView::UploadGraphics(context);}
    u32 GetItemCount() const override{return SfxSettingsView::FirstBank+(_banks?_banks->Count():0);}
    void GetViewSize(int& width,int& height) const override{width=203;height=44;}
    SharedPtr<View> CreateView() const override{
        return MaterialBannerListItemView::CreateShared(std::make_unique<BankRowModel>(_host,_colors),_colors,_fonts);
    }
    void BindView(SharedPtr<View> view,int index) const override{
        auto* row=static_cast<BannerListItemView*>(view.GetPointer());
        const bool off=index==SfxSettingsView::Off,defaults=index==SfxSettingsView::Default;
        const auto* entry=_banks&&index>=SfxSettingsView::FirstBank?_banks->Get(index-SfxSettingsView::FirstBank):nullptr;
        const char* bank=defaults?"default":entry?entry->folder:nullptr;
        const bool valid=off||bank;
        row->SetGraphics(_graphics);row->GetViewModel().SetIndex(valid?index:-1);
        row->SetFirstLineAsync(nullptr,off?"SFX off":defaults?"Default":bank?bank:"",true);
        row->SetSecondLineAsync(nullptr,off?(_soundEnabled?u"Disable sound effects":u"Current option"):
            defaults?u"Theme, then stock":bank&&_soundEnabled&&!strcasecmp(bank,_saved)?u"Current bank":u"Sound bank");
        row->SetThirdLineAsync(nullptr,u"");static_cast<BankRowModel&>(row->GetViewModel()).Bound(row,off);
    }
    void ReleaseView(SharedPtr<View> view,int index) const override{
        (void)index;auto* row=static_cast<BannerListItemView*>(view.GetPointer());
        row->GetViewModel().SetIndex(-1);row->GetViewModel().CancelQueueTask();row->SetIcon(nullptr);
    }
};
SfxSettingsView::SfxSettingsView(ISfxMenuHost* host,const MaterialColorScheme* colors,const IFontRepository* fonts):_host(host),_colors(colors){
    _bar=MaterialAppBarView::CreateShared(0,0,AppBarView::Orientation::Vertical,1,0,colors);
    _bar->SetButtonAction(0,[](IconButtonView*,void* p){static_cast<ISfxMenuHost*>(p)->MenuAction(Back);},host);
    _list=RecyclerView::CreateShared(42,0,256-42,192,RecyclerView::Mode::VerticalList);
    _list->SetPadding(0,3);_list->SetItemSpacing(0,3);
    _adapter=SharedPtr<SfxBankAdapter>::MakeShared(host,colors,fonts);
    AddChildTail(_bar.GetPointer());AddChildTail(_list.GetPointer());
}
void SfxSettingsView::InitVram(const VramContext& context){
    _bar->InitVram(context);auto* m=context.GetObjVramManager();if(!m)return;
    u32 back=m->Alloc(backIconTilesLen);dma_ntrCopy32(3,backIconTiles,m->GetVramAddress(back),backIconTilesLen);_bar->SetButtonIcon(0,back);
}
void SfxSettingsView::SetBanks(const SfxPackRepository* banks,const char* saved,int selected,const VramContext& context){
    if(_ready)return;
    _adapter->SetBanks(banks,saved,_soundEnabled);_adapter->InitVram(context);
    _list->SetAdapter(_adapter,selected);_list->InitVram(context);_ready=true;
}
void SfxSettingsView::SetEnabled(bool value,bool soundEnabled){
    // The process gates activation while IO is busy. Back remains usable.
    (void)value;_soundEnabled=soundEnabled;
}
void SfxSettingsView::FocusFirst(FocusManager& focus){if(_ready)_list->Focus(focus);else _bar->Focus(focus,0);}
int SfxSettingsView::Selected() const{return _ready?_list->GetSelectedItem():-1;}
void SfxSettingsView::Draw(GraphicsContext& context){ViewContainer::Draw(context);}
bool SfxSettingsView::HandleInput(const InputProvider& input,FocusManager& focus){
    if(input.Triggered(InputKey::B)){_host->MenuAction(Back);return true;}
    return View::HandleInput(input,focus);
}
SharedPtr<View> SfxSettingsView::MoveFocus(const SharedPtr<View>& current,FocusMoveDirection direction,View* source){
    if(!current)return nullptr;
    if(source==_bar.GetPointer()&&direction==FocusMoveDirection::Right&&_ready)return _list->MoveFocus(current,direction,this);
    if(source==_list.GetPointer()&&direction==FocusMoveDirection::Left)return _bar->MoveFocus(current,direction,this);
    return nullptr;
}
