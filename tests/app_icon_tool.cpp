// Writes assets/icons/tinglan.ico from IconHsx::appImage, so the exe, shortcuts and installers show the same
// mark as the windows. Sizes up to 128 px are classic 32-bit bitmaps that every reader accepts; 256 px is PNG.
// Usage: app_icon_tool <output.ico>
#include "IconHsx.h"
#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QtEndian>
#include <cstdio>
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);if(argc!=2){std::fprintf(stderr,"usage: app_icon_tool <output.ico>\n");return 2;}
    const QList<int> sizes{16,20,24,32,40,48,64,128,256};QList<QByteArray> images;
    auto le=[](QByteArray& out,quint32 v,int bytes){for(int i=0;i<bytes;++i)out.append(char((v>>(8*i))&0xff));};
    for(int size:sizes){
        const auto image=im::IconHsx::appImage(size).convertToFormat(QImage::Format_ARGB32);QByteArray entry;
        if(size>=256){QBuffer out(&entry);out.open(QIODevice::WriteOnly);if(!image.save(&out,"PNG"))return 3;images.append(entry);continue;}
        const int mask=((size+31)/32)*4*size; // AND mask rows, padded to 32 bits; transparency comes from alpha
        for(quint32 v:{40u,quint32(size),quint32(size*2)})le(entry,v,4);le(entry,1,2);le(entry,32,2);le(entry,0,4);le(entry,quint32(size*size*4+mask),4);for(int i=0;i<4;++i)le(entry,0,4);
        for(int y=size-1;y>=0;--y){const auto* row=reinterpret_cast<const QRgb*>(image.constScanLine(y));for(int x=0;x<size;++x){entry.append(char(qBlue(row[x])));entry.append(char(qGreen(row[x])));entry.append(char(qRed(row[x])));entry.append(char(qAlpha(row[x])));}}
        entry.append(QByteArray(mask,'\0'));images.append(entry);
    }
    QByteArray ico;auto u16=[&](quint16 v){char b[2];qToLittleEndian(v,b);ico.append(b,2);};auto u32=[&](quint32 v){char b[4];qToLittleEndian(v,b);ico.append(b,4);};
    u16(0);u16(1);u16(quint16(sizes.size()));quint32 offset=6+16*quint32(sizes.size());
    for(int i=0;i<sizes.size();++i){const char edge=char(sizes[i]>=256?0:sizes[i]);ico.append(edge);ico.append(edge);ico.append(char(0));ico.append(char(0));u16(1);u16(32);u32(quint32(images[i].size()));u32(offset);offset+=quint32(images[i].size());}
    for(const auto& png:images)ico.append(png);
    QFile file(QString::fromLocal8Bit(argv[1]));if(!file.open(QIODevice::WriteOnly)||file.write(ico)!=ico.size())return 4;
    std::printf("wrote %lld bytes, %lld sizes\n",static_cast<long long>(ico.size()),static_cast<long long>(sizes.size()));return 0;
}
