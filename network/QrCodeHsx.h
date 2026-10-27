#pragma once
#include <QByteArray>
#include <vector>
namespace im::net {
// Minimal QR Code encoder (ISO/IEC 18004): byte mode, error correction level M, versions 1–10,
// so at most 213 bytes. Enough for sign-in codes; no third-party library is needed.
class QrCodeHsx {
public:
    static QrCodeHsx encode(const QByteArray& data); // throws std::runtime_error when the data is too long
    int size() const { return size_; }
    int version() const { return version_; }
    int mask() const { return mask_; }
    bool dark(int x,int y) const { return modules_[size_t(y*size_+x)]; }
private:
    QrCodeHsx(int version,const std::vector<unsigned char>& codewords);
    void set(int x,int y,bool dark,bool function=true);
    void drawFunctionPatterns();
    void drawFormat(int mask);
    void drawVersion();
    void place(const std::vector<unsigned char>& codewords);
    void applyMask(int mask);
    long penalty() const;
    int version_=1,size_=21,mask_=0;
    std::vector<bool> modules_,function_;
};
}
