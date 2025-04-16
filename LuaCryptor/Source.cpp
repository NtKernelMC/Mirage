#include <Windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// ���������� ��� HEX ����� ��� XOR ����������/������������
constexpr unsigned char XOR_KEY1 = 0x12;
constexpr unsigned char XOR_KEY2 = 0x34;
constexpr unsigned char XOR_KEY3 = 0x56;

// ������� ��� ���������� ����� .lua � �������������� XOR ���������
bool EncryptLuaFile(const char* filePath)
{
    // ��������� ���� ��� ������ � �������� ������
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile)
    {
        std::cerr << "������: �� ������� ������� ���� ��� ����������: " << filePath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string fileContent = buffer.str();
    inFile.close();

    // ��������� XOR ���������� � ����� �������
    for (size_t i = 0; i < fileContent.size(); ++i)
    {
        fileContent[i] ^= XOR_KEY1;
        fileContent[i] ^= XOR_KEY2;
        fileContent[i] ^= XOR_KEY3;
    }

    // ��������� ��� �� ���� ��� ������ � �������� ������ � �������������� ������������� ����������
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile)
    {
        std::cerr << "������: �� ������� ������� ���� ��� ������: " << filePath << std::endl;
        return false;
    }
    outFile.write(fileContent.data(), fileContent.size());
    outFile.close();

    std::cout << "���� ������� ����������: " << filePath << std::endl;
    return true;
}

int main(int argc, char* argv[])
{
    SetConsoleTitleA("Mirage Cryptor by DroidZero");
    setlocale(LC_ALL, "Russian");
    system("color 09");
    // ���������, ������� �� ���� � �����
    if (argc < 2)
    {
        std::cout << "�������������: EncryptLua <���� � ����� .lua>" << std::endl;
        return 1;
    }

    const char* filePath = argv[1];

    // �������� ������� ����������
    if (!EncryptLuaFile(filePath))
    {
        std::cerr << "��������� ������ ��� ���������� �����." << std::endl;
        return 1;
    }
    while (true) Sleep(1000);
    return 0;
}
