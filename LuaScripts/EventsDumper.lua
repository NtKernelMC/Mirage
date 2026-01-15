local block_dumper = false
local selfResource = getResourceName(getThisResource())

bindKey("delete", "down", function()
    playSoundFrontEnd(11)
    block_dumper = true
    mirageFunc("antiMirage")
    mirageFunc("hideFunctionCall", true)
    setElementData(localPlayer, "antiMirage", true, false)
    mirageFunc("hideFunctionCall", false)
end)

function replaceResourceIdentifier(s)
    s = s:gsub("elem:resource%x%x%x%x%x%x%x%x", "root")
    s = s:gsub("elem:root%x%x%x%x%x%x%x%x", "root")
    return s
end

function replacePlayerName(s, playerName)
    local escapedPlayerName = playerName:gsub("([^%w])", "%%%1")
    local pattern = "elem:player%[" .. escapedPlayerName .. "%]"
    s = s:gsub(pattern, "localPlayer")
    return s
end

function replaceAnyVehicleIdentifier(s)
    return s:gsub("elem:vehicle%[%]%x%x%x%x%x%x%x%x", "localPlayer.vehicle")
end


local function bracesToParensOnce(s)
    local first = s:find("{", 1, true)
    if first then
        s = s:sub(1, first - 1) .. "(" .. s:sub(first + 1)
    end

    local last = s:match(".*()}")
    if last then
        s = s:sub(1, last - 1) .. ")" .. s:sub(last + 1)
    end

    return s
end

local LOG_MAX_LINES = 3000

local ui = {
    wnd = nil,
    memo = nil,
    btnClear = nil,
    visible = false,
    lines = {},
}

local function uiRefreshMemo()
    if not ui.memo then return end
    guiSetText(ui.memo, table.concat(ui.lines, "\n"))
    guiMemoSetCaretIndex(ui.memo, #guiGetText(ui.memo))
end

function LogLine(text)
    text = tostring(text or "")
    ui.lines[#ui.lines + 1] = text
    if #ui.lines > LOG_MAX_LINES then
        table.remove(ui.lines, 1)
    end
    if ui.visible then
        uiRefreshMemo()
    end
end

function ClearLog()
    ui.lines = {}
    if ui.memo then guiSetText(ui.memo, "") end
end

local function uiSetVisible(state)
    ui.visible = state and true or false
    if not ui.wnd then return end

    guiSetVisible(ui.wnd, ui.visible)
    showCursor(ui.visible)
    guiSetInputEnabled(ui.visible)

    if ui.visible then
        uiRefreshMemo()
    end
end

local function uiToggle()
    if block_dumper then return end
    uiSetVisible(not ui.visible)
end

local function uiCreate()
    local sx, sy = guiGetScreenSize()
    local w, h = math.floor(sx * 0.70), math.floor(sy * 0.55)
    local x, y = math.floor((sx - w) / 2), math.floor((sy - h) / 2)

    ui.wnd = guiCreateWindow(x, y, w, h, "Event Log (Ctrl+C to copy)", false)
    guiWindowSetSizable(ui.wnd, true)

    ui.memo = guiCreateMemo(10, 25, w - 20, h - 70, "", false, ui.wnd)
    guiMemoSetReadOnly(ui.memo, false) -- чтобы можно было выделять/копировать

    ui.btnClear = guiCreateButton(10, h - 35, 120, 25, "Clear", false, ui.wnd)
    addEventHandler("onClientGUIClick", ui.btnClear, function()
        ClearLog()
    end, false)

    -- Стартуем скрытым
    uiSetVisible(false)
end

local function isDumperElement(elem)
    if not elem or not isElement(elem) then return false end
    if elem == ui.wnd or elem == ui.memo or elem == ui.btnClear then
        return true
    end

    local parent = getElementParent(elem)
    while parent do
        if parent == ui.wnd then
            return true
        end
        parent = getElementParent(parent)
    end

    return false
end

local function DumperGuiGuard(sourceResource, functionName, isAllowedByACL, luaFilename, luaLineNumber, ...)
    if not isElement(ui.wnd) then return end
    if sourceResource and getResourceName(sourceResource) == selfResource then return end
    local elem = select(1, ...)
    if isDumperElement(elem) then
        return "skip"
    end
end


uiCreate()
bindKey("F1", "down", uiToggle)


function evDumper(sourceResource, functionName, isAllowedByACL, luaFilename, luaLineNumber, ...)
    local args = { ... }
    local resname = sourceResource and getResourceName(sourceResource)

    if tostring(args[1]) ~= 'CB:SetGroupHealthBuff'
        and tostring(args[1]) ~= 'onCaloriesUpdate'
        and tostring(args[1]) ~= 'lossHungryHealth'
        and tostring(args[1]) ~= 'OnPlayerWantGetPromoStatistics'
        and tostring(args[1]) ~= 'ChatStart'
        and tostring(args[1]) ~= 'ChatStop'
        and tostring(args[1]) ~= 'OnUpdateStaminaHandler'
        and tostring(args[1]) ~= 'Trade:Destroy'
        and tostring(args[1]) ~= 'loadVehicleDirtServer'
    then
        if resname ~= 'nrp_player_head'
            and tostring(args[1]) ~= 'saveWindowsPositionServer'
            and tostring(args[1]) ~= 'afk:cancelByKey'
            and tostring(args[1]) ~= 'onPlayerAFKStateChange'
        then
            local code_args = inspect(args)
            code_args = replaceResourceIdentifier(code_args)
            local my_elem = inspect(localPlayer)
            local playerName = my_elem:match("elem:player%[(.-)%]")
            code_args = replacePlayerName(code_args, playerName)
            code_args = replaceAnyVehicleIdentifier(code_args)
            code_args = bracesToParensOnce(code_args)
            if not block_dumper then
                LogLine("[".. tostring(resname) .. "] ".. tostring(functionName) .. ' ' .. code_args)
            end
        end
    end
end

mirageFunc("hideFunctionCall", true)
mirageFunc("setDbgHook", "preFunction", evDumper, { "triggerServerEvent", "triggerLatentServerEvent" } )
mirageFunc("setDbgHook", "preFunction", DumperGuiGuard, {
    "guiGetText",
})
mirageFunc("hideFunctionCall", false)
