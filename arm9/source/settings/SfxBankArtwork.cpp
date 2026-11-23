#include "common.h"
#include "SfxBankArtwork.h"
#include "sfx/SfxAssets.h"
#include "fat/File.h"
#include "romBrowser/FileType/BmpHeader.h"
#include "themes/material/MaterialColorScheme.h"
#include "core/math/ColorConverter.h"
#include "core/mini-printf.h"
#include <stdlib.h>
#include <string.h>
namespace {
u32 Read32(const u8* p){return p[0]|(u32(p[1])<<8)|(u32(p[2])<<16)|(u32(p[3])<<24);}
bool Cancelled(const bool* p){return p&&__atomic_load_n(p,__ATOMIC_ACQUIRE);}
u16 Pixel(unsigned r,unsigned g,unsigned b){return 0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10);}
}
void SfxBankArtwork::Speaker(u8* tiles,u16* pal,const MaterialColorScheme& colors){
    memset(tiles,0,512);memset(pal,0,32);pal[1]=ColorConverter::ToGBGR565(colors.primary);
    for(int y=0;y<32;++y)for(int x=0;x<32;++x){
        int dy=y-16,dx=x-17,d=dx*dx+dy*dy;
        bool ink=(x>=6&&x<=12&&y>=12&&y<=20)||(x>=12&&x<=19&&abs(dy)<=x-11)||
            (x>=22&&((d>=65&&d<=90)||(d>=145&&d<=175)));
        if(ink){unsigned at=((y/8)*4+x/8)*32+(y%8)*4+(x%8)/2;tiles[at]|=1<<((x&1)*4);}
    }
}
void SfxBankArtwork::MutedSpeaker(u8* tiles,u16* pal,const MaterialColorScheme& colors){
    Speaker(tiles,pal,colors);
    pal[1]=ColorConverter::ToGBGR565(colors.outline);
    pal[2]=ColorConverter::ToGBGR565(Rgb<8,8,8>(200,45,45));
    pal[3]=ColorConverter::ToGBGR565(colors.surfaceBright);
    // A contrasting diagonal slash, with a clear edge across the speaker cone.
    for(unsigned y=3;y<29;++y)for(unsigned x=3;x<29;++x){
        int distance=abs(static_cast<int>(x+y)-31);
        if(distance>2)continue;
        unsigned at=((y/8)*4+x/8)*32+(y%8)*4+(x%8)/2,shift=(x&1)*4;
        unsigned index=distance<=1?2:3;
        tiles[at]=(tiles[at]&~(15u<<shift))|(index<<shift);
    }
}
void SfxBankArtwork::Off(const MaterialColorScheme& colors){MutedSpeaker(icon,palette,colors);Package(colors);PaintIcon();}
void SfxBankArtwork::Package(const MaterialColorScheme& colors){
    memset(square,0,sizeof(square));auto c=colors.surfaceBright;u16 color=Pixel(c.r,c.g,c.b);
    for(int y=4;y<124;++y)for(int x=4;x<124;++x){
        int dx=x<16?16-x:x>111?x-111:0,dy=y<16?16-y:y>111?y-111:0;
        if(dx*dx+dy*dy<=144)square[y*128+x]=color;
    }
}
void SfxBankArtwork::PaintIcon(){
    for(unsigned y=0;y<96;++y)for(unsigned x=0;x<96;++x){
        unsigned ix=x/3,iy=y/3,at=((iy/8)*4+ix/8)*32+(iy%8)*4+(ix%8)/2;
        unsigned index=(icon[at]>>((ix&1)*4))&15;
        if(index)square[(16+y)*128+16+x]=0x8000|(palette[index]&0x7FFF);
    }
}
void SfxBankArtwork::Default(const MaterialColorScheme& colors){Speaker(icon,palette,colors);Package(colors);PaintIcon();}
bool SfxBankArtwork::ReadIcon(const char* path){
    File file;if(file.Open(path,FA_READ|FA_OPEN_EXISTING)!=FR_OK)return false;
    u8 head[BmpHeader::MaxHeaderSize+64];BmpHeader h;auto size=file.GetSize();
    if(size>16384||!file.ReadExact(head,sizeof(head))||head[26]!=1||head[27]||
        !BmpHeader::Parse(head,32,32,4,h)||h.dataOffset>size||size-h.dataOffset<512)return false;
    u8 raw[512],out[512];u16 pal[16]={};
    for(unsigned i=0;i<h.paletteCount;++i){const u8* p=head+h.paletteOffset+i*4;pal[i]=ColorConverter::ToGBGR565(Rgb<8,8,8>(p[2],p[1],p[0]));}
    if(file.Seek(h.dataOffset)!=FR_OK||!file.ReadExact(raw,sizeof(raw)))return false;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x){
        unsigned sy=h.topDown?y:31-y,index=(raw[sy*16+x/2]>>((x&1)?0:4))&15;
        unsigned at=((y/8)*4+x/8)*32+(y%8)*4+x%8/2;
        if(!(x&1))out[at]=index;else out[at]|=index<<4;
    }
    memcpy(icon,out,sizeof(icon));memcpy(palette,pal,sizeof(palette));return true;
}
bool SfxBankArtwork::ReadPreview(const char* path,const bool* cancel){
    File file;if(file.Open(path,FA_READ|FA_OPEN_EXISTING)!=FR_OK)return false;
    u8 h[54];auto size=file.GetSize();
    if(size>256*1024||!file.ReadExact(h,sizeof(h))||h[0]!='B'||h[1]!='M')return false;
    u32 offset=Read32(h+10),dib=Read32(h+14),width=Read32(h+18);s32 height=(s32)Read32(h+22);
    u32 bpp=h[28]|(h[29]<<8),stride=128*(bpp/8);
    if(dib<40||dib>124||width!=128||(height!=128&&height!=-128)||h[26]!=1||h[27]||
        (bpp!=24&&bpp!=32)||Read32(h+30)||offset<14+dib||offset>size||size-offset<stride*128)return false;
    u16* decoded=static_cast<u16*>(malloc(128*128*2));if(!decoded)return false;
    u8 row[512];bool valid=true;
    for(unsigned y=0;y<128;++y){
        if(Cancelled(cancel)){valid=false;break;}
        unsigned sy=height<0?y:127-y;
        if(file.Seek(offset+sy*stride)!=FR_OK||!file.ReadExact(row,stride)){valid=false;break;}
        for(unsigned x=0;x<128;++x){const u8* p=row+x*(bpp/8);decoded[y*128+x]=Pixel(p[2],p[1],p[0]);}
    }
    if(valid)for(unsigned y=0;y<96;++y)for(unsigned x=0;x<96;++x)
        square[(y+16)*128+x+16]=decoded[(y*128/96)*128+x*128/96];
    free(decoded);return valid;
}
void SfxBankArtwork::Load(const char* bank,const char* theme,const MaterialColorScheme& colors,const bool* cancel){
    Default(colors);if(Cancelled(cancel))return;char root[512],path[544];int length;
    (void)theme; // Default's artwork belongs to the customizable stock folder, not the theme.
    if(SfxAssets::IsDefault(bank))bank=SfxAssets::kDefaultBank;
    if(!SfxAssets::SafeFolder(bank))return;
    length=mini_snprintf(root,sizeof(root),"%s/%s",SfxAssets::kBankRoot,bank);
    if(length<0||(unsigned)length>=sizeof(root))return;
    mini_snprintf(path,sizeof(path),"%s/icon.bmp",root);if(ReadIcon(path)){Package(colors);PaintIcon();}
    if(Cancelled(cancel))return;
    mini_snprintf(path,sizeof(path),"%s/preview.bmp",root);ReadPreview(path,cancel);
}
