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
}

static std::string NormalizeDumpSlashes(std::string value)
{
	for (char& ch : value)
	{
		if (ch == '/') ch = '\\';
	}
	return value;
}

static std::string TrimDumpPath(std::string value)
{
	value = NormalizeDumpSlashes(std::move(value));
	while (!value.empty() && (value.front() == '@' || value.front() == '\\'))
	{
		value.erase(value.begin());
	}
	if (value.size() > 2 && value[1] == ':')
	{
		value.erase(0, 2);
		while (!value.empty() && value.front() == '\\')
		{
			value.erase(value.begin());
		}
	}
	return value;
}

static std::string SanitizeDumpPart(const std::string& part)
{
	if (part.empty()) return {};
	if (part == "." || part == "..") return xorstr_("_");

	std::string out;
	out.reserve(part.size());
	for (unsigned char ch : part)
	{
		switch (ch)
		{
		case '<':
		case '>':
		case ':':
		case '"':
		case '|':
		case '?':
		case '*':
			out.push_back('_');
			break;
		default:
			out.push_back(ch < 32 ? '_' : static_cast<char>(ch));
			break;
		}
	}

	if (out.empty()) return xorstr_("_");
	return out;
}

static std::string JoinDumpParts(const std::vector<std::string>& parts)
{
	std::string out;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (parts[i].empty()) continue;
		if (!out.empty()) out.push_back('\\');
		out.append(parts[i]);
	}
	return out;
}

static std::string BuildDumpRelativePath(const std::string& name, const std::string& fallback_name = {})
{
	std::string value = TrimDumpPath(name);
	if (value.empty()) value = TrimDumpPath(fallback_name);
	if (value.empty()) value = xorstr_("unknown\\chunk.lua");

	std::vector<std::string> parts;
	size_t start = 0;
	while (start <= value.size())
	{
		size_t pos = value.find('\\', start);
		std::string token = pos == std::string::npos ? value.substr(start) : value.substr(start, pos - start);
		token = SanitizeDumpPart(token);
		if (!token.empty()) parts.push_back(token);
		if (pos == std::string::npos) break;
		start = pos + 1;
	}

	std::string result = JoinDumpParts(parts);
	if (result.empty()) return xorstr_("unknown\\chunk.lua");
	return result;
}

static std::string DumpExtensionFromName(const std::string& name)
{
	std::string normalized = NormalizeDumpSlashes(name);
	size_t pos = normalized.find_last_of('.');
	if (pos == std::string::npos) return xorstr_(".lua");
	std::string ext = normalized.substr(pos);
	if (ext == xorstr_(".luac")) return ext;
	if (ext == xorstr_(".lua")) return ext;
	return xorstr_(".lua");
}

static std::filesystem::path BuildDumpRoot(const std::string& dir_name)
{
	std::filesystem::path root(lua_scripts_dir);
	root /= dir_name;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	return root;
}

static std::string WriteDumpFile(const std::string& dir_name, const std::string& relative_path, char* buff, size_t sz)
{
	if (!buff || sz == 0) return {};

	std::filesystem::path root = BuildDumpRoot(dir_name);
	std::filesystem::path relative(relative_path);
	std::filesystem::path full_path = root / relative;

	std::error_code ec;
	if (full_path.has_parent_path())
		std::filesystem::create_directories(full_path.parent_path(), ec);

	FILE* hFile = fopen(full_path.string().c_str(), xorstr_("wb"));
	if (hFile != nullptr)
	{
		fwrite(buff, 1, sz, hFile);
		fclose(hFile);
		return full_path.string();
	}

	LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t dump file: %s\n"), full_path.string().c_str());
	return {};
}

std::vector<std::vector<char>> dumpedChunks;
std::unordered_set<uint32_t> dumpedChunkChecksums;

uint32_t CalculateChecksum(const char* buff, size_t sz)
{
	uint32_t checksum = 0;
	for (size_t i = 0; i < sz; ++i)
	{
		checksum = checksum * 31 + static_cast<unsigned char>(buff[i]);
	}
	return checksum;
}

void RemoveOldDumpedScripts(std::string dir_name)
{
	std::filesystem::path root(lua_scripts_dir);
	root /= dir_name;
	std::error_code ec;
	if (std::filesystem::exists(root, ec))
		std::filesystem::remove_all(root, ec);
	std::filesystem::create_directories(root, ec);
}

std::string DumpScriptEx(std::string dir_name, std::string name, char* buff, size_t sz)
{
	const std::string relative_path = BuildDumpRelativePath(name, xorstr_("unknown\\chunk.lua"));
	return WriteDumpFile(dir_name, relative_path, buff, sz);
}

void DumpScript(std::string dir_name, std::string name, char* buff, size_t sz)
{
	DumpScriptEx(std::move(dir_name), std::move(name), buff, sz);
}

static std::string BuildCachedDumpRelativePath(const std::string& name, uint32_t checksum)
{
	const std::string normalized = BuildDumpRelativePath(name);
	size_t leaf_pos = normalized.find_last_of('\\');
	std::string leaf = leaf_pos == std::string::npos ? normalized : normalized.substr(leaf_pos + 1);
	if (!normalized.empty() &&
		!findStringIC(leaf, xorstr_("chunk.lua")) &&
		!findStringIC(leaf, xorstr_("chunk.luac")))
	{
		return normalized;
	}

	char hash_buf[32]{};
	sprintf(hash_buf, xorstr_("chunk_%08X"), checksum);
	return std::string(xorstr_("cache\\")) + hash_buf + DumpExtensionFromName(name);
}

std::string DumpIfNotDuplicateEx(const char* path, const char* name, char* buff, size_t sz)
{
	if (!buff || sz == 0) return {};

	uint32_t currentChecksum = CalculateChecksum(buff, sz);
	if (dumpedChunkChecksums.find(currentChecksum) != dumpedChunkChecksums.end())
	{
		return {};
	}

	dumpedChunkChecksums.insert(currentChecksum);
	dumpedChunks.emplace_back(buff, buff + sz);

	const std::string relative_path = BuildCachedDumpRelativePath(name ? name : xorstr_("chunk.lua"), currentChecksum);
	return WriteDumpFile(path ? path : xorstr_("Chunks"), relative_path, buff, sz);
}

void DumpIfNotDuplicate(const char* path, const char* name, char* buff, size_t sz)
{
	DumpIfNotDuplicateEx(path, name, buff, sz);
}
