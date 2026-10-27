#include "QrCodeHsx.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
namespace im::net {
namespace {
// Error correction level M, indexed by version.
constexpr int EccPerBlock[11]={0,10,16,26,18,24,16,18,22,22,26};
constexpr int Blocks[11]={0,1,1,1,2,2,4,4,4,5,5};
int rawModules(int v){int r=(16*v+128)*v+64;if(v>=2){const int n=v/7+2;r-=(25*n-10)*n-55;if(v>=7)r-=36;}return r;}
int dataCodewords(int v){return rawModules(v)/8-EccPerBlock[v]*Blocks[v];}
// GF(2^8) with the QR polynomial x^8+x^4+x^3+x^2+1.
unsigned char multiply(unsigned char x,unsigned char y){int z=0;for(int i=7;i>=0;--i){z=(z<<1)^((z>>7)*0x11D);z^=((y>>i)&1)*x;}return static_cast<unsigned char>(z);}
std::vector<unsigned char> divisor(int degree){
    std::vector<unsigned char> r(static_cast<size_t>(degree),0);r.back()=1;unsigned char root=1;
    for(int i=0;i<degree;++i){for(size_t j=0;j<r.size();++j){r[j]=multiply(r[j],root);if(j+1<r.size())r[j]^=r[j+1];}root=multiply(root,2);}
    return r;
}
std::vector<unsigned char> remainder(const std::vector<unsigned char>& data,const std::vector<unsigned char>& div){
    std::vector<unsigned char> r(div.size(),0);
    for(auto b:data){const auto factor=static_cast<unsigned char>(b^r.front());r.erase(r.begin());r.push_back(0);for(size_t i=0;i<r.size();++i)r[i]^=multiply(div[i],factor);}
    return r;
}
std::vector<int> alignment(int v){
    if(v==1)return {};const int n=v/7+2,size=v*4+17,step=(v*4+n*2+1)/(n*2-2)*2;
    std::vector<int> r(static_cast<size_t>(n));r[0]=6;for(int i=n-1,pos=size-7;i>=1;--i,pos-=step)r[size_t(i)]=pos;return r;
}
}
QrCodeHsx QrCodeHsx::encode(const QByteArray& data){
    int version=1;while(version<=10&&4+(version<10?8:16)+8*data.size()>dataCodewords(version)*8)++version;
    if(version>10)throw std::runtime_error("二维码内容过长");
    std::vector<bool> bits;auto append=[&](unsigned value,int count){for(int i=count-1;i>=0;--i)bits.push_back(((value>>i)&1)!=0);};
    append(4,4);append(unsigned(data.size()),version<10?8:16);for(char c:data)append(static_cast<unsigned char>(c),8);
    const size_t capacity=size_t(dataCodewords(version))*8;
    append(0,int(std::min<size_t>(4,capacity-bits.size())));append(0,int((8-bits.size()%8)%8));
    for(unsigned pad=0xEC;bits.size()<capacity;pad^=0xEC^0x11)append(pad,8);
    std::vector<unsigned char> codewords(bits.size()/8,0);for(size_t i=0;i<bits.size();++i)if(bits[i])codewords[i>>3]|=static_cast<unsigned char>(1<<(7-(i&7)));
    // Split into blocks, append each block's error correction, then interleave the blocks.
    const int blocks=Blocks[version],ecc=EccPerBlock[version],raw=rawModules(version)/8,shortBlocks=blocks-raw%blocks,shortLength=raw/blocks;
    const auto div=divisor(ecc);std::vector<std::vector<unsigned char>> parts;size_t k=0;
    for(int i=0;i<blocks;++i){
        const size_t length=size_t(shortLength-ecc+(i<shortBlocks?0:1));std::vector<unsigned char> block(codewords.begin()+std::ptrdiff_t(k),codewords.begin()+std::ptrdiff_t(k+length));k+=length;
        const auto check=remainder(block,div);if(i<shortBlocks)block.push_back(0);block.insert(block.end(),check.begin(),check.end());parts.push_back(block);
    }
    std::vector<unsigned char> all;
    for(size_t i=0;i<parts[0].size();++i)for(size_t j=0;j<parts.size();++j)if(i!=size_t(shortLength-ecc)||int(j)>=shortBlocks)all.push_back(parts[j][i]);
    return QrCodeHsx(version,all);
}
QrCodeHsx::QrCodeHsx(int version,const std::vector<unsigned char>& codewords)
    :version_(version),size_(version*4+17),modules_(size_t(size_*size_)),function_(size_t(size_*size_)){
    drawFunctionPatterns();place(codewords);
    long best=-1;
    for(int m=0;m<8;++m){applyMask(m);drawFormat(m);const long score=penalty();if(best<0||score<best){best=score;mask_=m;}applyMask(m);}
    applyMask(mask_);drawFormat(mask_);
}
void QrCodeHsx::set(int x,int y,bool dark,bool function){const auto i=size_t(y*size_+x);modules_[i]=dark;if(function)function_[i]=true;}
void QrCodeHsx::drawFunctionPatterns(){
    for(int i=0;i<size_;++i){set(6,i,i%2==0);set(i,6,i%2==0);}
    auto finder=[&](int cx,int cy){for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx){const int x=cx+dx,y=cy+dy;if(x<0||y<0||x>=size_||y>=size_)continue;const int d=std::max(std::abs(dx),std::abs(dy));set(x,y,d!=2&&d!=4);}};
    finder(3,3);finder(size_-4,3);finder(3,size_-4);
    const auto pos=alignment(version_);const int n=int(pos.size());
    for(int i=0;i<n;++i)for(int j=0;j<n;++j){
        if((i==0&&j==0)||(i==0&&j==n-1)||(i==n-1&&j==0))continue; // these would overlap the finder patterns
        for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)set(pos[size_t(i)]+dx,pos[size_t(j)]+dy,std::max(std::abs(dx),std::abs(dy))!=1);
    }
    drawFormat(0);drawVersion();
}
void QrCodeHsx::drawFormat(int mask){
    const int data=mask; // level M has format bits 00
    int rem=data;for(int i=0;i<10;++i)rem=(rem<<1)^((rem>>9)*0x537);
    const int bits=((data<<10)|rem)^0x5412;auto bit=[bits](int i){return ((bits>>i)&1)!=0;};
    for(int i=0;i<=5;++i)set(8,i,bit(i));
    set(8,7,bit(6));set(8,8,bit(7));set(7,8,bit(8));
    for(int i=9;i<15;++i)set(14-i,8,bit(i));
    for(int i=0;i<8;++i)set(size_-1-i,8,bit(i));
    for(int i=8;i<15;++i)set(8,size_-15+i,bit(i));
    set(8,size_-8,true);
}
void QrCodeHsx::drawVersion(){
    if(version_<7)return;
    int rem=version_;for(int i=0;i<12;++i)rem=(rem<<1)^((rem>>11)*0x1F25);const long bits=(long(version_)<<12)|rem;
    for(int i=0;i<18;++i){const bool b=((bits>>i)&1)!=0;const int a=size_-11+i%3,c=i/3;set(a,c,b);set(c,a,b);}
}
void QrCodeHsx::place(const std::vector<unsigned char>& codewords){
    size_t i=0;const size_t total=codewords.size()*8;
    for(int right=size_-1;right>=1;right-=2){
        if(right==6)right=5; // skip the vertical timing column
        for(int vert=0;vert<size_;++vert)for(int j=0;j<2;++j){
            const int x=right-j;const bool upward=((right+1)&2)==0;const int y=upward?size_-1-vert:vert;const auto at=size_t(y*size_+x);
            if(!function_[at]&&i<total){modules_[at]=((codewords[i>>3]>>(7-int(i&7)))&1)!=0;++i;}
        }
    }
}
void QrCodeHsx::applyMask(int mask){
    for(int y=0;y<size_;++y)for(int x=0;x<size_;++x){
        bool invert=false;
        switch(mask){
        case 0:invert=(x+y)%2==0;break;case 1:invert=y%2==0;break;case 2:invert=x%3==0;break;case 3:invert=(x+y)%3==0;break;
        case 4:invert=(x/3+y/2)%2==0;break;case 5:invert=x*y%2+x*y%3==0;break;case 6:invert=(x*y%2+x*y%3)%2==0;break;default:invert=((x+y)%2+x*y%3)%2==0;
        }
        const auto at=size_t(y*size_+x);if(!function_[at]&&invert)modules_[at]=!modules_[at];
    }
}
long QrCodeHsx::penalty()const{
    long score=0;auto at=[this](int x,int y){return modules_[size_t(y*size_+x)];};
    static const bool before[11]={1,0,1,1,1,0,1,0,0,0,0},after[11]={0,0,0,0,1,0,1,1,1,0,1};
    for(int pass=0;pass<2;++pass)for(int a=0;a<size_;++a){
        auto m=[&](int b){return pass?at(a,b):at(b,a);};
        // Runs of five or more modules of one colour.
        int run=0;bool colour=false;
        for(int b=0;b<size_;++b){const bool c=m(b);if(b>0&&c==colour)++run;else{if(run>=5)score+=3+run-5;colour=c;run=1;}}
        if(run>=5)score+=3+run-5;
        // Patterns that look like a finder pattern.
        for(int b=0;b+11<=size_;++b){bool one=true,two=true;for(int i=0;i<11;++i){one=one&&m(b+i)==before[i];two=two&&m(b+i)==after[i];}if(one)score+=40;if(two)score+=40;}
    }
    for(int y=0;y+1<size_;++y)for(int x=0;x+1<size_;++x){const bool c=at(x,y);if(c==at(x+1,y)&&c==at(x,y+1)&&c==at(x+1,y+1))score+=3;}
    int dark=0;for(const bool m:modules_)dark+=m?1:0;const int total=size_*size_;
    score+=long((std::abs(dark*20-total*10)+total-1)/total-1)*10;
    return score;
}
}
