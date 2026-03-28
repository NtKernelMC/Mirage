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
		char tmp_stamp[64]; memset(tmp_stamp, 0, sizeof(tmp_stamp));
		sprintf(tmp_stamp, xorstr_("[%d:%d:%d]"), now->tm_hour, now->tm_min, now->tm_sec);
		char msg_buf[8192]; memset(msg_buf, 0, sizeof(msg_buf));
		va_list arglist; va_start(arglist, log);
		vsnprintf(msg_buf, sizeof(msg_buf) - 1, log ? log : "", arglist);
		va_end(arglist);
		fprintf(hFile, "%s %s", tmp_stamp, msg_buf);
		fclose(hFile);
	}
#endif
}
