// ���������� ��� HEX ����� ��� XOR-����������/������������
constexpr unsigned char XOR_KEY1 = 0x12;
constexpr unsigned char XOR_KEY2 = 0x34;
constexpr unsigned char XOR_KEY3 = 0x56;

// ������� ��� ������������ ������ (��� ��� XOR �����������, �� �� ������ ��������� � ��� ����������)
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

// ������� ��� ���������� (����� �������������� ��������� .exe ��� ������ lua ��������)
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

    // ���������� ����������� XOR � ����� �������
    for (size_t i = 0; i < fileContent.size(); ++i)
    {
        fileContent[i] ^= XOR_KEY1;
        fileContent[i] ^= XOR_KEY2;
        fileContent[i] ^= XOR_KEY3;
    }

    // �������������� ��� �� ���� ������������� ����������
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