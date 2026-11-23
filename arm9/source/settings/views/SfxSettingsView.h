#pragma once
#include "gui/views/ViewContainer.h"
#include "gui/views/RecyclerView.h"
#include "romBrowser/views/AppBarView.h"
#include "sfx/SfxPackRepository.h"
class MaterialColorScheme;
class IFontRepository;
class SfxBankAdapter;
class ISfxMenuHost {
public:virtual void MenuAction(int action)=0;
protected:~ISfxMenuHost()=default;
};
// Theme-selector Material cards, scrolling and geometry; no staged Save button.
class SfxSettingsView:public ViewContainer {
    SHARED_ONLY(SfxSettingsView)
public:
    enum {Back=-1,Off=0,Default=1,FirstBank=2};
    void InitVram(const VramContext& context) override;
    void SetBanks(const SfxPackRepository* banks,const char* saved,int selected,const VramContext& context);
    void SetEnabled(bool value,bool soundEnabled);
    void Draw(GraphicsContext& context) override;
    SharedPtr<View> MoveFocus(const SharedPtr<View>& current,FocusMoveDirection direction,View* source) override;
    bool HandleInput(const InputProvider& input,FocusManager& focus) override;
    Rectangle GetBounds() const override{return Rectangle(0,0,256,192);}
    void FocusFirst(FocusManager& focus);
    int Selected() const;
private:
    ISfxMenuHost* _host;
    const MaterialColorScheme* _colors;
    SharedPtr<AppBarView> _bar;
    SharedPtr<RecyclerView> _list;
    SharedPtr<SfxBankAdapter> _adapter;
    bool _ready=false,_soundEnabled=true;
    SfxSettingsView(ISfxMenuHost* host,const MaterialColorScheme* colors,const IFontRepository* fonts);
};
