#pragma once
// OFFLINE supplied-clear XMQ application ONLY; upstream header is AES Mode 5.
// Never call on live RF without verified decryption and identity matching.
// GPL-3.0-or-later reference: wmbusmeters/drivers/src/lansenth.xmq
#include <cstddef>
#include <cstdint>
namespace xmq_portable { namespace lansenth_clear {
enum class Status:uint8_t {Ok,Invalid,Missing,Duplicate,Unsupported};
struct Reading {double temperature_c{0},temperature_1h_c{0},temperature_24h_c{0},
humidity_rh{0},humidity_1h_rh{0},humidity_24h_rh{0};};
inline int16_t signed16(const uint8_t*p){unsigned v=unsigned(p[0])|(unsigned(p[1])<<8);
return int16_t(v<32768?v:int(v)-65536);}
inline Status decode(const uint8_t*data,size_t len,Reading&out){
out={};if(!data||len<2||data[0]!=0x2f||data[1]!=0x2f)return Status::Invalid;
Reading r{};unsigned mask=0;size_t i=2;
while(i<len){if(data[i]==0x2f){++i;continue;}
unsigned slot=6;size_t hdr=0;
if(i+2<=len&&data[i]==0x02&&data[i+1]==0x65){slot=0;hdr=2;}
else if(i+2<=len&&data[i]==0x42&&data[i+1]==0x65){slot=1;hdr=2;}
else if(i+3<=len&&data[i]==0x82&&data[i+1]==0x01&&data[i+2]==0x65){slot=2;hdr=3;}
else if(i+3<=len&&data[i]==0x02&&data[i+1]==0xfb&&data[i+2]==0x1a){slot=3;hdr=3;}
else if(i+3<=len&&data[i]==0x42&&data[i+1]==0xfb&&data[i+2]==0x1a){slot=4;hdr=3;}
else if(i+4<=len&&data[i]==0x82&&data[i+1]==0x01&&data[i+2]==0xfb&&data[i+3]==0x1a){slot=5;hdr=4;}
if(slot>5||i+hdr+2>len)return Status::Unsupported;
if(mask&(1u<<slot))return Status::Duplicate;
double v=double(signed16(data+i+hdr))/(slot<3?100.0:10.0);
if((slot<3&&(v< -100||v>150))||(slot>=3&&(v<0||v>100)))return Status::Unsupported;
switch(slot){case 0:r.temperature_c=v;break;case 1:r.temperature_1h_c=v;break;
case 2:r.temperature_24h_c=v;break;case 3:r.humidity_rh=v;break;
case 4:r.humidity_1h_rh=v;break;case 5:r.humidity_24h_rh=v;break;}
mask|=1u<<slot;i+=hdr+2;}
if(mask!=63)return Status::Missing;out=r;return Status::Ok;
}
}}