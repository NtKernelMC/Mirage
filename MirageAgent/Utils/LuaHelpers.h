#pragma once

typedef struct LoadS {
	char* s;
	size_t size;
} LoadS;
typedef const char* (*lua_Reader) (void* L, void* ud, size_t* sz);
typedef int(__cdecl* ptr_lua_load)(void* L, lua_Reader reader, void* data, const char* chunkname);
ptr_lua_load call_lua_load = nullptr;
static const char* getS(void* L, void* ud, size_t* size)
{
	LoadS* ls = (LoadS*)ud;
	(void)L;
	if (ls->size == 0) return NULL;
	*size = ls->size;
	ls->size = 0;
	return ls->s;
}
