#pragma once

bool RemoveDirRecursive(const std::wstring& dir)
{
	namespace fs = std::filesystem;

	std::error_code ec;
	auto removed = fs::remove_all(fs::path(dir), ec);

	if (ec) return false;

	return true;
}

static std::string kDumpDir{ xorstr_("DumpedHeap") };
// Create the directory once.
static void EnsureDumpDirectory()
{
	static std::once_flag flag;
	std::call_once(flag, []()
		{
			kDumpDir = CvWideToAnsi(mapped_image_dir) + xorstr_("\\DumpedHeap");
			std::filesystem::create_directories(kDumpDir);
		});
}

static std::string RandomString(std::size_t len)
{
	static const char charset[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789";
	static thread_local std::mt19937 gen{ std::random_device{}() };
	std::uniform_int_distribution<>  dist(0, sizeof(charset) - 2);

	std::string out;
	out.reserve(len);
	while (len--)
		out.push_back(charset[dist(gen)]);
	return out;
}
