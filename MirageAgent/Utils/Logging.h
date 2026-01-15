#pragma once

void RemoveOldLog()
{
#ifdef WITH_LOGGING
	char new_dir[600];
	memset(new_dir, 0, sizeof(new_dir));
	sprintf(new_dir, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), LOG_NAME);
	FILE* hFile = fopen(new_dir, xorstr_("rb"));
	if (hFile) { fclose(hFile); DeleteFileA(new_dir); }
#endif
}

void __stdcall LogInFile(std::string log_name, const char* log, ...)
{
#ifdef WITH_LOGGING
	char new_dir[600];
	memset(new_dir, 0, sizeof(new_dir));
	sprintf(new_dir, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), log_name.c_str());
	FILE* hFile = fopen(new_dir, xorstr_("a+"));
	if (hFile)
	{
		time_t t = std::time(0); tm* now = std::localtime(&t);
		char tmp_stamp[600]; memset(tmp_stamp, 0, sizeof(tmp_stamp));
		sprintf(tmp_stamp, xorstr_("[%d:%d:%d]"), now->tm_hour, now->tm_min, now->tm_sec);
		strcat(tmp_stamp, std::string(" " + std::string(log)).c_str());
		va_list arglist; va_start(arglist, log); vfprintf(hFile, tmp_stamp, arglist);
		va_end(arglist); fclose(hFile);
	}
#endif
}
