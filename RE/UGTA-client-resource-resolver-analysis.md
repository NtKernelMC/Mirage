# Objective
Найти в `UGTA client.dll` безопасный путь получения реального `CResource*` для cross-resource Lua thread inject и убрать зависимость от Lua userdata/scriptID как от "готового" указателя.

# Scope
- Бинарь: `UGTA client.dll`, открытый в IDA Pro через `ida-pro-mcp`.
- Кодовая цель: `MirageAgent` runtime bridge для `luaThreadLoad`.
- Интересующие узлы:
1. safe resolver по имени ресурса
2. safe resolver owner-resource из `lua_State*`
3. подтверждение, что старый путь через userdata не дает готовый `CResource*`

# Method
1. Через MCP просмотрены функции вокруг `CResourceManager` и xref-цепочки от `getResourceFromName`.
2. Сверены декомпы с локальными сорсами MTA `1.6.0`:
- `CResourceManager.cpp`
- `CLuaResourceDefs.cpp`
- `CLuaFunctionParseHelpers.cpp`
3. После подтверждения адресов обновлен bridge в `MirageAgent`.

# Findings
1. Safe resolver по имени ресурса уже есть в клиенте
- `0x100EA5F0` (`sub_100EA5F0`) принимает `CResourceManager*` и `const char*`, проходит по списку ресурсов и возвращает реальный `CResource*`.
- Это и есть `CResourceManager::GetResource(const char* szResourceName)`.

2. `getResourceFromName` Lua binding внутри использует именно safe resolver, а не userdata как указатель
- `0x101DDD50` (`sub_101DDD50`) берет имя ресурса из Lua, вызывает `sub_100EA5F0((_DWORD *)dword_105AB668, name)`, а потом делает `lua_pushresource(a1, v5)`.
- Значит userdata в Lua только обертка над scriptID, а живой `CResource*` получается раньше, внутри клиента.

3. Safe owner-resource resolver из `lua_State*` тоже есть
- `0x100EA640` (`sub_100EA640`) получает owner `CResource*` из `lua_State*`.
- По смыслу и сорсам это `CResourceManager::GetResourceFromLuaState(lua_State* luaVM)`.
- В текущем билде функция скомпилилась как `__stdcall(int)` из-за неиспользуемого `this`.

4. `0x100EA670` не подходит для scriptID-resolve под эту задачу
- По декомпу это lookup по `unsigned __int16` через map/list ресурсов.
- По соответствию сорсам это `CResourceManager::GetResourceFromNetID(unsigned short usNetID)`, а не `GetResourceFromScriptID`.
- Пихать его вместо scriptID-resolver было бы хуйней и давало бы ложные указатели.

5. Старый Mirage-path ломался логически
- `luaThreadLoad` раньше мог получить target resource только через Lua userdata и трактовал это как `CResource*`.
- В MTA resource userdata хранит scriptID, а не прямой pointer на `CResource`.
- Поэтому cross-resource inject мог улетать в `CreateVirtualMachine` с мусорным адресом и рандомно падать.

6. Для автоподхвата `CResourceManager*` подходит сигнатурный slot-load
- В клиенте есть место с `mov eax, dword_105AB668`.
- Паттерн `A1 ? ? ? ? 89 45 ? 8D 85` выводит на инструкцию загрузки global manager pointer.
- Из `mov` можно безопасно вытащить сам адрес слота (`moffs32` после `A1`) и использовать его как `void**`.

# Evidence
1. IDA MCP `decompile(0x101DDD50)`
- видно прямой вызов `sub_100EA5F0((_DWORD *)dword_105AB668, v4)`
- после него `lua_pushresource(a1, v5)`

2. IDA MCP `decompile(0x100EA5F0)`
- цикл по `m_resources`
- `_stricmp(resourceName, String2)`
- возврат `v4[2]`, то есть pointer на найденный ресурс

3. IDA MCP `decompile(0x100EA640)`
- вызов `sub_10130A20(luaVM)`
- возврат `*(... + 72)`, что совпадает с `CLuaMain::m_pResource`

4. IDA MCP `decompile(0x100EA670)` и `export_funcs(prototypes)`
- prototype: `_WORD *__thiscall(_DWORD *this, unsigned __int16)`
- lookup по `unsigned short`, что совпадает с `GetResourceFromNetID`, а не с `GetResourceFromScriptID(uint)`

5. Локальные сорсы MTA
- `CResourceManager::GetResource(const char*)`
- `CResourceManager::GetResourceFromLuaState(lua_State*)`
- `lua_pushresource(... reinterpret_cast<unsigned int*>(pResource->GetScriptID()))`
- `GetResourceFromScriptID(reinterpret_cast<unsigned long>(ptr))`
6. Пользовательский паттерн на global manager slot
- `A1 ? ? ? ? 89 45 ? 8D 85`
- Нужно читать не адрес инструкции, а `DWORD` по `mov+1`

# Hypotheses
1. Для стабильного cross-resource inject достаточно:
- safe `GetResource(const char*)`
- safe `GetResourceFromLuaState(lua_State*)`
- валидный `CResourceManager*`

2. `CLuaManager*` можно и дальше получать из owner-resource scan-пути как fallback
- главный краш был не в `luaManager`, а в подсовывании scriptID вместо `CResource*`

# Next Steps
1. Прогнать живой inject в другой ресурс и проверить, что `CreateVirtualMachine` получает валидный `CResource*`.
2. Если direct global-slot паттерн поплывет на другом билде, добавить fallback на `g_pClientGame + m_pResourceManager`.

# Changelog
- 2026-03-08: Через IDA MCP подтверждены safe resource resolvers `0x100EA5F0` и `0x100EA640`; `0x100EA670` исключен как netID-resolver. `MirageAgent` переведен на safe cross-resource target resolve по имени ресурса.
- 2026-03-08: Добавлен автоподхват global `CResourceManager*` через паттерн `A1 ? ? ? ? 89 45 ? 8D 85` с извлечением slot-адреса из `mov eax, dword_xxxxxxxx`.
