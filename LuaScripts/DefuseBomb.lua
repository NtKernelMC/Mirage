local HOOK_NAMES = {
    "string.rep",
    "load",
    "loadstring",
}

local REP_COUNT_LIMIT = 2 ^ 20
local PAYLOAD_SIZE_LIMIT = 4 * 1024 * 1024
local FOLLOWUP_WINDOW_MS = 2000

local hook_installed = false
local last_bomb = {
    tick = 0,
    resource = false,
    file = "",
    line = 0,
    reason = "",
}

local function now()
    if getTickCount then
        return getTickCount()
    end
    return 0
end

local function res_name(res)
    if not res or not getResourceName then
        return "unknown"
    end

    local ok, name = pcall(getResourceName, res)
    if ok and name and name ~= "" then
        return name
    end

    return "unknown"
end

local function dbg(msg)
    if outputDebugString then
        outputDebugString("[MirageBombGuard] " .. tostring(msg), 2)
    end
end

local function is_space_char(ch)
    return ch == " " or ch == "\n" or ch == "\r" or ch == "\t" or ch == "\v" or ch == "\f" or ch == "\0"
end

local function is_space_token(token)
    if type(token) ~= "string" or token == "" then
        return false
    end

    for i = 1, #token do
        if not is_space_char(token:sub(i, i)) then
            return false
        end
    end

    return true
end

local function looks_like_whitespace_blob(value)
    if type(value) ~= "string" then
        return false
    end

    local len = #value
    if len < PAYLOAD_SIZE_LIMIT then
        return false
    end

    local samples = 96
    local step = math.max(1, math.floor(len / samples))
    local seen = {}
    local seen_count = 0
    local checked = 0

    for i = 1, len, step do
        local ch = value:sub(i, i)
        if not is_space_char(ch) then
            return false
        end

        if not seen[ch] then
            seen[ch] = true
            seen_count = seen_count + 1
            if seen_count > 2 then
                return false
            end
        end

        checked = checked + 1
        if checked >= samples then
            break
        end
    end

    return checked >= 16
end

local function looks_like_rep_bomb(pattern, count)
    if type(pattern) ~= "string" then
        return false
    end

    count = tonumber(count)
    if not count or count < REP_COUNT_LIMIT then
        return false
    end

    if #pattern == 0 or #pattern > 8 then
        return false
    end

    if not is_space_token(pattern) then
        return false
    end

    return (#pattern * count) >= PAYLOAD_SIZE_LIMIT
end

local function inspect_reader_upvalues(reader)
    if type(reader) ~= "function" then
        return false, nil
    end

    if type(debug) ~= "table" or type(debug.getupvalue) ~= "function" then
        return false, nil
    end

    for i = 1, 16 do
        local name, value = debug.getupvalue(reader, i)
        if not name then
            break
        end

        if looks_like_whitespace_blob(value) then
            return true, name
        end
    end

    return false, nil
end

local function mark_bomb(sourceResource, luaFilename, luaLineNumber, functionName, reason)
    last_bomb.tick = now()
    last_bomb.resource = sourceResource or false
    last_bomb.file = tostring(luaFilename or "?")
    last_bomb.line = tonumber(luaLineNumber) or 0
    last_bomb.reason = tostring(reason or "unknown")

    dbg(string.format(
        "blocked %s from %s (%s:%d) reason=%s",
        tostring(functionName),
        res_name(sourceResource),
        last_bomb.file,
        last_bomb.line,
        last_bomb.reason
    ))
end

local function should_block_followup(sourceResource)
    if last_bomb.tick == 0 then
        return false
    end

    if now() - last_bomb.tick > FOLLOWUP_WINDOW_MS then
        return false
    end

    return sourceResource == last_bomb.resource
end

local function logical_bomb_hook(sourceResource, functionName, isAllowedByACL, luaFilename, luaLineNumber, ...)
    local args = { ... }

    if functionName == "string.rep" then
        if looks_like_rep_bomb(args[1], args[2]) then
            mark_bomb(sourceResource, luaFilename, luaLineNumber, functionName, "huge whitespace string.rep")
            return "skip"
        end
        return
    end

    if functionName == "loadstring" then
        if looks_like_whitespace_blob(args[1]) then
            mark_bomb(sourceResource, luaFilename, luaLineNumber, functionName, "huge whitespace loadstring")
            return "skip"
        end
        if should_block_followup(sourceResource) then
            mark_bomb(sourceResource, luaFilename, luaLineNumber, functionName, "follow-up after string.rep bomb")
            return "skip"
        end
        return
    end

    if functionName == "load" then
        local chunk = args[1]

        if looks_like_whitespace_blob(chunk) then
            mark_bomb(sourceResource, luaFilename, luaLineNumber, functionName, "huge whitespace load")
            return "skip"
        end

        if type(chunk) == "function" then
            local hit, upvalue_name = inspect_reader_upvalues(chunk)
            if hit then
                mark_bomb(sourceResource, luaFilename, luaLineNumber, functionName, "reader upvalue bomb: " .. tostring(upvalue_name))
                return "skip"
            end
        end

        if should_block_followup(sourceResource) then
            mark_bomb(sourceResource, luaFilename, luaLineNumber, functionName, "follow-up after string.rep bomb")
            return "skip"
        end
    end
end

local function hook_api(name, ...)
    if mirageFunc then
        if name == "hideFunctionCall" then
            return mirageFunc("hideFunctionCall", ...)
        end
        if name == "setDbgHook" then
            return mirageFunc("setDbgHook", ...)
        end
        if name == "removeDbgHook" then
            return mirageFunc("removeDbgHook", ...)
        end
    end

    if name == "setDbgHook" and addDebugHook then
        return addDebugHook(...)
    end

    if name == "removeDbgHook" and removeDebugHook then
        return removeDebugHook(...)
    end

    if name == "hideFunctionCall" then
        return true
    end

    return false
end

local function install_hook()
    if hook_installed then
        return true
    end

    hook_api("hideFunctionCall", true)
    local ok = hook_api("setDbgHook", "preFunction", logical_bomb_hook, HOOK_NAMES)
    hook_api("hideFunctionCall", false)

    hook_installed = ok and true or false
    if hook_installed then
        dbg("hook installed")
    else
        dbg("failed to install hook")
    end

    return hook_installed
end

local function remove_hook()
    if not hook_installed then
        return true
    end

    hook_api("hideFunctionCall", true)
    local ok = hook_api("removeDbgHook", "preFunction", logical_bomb_hook)
    hook_api("hideFunctionCall", false)

    hook_installed = not (ok and true or false)
    if not hook_installed then
        dbg("hook removed")
    end

    return ok and true or false
end

_G.MirageLogicalBombGuardEnable = install_hook
_G.MirageLogicalBombGuardDisable = remove_hook
_G.MirageLogicalBombGuardInfo = function()
    return {
        installed = hook_installed,
        last_tick = last_bomb.tick,
        last_resource = res_name(last_bomb.resource),
        last_file = last_bomb.file,
        last_line = last_bomb.line,
        last_reason = last_bomb.reason,
    }
end

install_hook()

if addEventHandler and resourceRoot then
    addEventHandler("onClientResourceStop", resourceRoot, function()
        remove_hook()
    end)
end
