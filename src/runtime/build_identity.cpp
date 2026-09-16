#include "runtime/build_identity.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <vector>

namespace cojvr::runtime {
namespace {

class AlgorithmHandle {
public:
    ~AlgorithmHandle() {
        if (handle_) BCryptCloseAlgorithmProvider(handle_, 0);
    }
    BCRYPT_ALG_HANDLE* put() noexcept { return &handle_; }
    BCRYPT_ALG_HANDLE get() const noexcept { return handle_; }
private:
    BCRYPT_ALG_HANDLE handle_ = nullptr;
};

class HashHandle {
public:
    ~HashHandle() {
        if (handle_) BCryptDestroyHash(handle_);
    }
    BCRYPT_HASH_HANDLE* put() noexcept { return &handle_; }
    BCRYPT_HASH_HANDLE get() const noexcept { return handle_; }
private:
    BCRYPT_HASH_HANDLE handle_ = nullptr;
};

std::string ToHex(const std::array<unsigned char, 32>& digest) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string result;
    result.resize(digest.size() * 2);
    for (std::size_t i = 0; i < digest.size(); ++i) {
        result[i * 2] = kHex[digest[i] >> 4];
        result[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    return result;
}

} // namespace

std::optional<std::string> Sha256File(const std::filesystem::path& path) noexcept {
    try {
        AlgorithmHandle algorithm;
        if (BCryptOpenAlgorithmProvider(algorithm.put(), BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
            return std::nullopt;
        }

        DWORD object_size = 0;
        DWORD bytes = 0;
        if (BCryptGetProperty(algorithm.get(), BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &bytes, 0) < 0) {
            return std::nullopt;
        }

        std::vector<unsigned char> object(object_size);
        HashHandle hash;
        if (BCryptCreateHash(algorithm.get(), hash.put(), object.data(), object_size, nullptr, 0, 0) < 0) {
            return std::nullopt;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) return std::nullopt;

        std::vector<char> buffer(64 * 1024);
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = input.gcount();
            if (count > 0 &&
                BCryptHashData(hash.get(), reinterpret_cast<PUCHAR>(buffer.data()),
                               static_cast<ULONG>(count), 0) < 0) {
                return std::nullopt;
            }
        }
        if (!input.eof()) return std::nullopt;

        std::array<unsigned char, 32> digest{};
        if (BCryptFinishHash(hash.get(), digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
            return std::nullopt;
        }
        return ToHex(digest);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace cojvr::runtime
