#pragma once

#include <Windows.h>
#include <wincrypt.h>
#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "xorstr.h"

#pragma comment(lib, "advapi32.lib")

void __stdcall LogInFile(std::string log_name, const char* log, ...);

namespace
{
constexpr BYTE kHeaderMagic[8] = { 'M', 'G', 'A', 'E', 'S', '0', '2', '!' };
constexpr BYTE kPayloadMagic[8] = { 'M', 'G', 'L', 'U', 'A', '0', '1', '!' };
constexpr size_t kHeaderMagicSize = sizeof(kHeaderMagic);
constexpr size_t kPayloadMagicSize = sizeof(kPayloadMagic);
constexpr size_t kIvSize = 16;
constexpr size_t kHeaderSize = kHeaderMagicSize + kIvSize;

inline BYTE rotl8(BYTE value, unsigned int shift)
{
	return static_cast<BYTE>((value << shift) | (value >> (8 - shift)));
}

inline void BuildKeyMaterial(std::array<BYTE, 32>& out)
{
	static const BYTE kSeedA[32] = {
		0xA1,0x2F,0x9C,0x44,0x5E,0xD7,0x13,0xB8,
		0x6A,0xF1,0x0D,0xC2,0x7E,0x39,0x84,0x5B,
		0xD0,0x22,0x9F,0x68,0xB3,0x4C,0x17,0xE5,
		0x7A,0x90,0x3D,0xC9,0x56,0x1B,0xF4,0x8E
	};
	static const BYTE kSeedB[32] = {
		0x3C,0xE9,0x5A,0x11,0xB7,0x24,0x8D,0xF0,
		0x6E,0x02,0x9B,0xD4,0x77,0x1A,0xC6,0x58,
		0xAF,0x30,0xE2,0x4D,0x95,0x7F,0x0C,0xB1,
		0x63,0x28,0xD9,0x05,0xBE,0x4A,0xF8,0x12
	};
	static const BYTE kSalt1[] = "Mirage::Lua::AES::v2";

	uint32_t mix = 0xC7F4A7C3u;
	for (size_t i = 0; i < 32; ++i)
	{
		mix ^= static_cast<uint32_t>(kSeedA[i]) << (i % 8);
		mix *= 0x45D9F3Bu;
		mix ^= (mix >> 16);
	}

	for (size_t i = 0; i < 32; ++i)
	{
		BYTE b = static_cast<BYTE>(kSeedA[i] ^ kSeedB[(i * 5 + 7) & 31]);
		b = rotl8(b, static_cast<unsigned int>((i % 7) + 1));
		b ^= static_cast<BYTE>(kSalt1[i % (sizeof(kSalt1) - 1)]);
		b ^= static_cast<BYTE>((mix >> (i % 24)) & 0xFF);
		b = static_cast<BYTE>(b + (i * 11u) + (mix & 0x1Fu));
		out[i] = b;
	}
}

struct CryptoHandle
{
	HCRYPTPROV prov{ 0 };
	HCRYPTHASH hash{ 0 };
	HCRYPTKEY  key{ 0 };

	~CryptoHandle()
	{
		if (key)  CryptDestroyKey(key);
		if (hash) CryptDestroyHash(hash);
		if (prov) CryptReleaseContext(prov, 0);
	}
};

inline bool InitAesKey(CryptoHandle& h, const BYTE* iv)
{
	if (!CryptCreateHash(h.prov, CALG_SHA_256, 0, 0, &h.hash))
		return false;

	std::array<BYTE, 32> material{};
	BuildKeyMaterial(material);

	static const BYTE kSalt2[] = "Mirage/AES/KeyDerive/2026";
	if (!CryptHashData(h.hash, material.data(), static_cast<DWORD>(material.size()), 0))
		return false;
	if (!CryptHashData(h.hash, kSalt2, static_cast<DWORD>(sizeof(kSalt2) - 1), 0))
		return false;
	if (!CryptDeriveKey(h.prov, CALG_AES_256, h.hash, CRYPT_EXPORTABLE, &h.key))
		return false;

	DWORD mode = CRYPT_MODE_CBC;
	if (!CryptSetKeyParam(h.key, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
		return false;
	if (!CryptSetKeyParam(h.key, KP_IV, iv, 0))
		return false;

	return true;
}

inline bool EncryptAesBuffer(const BYTE* input, size_t input_len, std::vector<BYTE>& out)
{
	if (!input || input_len == 0 || input_len > MAXDWORD)
		return false;

	CryptoHandle h;
	if (!CryptAcquireContextA(&h.prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
		return false;

	BYTE iv[kIvSize] = {};
	if (!CryptGenRandom(h.prov, kIvSize, iv))
		return false;

	if (!InitAesKey(h, iv))
		return false;

	const DWORD blockSize = 16;
	DWORD plain_len = static_cast<DWORD>(input_len + kPayloadMagicSize);
	DWORD buf_capacity = plain_len + blockSize;
	std::vector<BYTE> buffer(buf_capacity);
	memcpy(buffer.data(), kPayloadMagic, kPayloadMagicSize);
	memcpy(buffer.data() + kPayloadMagicSize, input, input_len);

	if (!CryptEncrypt(h.key, 0, TRUE, 0, buffer.data(), &plain_len, buf_capacity))
		return false;

	out.resize(kHeaderSize + plain_len);
	memcpy(out.data(), kHeaderMagic, kHeaderMagicSize);
	memcpy(out.data() + kHeaderMagicSize, iv, kIvSize);
	memcpy(out.data() + kHeaderSize, buffer.data(), plain_len);
	return true;
}
} // namespace

inline bool IsEncryptedScript(const std::string& input)
{
	return (input.size() >= kHeaderSize) &&
		(std::memcmp(input.data(), kHeaderMagic, kHeaderMagicSize) == 0);
}

inline std::string DecryptBuffer(const std::string& input, bool* out_decrypted = nullptr)
{
	if (out_decrypted) *out_decrypted = false;
	if (!IsEncryptedScript(input))
		return input;

	const BYTE* iv = reinterpret_cast<const BYTE*>(input.data() + kHeaderMagicSize);
	size_t cipher_len = input.size() - kHeaderSize;
	if (cipher_len == 0 || cipher_len > MAXDWORD)
		return input;

	std::vector<BYTE> buffer(cipher_len);
	memcpy(buffer.data(), input.data() + kHeaderSize, cipher_len);

	CryptoHandle h;
	if (!CryptAcquireContextA(&h.prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
		return input;
	if (!InitAesKey(h, iv))
		return input;

	DWORD len = static_cast<DWORD>(cipher_len);
	if (!CryptDecrypt(h.key, 0, TRUE, 0, buffer.data(), &len))
		return input;
	if (len < kPayloadMagicSize)
		return input;
	if (std::memcmp(buffer.data(), kPayloadMagic, kPayloadMagicSize) != 0)
		return input;

	std::string output;
	output.assign(reinterpret_cast<const char*>(buffer.data() + kPayloadMagicSize), len - kPayloadMagicSize);
	if (out_decrypted) *out_decrypted = true;
	return output;
}

inline bool EncryptLuaFile(const char* filePath)
{
	std::ifstream inFile(filePath, std::ios::binary);
	if (!inFile)
	{
		LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't open file for encryption: %s\n"), filePath);
		return false;
	}
	std::stringstream buffer;
	buffer << inFile.rdbuf();
	std::string fileContent = buffer.str();
	inFile.close();

	if (IsEncryptedScript(fileContent))
	{
		LogInFile(LOG_NAME, xorstr_("[LOG] Skipping encryption, already encrypted: %s\n"), filePath);
		return true;
	}

	std::vector<BYTE> encrypted;
	if (!EncryptAesBuffer(reinterpret_cast<const BYTE*>(fileContent.data()), fileContent.size(), encrypted))
	{
		LogInFile(LOG_NAME, xorstr_("[LOG] Error: encrypt failed: %s\n"), filePath);
		return false;
	}

	std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
	if (!outFile)
	{
		LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't open file for writing: %s\n"), filePath);
		return false;
	}
	outFile.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
	outFile.close();
	return true;
}
