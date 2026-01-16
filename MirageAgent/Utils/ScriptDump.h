#pragma once

bool IsFileExist(std::string file_name)
{
	FILE* hFile = fopen(file_name.c_str(), "rb");
	if (hFile != nullptr)
	{
		fclose(hFile);
		return true;
	}
	return false;
}

bool __stdcall IsDirectoryExists(const std::string& dirName_in)
{
	DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
	if (ftyp == INVALID_FILE_ATTRIBUTES) return false;
	if (ftyp & FILE_ATTRIBUTE_DIRECTORY) return true;
	return false;
};

std::string CleanScriptName(std::string str)
{
	std::string ans = str + xorstr_("\\");;
	std::string delimiter = xorstr_("\\");
	size_t pos = 0; std::string token;
	while ((pos = ans.find(delimiter)) != std::string::npos)
	{
		token = ans.substr(0, pos);
		ans.erase(0, pos + delimiter.length());
	}
	return token;
}

std::vector<std::vector<char>> dumpedChunks;
std::unordered_set<uint32_t> dumpedChunkChecksums;
// Simple checksum helper.
uint32_t CalculateChecksum(const char* buff, size_t sz) {
	uint32_t checksum = 0;
	for (size_t i = 0; i < sz; ++i) {
		checksum = checksum * 31 + static_cast<unsigned char>(buff[i]);
	}
	return checksum;
}

void RemoveOldDumpedScripts(std::string dir_name)
{
	char cwd[500]; memset(cwd, 0, sizeof(cwd));
	sprintf(cwd, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), dir_name.c_str());
	if (!IsDirectoryExists(cwd)) CreateDirectoryA(cwd, 0);
	else
	{
		std::filesystem::remove_all(cwd);
		CreateDirectoryA(cwd, 0);
	}
}

static int g_dump_script_ctr = 1;

static std::string BuildDumpPath(const std::string& dir_name, const std::string& name, int ctr)
{
	char ctr_buf[10]; memset(ctr_buf, 0, sizeof(ctr_buf));
	sprintf(ctr_buf, xorstr_("a%d_"), ctr);
	std::string clean_name = CleanScriptName(name);
	char cwd[500]; memset(cwd, 0, sizeof(cwd));
	sprintf(cwd, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), dir_name.c_str());
	return cwd + std::string(xorstr_("\\")) + (ctr_buf + clean_name);
}

std::string DumpScriptEx(std::string dir_name, std::string name, char* buff, size_t sz)
{
	int ctr = g_dump_script_ctr++;
	std::string script_path = BuildDumpPath(dir_name, name, ctr);
	FILE* hFile = fopen(script_path.c_str(), xorstr_("wb"));
	if (hFile != nullptr)
	{
		fwrite(buff, 1, sz, hFile);
		fclose(hFile);
	}
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t dump file: %s\n"), script_path.c_str());
	return script_path;
}

void DumpScript(std::string dir_name, std::string name, char* buff, size_t sz)
{
	DumpScriptEx(dir_name, name, buff, sz);
}

// Skip dumping chunks with a checksum that was already seen.
std::string DumpIfNotDuplicateEx(const char* path, const char* name, char* buff, size_t sz) {
	uint32_t currentChecksum = CalculateChecksum(buff, sz);

	// If the checksum already exists, skip the dump.
	if (dumpedChunkChecksums.find(currentChecksum) != dumpedChunkChecksums.end()) {
		return ""; // Chunk already exists.
	}

	// Store the checksum to avoid duplicates.
	dumpedChunkChecksums.insert(currentChecksum);

	// Store the chunk data.
	std::vector<char> chunk(buff, buff + sz);
	dumpedChunks.push_back(chunk);

	// Call the original dump function.
	return DumpScriptEx(path, name, buff, sz);
}

// Skip dumping chunks with a checksum that was already seen.
void DumpIfNotDuplicate(const char* path, const char* name, char* buff, size_t sz) {
	DumpIfNotDuplicateEx(path, name, buff, sz);
}
