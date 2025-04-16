// Определяем три HEX ключа для XOR-шифрования/дешифрования
constexpr unsigned char XOR_KEY1 = 0x12;
constexpr unsigned char XOR_KEY2 = 0x34;
constexpr unsigned char XOR_KEY3 = 0x56;

// Функция для дешифрования буфера (так как XOR симметричен, та же логика применима и для шифрования)
std::string DecryptBuffer(const std::string& input)
{
    std::string output = input;
    for (size_t i = 0; i < output.size(); ++i)
    {
        output[i] ^= XOR_KEY1;
        output[i] ^= XOR_KEY2;
        output[i] ^= XOR_KEY3;
    }
    return output;
}

// Функция для шифрования (будет использоваться сторонним .exe для защиты lua скриптов)
bool EncryptLuaFile(const char* filePath)
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

    // Шифрование посредством XOR с тремя ключами
    for (size_t i = 0; i < fileContent.size(); ++i)
    {
        fileContent[i] ^= XOR_KEY1;
        fileContent[i] ^= XOR_KEY2;
        fileContent[i] ^= XOR_KEY3;
    }

    // Перезаписываем тот же файл зашифрованным содержимым
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't open file for writing: %s\n"), filePath);
        return false;
    }
    outFile.write(fileContent.data(), fileContent.size());
    outFile.close();
    return true;
}