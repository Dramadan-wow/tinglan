// QR encoder structure checks. With a file argument it also writes the matrix as text ('#' dark, '.' light)
// so an independent decoder can read it back.
#include "QrCodeHsx.h"
#include <QFile>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace {
int checks=0;
void expect(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(std::string("FAILED: ")+what);}
bool finder(const im::net::QrCodeHsx& q,int left,int top){
    for(int y=0;y<7;++y)for(int x=0;x<7;++x){const int d=std::max(std::abs(x-3),std::abs(y-3));if(q.dark(left+x,top+y)!=(d!=2))return false;}return true;
}
// Read the first copy of the format information and check its BCH code.
int formatMask(const im::net::QrCodeHsx& q){
    int bits=0;auto put=[&](int i,bool v){if(v)bits|=1<<i;};
    for(int i=0;i<=5;++i)put(i,q.dark(8,i));
    put(6,q.dark(8,7));put(7,q.dark(8,8));put(8,q.dark(7,8));
    for(int i=9;i<15;++i)put(i,q.dark(14-i,8));
    bits^=0x5412;const int data=bits>>10;int rem=data;for(int i=0;i<10;++i)rem=(rem<<1)^((rem>>9)*0x537);
    return ((data<<10)|rem)==bits&&(data>>3)==0?data&7:-1;
}
}
int main(int argc,char** argv){
    try{
        const QByteArray text="TINGLANQR1:0123456789abcdef0123456789abcdef";
        const auto q=im::net::QrCodeHsx::encode(text);
        expect(q.version()==4&&q.size()==33,"a 43-byte sign-in code needs version 4 at level M");
        expect(finder(q,0,0)&&finder(q,q.size()-7,0)&&finder(q,0,q.size()-7),"three finder patterns in the corners");
        bool timing=true;for(int i=8;i<q.size()-8;++i)timing=timing&&q.dark(i,6)==(i%2==0)&&q.dark(6,i)==(i%2==0);expect(timing,"timing patterns alternate");
        expect(q.dark(8,q.size()-8),"dark module is set");
        expect(formatMask(q)==q.mask(),"format information carries level M and the chosen mask");
        expect(im::net::QrCodeHsx::encode(QByteArray(213,'x')).version()==10,"213 bytes need version 10");
        bool rejected=false;try{im::net::QrCodeHsx::encode(QByteArray(214,'x'));}catch(const std::runtime_error&){rejected=true;}expect(rejected,"longer data is refused");
        if(argc>1){
            QFile out(QString::fromLocal8Bit(argv[1]));expect(out.open(QIODevice::WriteOnly),"matrix file opens");
            for(const auto& sample:{text,QByteArray("听澜 TINGLAN 扫码登录测试 / version check ")+QByteArray(120,'z')}){
                const auto m=im::net::QrCodeHsx::encode(sample);out.write(sample.toBase64()+"\n");
                for(int y=0;y<m.size();++y){QByteArray row;for(int x=0;x<m.size();++x)row+=m.dark(x,y)?'#':'.';out.write(row+"\n");}out.write("\n");
            }
        }
        std::printf("qr code checks passed: %d\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
