// Mirage Cryptor / Decryptor  —  goto‑free edition
// ------------------------------------------------
// • drag‑and‑drop *.lua  →  AES‑decrypt in‑place
// • ключи взяты из .rdata (pbData, IV)
// • опция  /enc  — доп. тройной XOR обратно

#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <wincrypt.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

// ────────────────────────────────────────────────
// 0. Константы из .rdata
static const BYTE g_PbData[32] = {
    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
    0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10,
    0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
    0x24,0x68,0xAC,0xE0,0x13,0x57,0x9B,0xDF
};
static const BYTE g_IV[16] = {
    0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
    0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00
};

// ────────────────────────────────────────────────
// 1. RAII‑обёртка для крипто‑хендлов
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
    bool ok() const { return prov && hash && key; }
};

// ────────────────────────────────────────────────
// 2. AES‑CBC дешифр без goto
bool DecryptAesBuffer(BYTE* buf, DWORD& len)
{
    if (!buf || !len) return false;

    CryptoHandle h;

    if (!CryptAcquireContextA(&h.prov, nullptr, nullptr,
        PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return false;

    if (!CryptCreateHash(h.prov, CALG_SHA_256, 0, 0, &h.hash))
        return false;

    if (!CryptHashData(h.hash, g_PbData, sizeof(g_PbData), 0))
        return false;

    if (!CryptDeriveKey(h.prov, CALG_AES_256, h.hash,
        CRYPT_EXPORTABLE, &h.key))
        return false;

    if (!CryptSetKeyParam(h.key, KP_IV, g_IV, 0))
        return false;

    DWORD mode = CRYPT_MODE_CBC;
    if (!CryptSetKeyParam(h.key, KP_MODE,
        reinterpret_cast<BYTE*>(&mode), 0))
        return false;

    if (!CryptDecrypt(h.key, 0, TRUE, 0, buf, &len))
        return false;

    return true;            // RAII‑деструктор почистит всё сам
}

// ────────────────────────────────────────────────
// 3. Тройной XOR‑encrypt (как раньше)
constexpr unsigned char XOR_KEY1 = 0x22;
constexpr unsigned char XOR_KEY2 = 0x69;
constexpr unsigned char XOR_KEY3 = 0x95;

bool XorEncryptFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) { std::cerr << "[-] can't open " << path << '\n'; return false; }
    std::vector<char> data((std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    in.close();

    for (auto& c : data)
    {
        c ^= XOR_KEY1;
        c ^= XOR_KEY2;
        c ^= XOR_KEY3;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) { std::cerr << "[-] can't write " << path << '\n'; return false; }
    out.write(data.data(), data.size());
    std::cout << "[+] XOR‑encrypted: " << path << '\n';
    return true;
}

// ────────────────────────────────────────────────
// 4. Main
int main(int argc, char* argv[])
{
    SetConsoleTitleA("Mirage Cryptor by DroidZero");
    setlocale(LC_ALL, "Russian");
    system("color 09");

    if (argc < 2)
    {
        std::cout << "Перетащи .lua на exe или запусти так:\n"
            << "  decrypt.exe <file.lua> [/enc]\n";
        return 0;
    }

    const std::string luaPath = argv[1];
    const bool doXorAgain = (argc >= 3 && _stricmp(argv[2], "/enc") == 0);

    // читаем файл
    std::ifstream fin(luaPath, std::ios::binary);
    if (!fin) { std::cerr << "[-] read fail: " << luaPath << '\n'; return 1; }
    std::vector<BYTE> blob((std::istreambuf_iterator<char>(fin)),
        std::istreambuf_iterator<char>());
    fin.close();

    DWORD sz = static_cast<DWORD>(blob.size());
    std::cout << "[*] size before decrypt: " << sz << " bytes\n";

    if (!DecryptAesBuffer(blob.data(), sz))
    {
        std::cerr << "[-] AES decrypt failed. GLE = 0x"
            << std::hex << GetLastError() << std::dec << '\n';
        return 1;
    }
    blob.resize(sz);
    std::cout << "[+] decrypt ok, new size: " << sz << '\n';

    // перезаписываем файл
    std::ofstream fout(luaPath, std::ios::binary | std::ios::trunc);
    if (!fout) { std::cerr << "[-] write fail: " << luaPath << '\n'; return 1; }
    fout.write(reinterpret_cast<char*>(blob.data()), blob.size());
    std::cout << "[✓] plaintext written.\n";

    if (doXorAgain)
        XorEncryptFile(luaPath);

    std::cout << "Done. Press any key...\n";
    std::cin.get();
    return 0;
}
