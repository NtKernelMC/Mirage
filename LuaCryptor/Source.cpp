#include <Windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// Определяем три HEX ключа для XOR шифрования/дешифрования
constexpr unsigned char XOR_KEY1 = 0x12;
constexpr unsigned char XOR_KEY2 = 0x34;
constexpr unsigned char XOR_KEY3 = 0x56;

// Функция для шифрования файла .lua с использованием XOR алгоритма
bool EncryptLuaFile(const char* filePath)
{
    // Открываем файл для чтения в бинарном режиме
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Ошибка: не удалось открыть файл для шифрования: " << filePath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string fileContent = buffer.str();
    inFile.close();

    // Применяем XOR шифрование с тремя ключами
    for (size_t i = 0; i < fileContent.size(); ++i)
    {
        fileContent[i] ^= XOR_KEY1;
        fileContent[i] ^= XOR_KEY2;
        fileContent[i] ^= XOR_KEY3;
    }

    // Открываем тот же файл для записи в бинарном режиме и перезаписываем зашифрованное содержимое
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile)
    {
        std::cerr << "Ошибка: не удалось открыть файл для записи: " << filePath << std::endl;
        return false;
    }
    outFile.write(fileContent.data(), fileContent.size());
    outFile.close();

    std::cout << "Файл успешно зашифрован: " << filePath << std::endl;
    return true;
}

int main(int argc, char* argv[])
{
    SetConsoleTitleA("Mirage Cryptor by DroidZero");
    setlocale(LC_ALL, "Russian");
    system("color 09");
    // Проверяем, передан ли путь к файлу
    if (argc < 2)
    {
        std::cout << "Использование: EncryptLua <путь к файлу .lua>" << std::endl;
        return 1;
    }

    const char* filePath = argv[1];

    // Вызываем функцию шифрования
    if (!EncryptLuaFile(filePath))
    {
        std::cerr << "Произошла ошибка при шифровании файла." << std::endl;
        return 1;
    }
    while (true) Sleep(1000);
    return 0;
}
