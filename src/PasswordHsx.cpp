#include "PasswordHsx.h"
#include "TypesHsx.h"
#include "ScopeHsx.h"
#include <array>
#ifdef IM_QT_CRYPTO
#include <QPasswordDigestor>
#include <QCryptographicHash>
#include <QRandomGenerator>
#endif
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif
namespace im {
namespace {
// Digest format: pbkdf2-sha256:<iterations>:<32 hex salt>:<64 hex hash>. Both supported costs
// have six digits, so the salt and hash offsets are identical for old and new digests.
constexpr unsigned long CurrentIterations = 600000, LegacyIterations = 100000;
std::string prefix(unsigned long iterations) { return "pbkdf2-sha256:" + std::to_string(iterations) + ":"; }
constexpr size_t PrefixSize = sizeof("pbkdf2-sha256:600000:") - 1;
unsigned long iterations(const std::string& encoded) {
    for (const auto count : {CurrentIterations, LegacyIterations}) if (encoded.rfind(prefix(count), 0) == 0) return count;
    return 0;
}
std::string hex(const unsigned char* bytes, size_t count) {
    const char* digits = "0123456789abcdef"; std::string result;
    for (size_t i = 0; i < count; ++i) { result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15]; }
    return result;
}
unsigned char nibble(char c) { return static_cast<unsigned char>(c <= '9' ? c - '0' : c - 'a' + 10); }
std::string derive(const std::string& password, std::array<unsigned char, 16> salt, unsigned long rounds) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm{};
    require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) >= 0, "密码算法初始化失败");
    ScopeHsx close([&] { BCryptCloseAlgorithmProvider(algorithm, 0); });
    std::array<unsigned char, 32> result{};
    require(BCryptDeriveKeyPBKDF2(algorithm,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())), static_cast<ULONG>(password.size()),
        salt.data(), static_cast<ULONG>(salt.size()), rounds, result.data(), static_cast<ULONG>(result.size()), 0) >= 0,
        "密码摘要计算失败");
    return hex(result.data(), result.size());
#elif defined(IM_QT_CRYPTO)
    const auto bytes=QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,QByteArray::fromStdString(password),QByteArray(reinterpret_cast<const char*>(salt.data()),16),static_cast<quint64>(rounds),32);
    require(bytes.size()==32,"密码摘要计算失败");return bytes.toHex().toStdString();
#else
    (void)password; (void)salt; (void)rounds;
    throw std::runtime_error("管理员认证需要 Windows CNG 运行环境");
#endif
}
}
bool PasswordHsx::valid(const std::string& encoded) {
    if (!iterations(encoded) || encoded.size() != PrefixSize + 32 + 1 + 64 || encoded[PrefixSize+32] != ':') return false;
    for (size_t i = PrefixSize; i < encoded.size(); ++i) {
        if (i == PrefixSize+32) continue;
        const char c = encoded[i]; if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) return false;
    }
    return true;
}
bool PasswordHsx::outdated(const std::string& encoded) { return valid(encoded) && iterations(encoded) < CurrentIterations; }
void PasswordHsx::check(const std::string& password, size_t minimum, const std::string& field) {
    checkText(password, field, false, 128);
    require(password.size() >= minimum, field + "至少" + std::to_string(minimum) + "个字节");
}
std::string PasswordHsx::hash(const std::string& password, size_t minimum, const std::string& field) {
    check(password, minimum, field);
    std::array<unsigned char, 16> salt{};
#ifdef _WIN32
    require(BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0,
        "密码随机盐生成失败");
#elif defined(IM_QT_CRYPTO)
    for(auto& byte:salt)byte=static_cast<unsigned char>(QRandomGenerator::system()->generate());
#else
    throw std::runtime_error("Secure random provider unavailable");
#endif
    return prefix(CurrentIterations) + hex(salt.data(), salt.size()) + ':' + derive(password, salt, CurrentIterations);
}
bool PasswordHsx::verify(const std::string& password, const std::string& encoded) {
    if (!valid(encoded) || password.empty() || password.size() > 128) return false;
    std::array<unsigned char, 16> salt{};
    for (size_t i = 0; i < salt.size(); ++i) salt[i] = static_cast<unsigned char>(nibble(encoded[PrefixSize+2*i])*16 + nibble(encoded[PrefixSize+2*i+1]));
    const auto actual = derive(password, salt, iterations(encoded)), expected = encoded.substr(PrefixSize+33);
    unsigned difference = 0; for (size_t i = 0; i < expected.size(); ++i) difference |= static_cast<unsigned>(actual[i] ^ expected[i]);
    return difference == 0;
}
}
