local isGUIOpen = false
local currentTab = "injector"
local last_code = ""
local block_dumper = false

local selfResource = getResourceName(getThisResource())

local LOG_MAX_LINES = 3000
local RENDER_MAX_LINES = 900
local eventLines = {}
local luaThreads = {}
local selectedThreadId = nil
local targetResourceName = selfResource or ""
local debugModeEnabled = false
local uiRenderHookActive = false
local activatePanicMode = nil
local fenrirVehicle = nil
local luftwaffeDrivers = {}
local luftwaffeSelectedPlayer = nil
local luftwaffeSelectedPid = nil
local luftwaffeFilterId = ""
local luftwaffeSpectating = false

local ui = {
    width = 1160,
    height = 820,
    bannerW = 1128,
    bannerH = 210,
    posInitialized = false,
    winX = nil,
    winY = nil,
    assetsReady = false,
    keyCaptionFont = "mirage_cyber_caption",
    keyButtonFont = "mirage_cyber_buttons",
    keyBanner = "mirage_unified_banner",
    codeKey = "injector_code_buffer",
    resourceKey = "injector_target_resource",
    luftwaffeFilterKey = "luftwaffe_filter_player_id",
}

local function joinPath(base, name)
    if not base or base == "" then return name end
    local tail = base:sub(-1)
    if tail ~= "\\" and tail ~= "/" then
        base = base .. "\\"
    end
    return base .. name
end

local function setupImGuiAssets()
    if ui.assetsReady then return end

    local base = mirageFunc("getLuaScriptsPath") or ""
    local banner = joinPath(base, "background.jpg")
    local captionFont = joinPath(base, "Cyber Blast.otf")

    mirageFunc("imgui.loadImage", banner, ui.keyBanner)
    mirageFunc("imgui.loadFont", captionFont, 34, ui.keyCaptionFont)
    mirageFunc("imgui.loadFont", captionFont, 48, ui.keyButtonFont)

    mirageFunc("imgui.themeDark")
    mirageFunc("imgui.setStyleRounding", 14.0, 10.0, 8.0, 10.0)
    mirageFunc("imgui.enable", true)

    ui.assetsReady = true
end

local function setUiVisible(state)
    local wasOpen = isGUIOpen
    isGUIOpen = state and true or false
    if (not wasOpen) and isGUIOpen then
        ui.posInitialized = false
        ui.winX = nil
        ui.winY = nil
    end
    mirageFunc("blockScreen", isGUIOpen)
    mirageFunc("imgui.captureInput", isGUIOpen)
    mirageFunc("imgui.blockBinds", isGUIOpen)
    mirageFunc("imgui.forceCursor", isGUIOpen)
    mirageFunc("imgui.drawCursor", isGUIOpen)
    if showCursor then
        showCursor(isGUIOpen, true)
    end
    if setCursorAlpha then
        setCursorAlpha(isGUIOpen and 0 or 255)
    end
    if toggleAllControls then
        pcall(toggleAllControls, not isGUIOpen)
    end
    showChat(not isGUIOpen)
    if DisableHUD then DisableHUD(isGUIOpen) end
end

local function pushMainTheme()
    mirageFunc("imgui.pushStyleColor", 0, 240, 224, 224, 255) -- Text
    mirageFunc("imgui.pushStyleColor", 2, 7, 6, 7, 172)        -- WindowBg
    mirageFunc("imgui.pushStyleColor", 3, 15, 6, 8, 206)       -- ChildBg
    mirageFunc("imgui.pushStyleColor", 5, 146, 32, 45, 230)    -- Border
    mirageFunc("imgui.pushStyleColor", 7, 44, 10, 15, 232)     -- FrameBg
    mirageFunc("imgui.pushStyleColor", 8, 70, 16, 23, 246)     -- FrameBgHovered
    mirageFunc("imgui.pushStyleColor", 9, 102, 22, 34, 255)    -- FrameBgActive
    mirageFunc("imgui.pushStyleColor", 10, 58, 12, 20, 255)    -- TitleBg
    mirageFunc("imgui.pushStyleColor", 11, 96, 18, 30, 255)    -- TitleBgActive
    mirageFunc("imgui.pushStyleColor", 12, 40, 9, 14, 230)     -- TitleBgCollapsed

    mirageFunc("imgui.pushStyleColor", 21, 105, 22, 34, 236)   -- Button
    mirageFunc("imgui.pushStyleColor", 22, 146, 31, 46, 248)   -- ButtonHovered
    mirageFunc("imgui.pushStyleColor", 23, 180, 38, 58, 255)   -- ButtonActive

    mirageFunc("imgui.pushStyleColor", 24, 92, 22, 34, 235)    -- Header
    mirageFunc("imgui.pushStyleColor", 25, 128, 28, 42, 248)   -- HeaderHovered
    mirageFunc("imgui.pushStyleColor", 26, 156, 34, 52, 255)   -- HeaderActive

    mirageFunc("imgui.pushStyleColor", 14, 26, 8, 10, 186)     -- ScrollbarBg
    mirageFunc("imgui.pushStyleColor", 15, 105, 23, 36, 236)   -- ScrollbarGrab
    mirageFunc("imgui.pushStyleColor", 16, 156, 32, 49, 248)   -- ScrollbarGrabHovered
    mirageFunc("imgui.pushStyleColor", 17, 208, 46, 66, 255)   -- ScrollbarGrabActive

    mirageFunc("imgui.pushStyleColor", 18, 212, 54, 70, 255)   -- CheckMark
    mirageFunc("imgui.pushStyleColor", 19, 136, 30, 46, 246)   -- SliderGrab
    mirageFunc("imgui.pushStyleColor", 20, 196, 45, 66, 255)   -- SliderGrabActive

    mirageFunc("imgui.pushStyleColor", 27, 186, 30, 44, 255)   -- Separator
    mirageFunc("imgui.pushStyleColor", 28, 212, 42, 58, 255)   -- SeparatorHovered
    mirageFunc("imgui.pushStyleColor", 29, 228, 52, 70, 255)   -- SeparatorActive
end

local function popMainTheme()
    mirageFunc("imgui.popStyleColor", 26)
end

local function pushTabTheme()
    mirageFunc("imgui.pushStyleColor", 0, 236, 236, 236, 255)  -- Text
    mirageFunc("imgui.pushStyleColor", 33, 148, 36, 54, 252)   -- TabHovered
    mirageFunc("imgui.pushStyleColor", 34, 84, 16, 28, 236)    -- Tab
    mirageFunc("imgui.pushStyleColor", 35, 176, 38, 58, 252)   -- TabSelected
    mirageFunc("imgui.pushStyleColor", 36, 194, 40, 60, 255)   -- TabSelectedOverline
    mirageFunc("imgui.pushStyleColor", 37, 62, 12, 22, 226)    -- TabDimmed
    mirageFunc("imgui.pushStyleColor", 38, 146, 30, 48, 246)   -- TabDimmedSelected
    mirageFunc("imgui.pushStyleColor", 39, 176, 34, 52, 250)   -- TabDimmedSelectedOverline
end

local function popTabTheme()
    mirageFunc("imgui.popStyleColor", 8)
end

local function drawSmokyBackdrop(x, y, w, h)
    local rounding = 14
    mirageFunc("imgui.drawRectFilled", x, y, w, h, 4, 4, 4, 170, rounding, false)

    local steps = 30
    local stripeH = h / steps
    for i = 0, steps - 1 do
        local t = i / (steps - 1)
        local r = math.floor(10 + (88 * t))
        local g = math.floor(6 + (18 * t))
        local b = math.floor(8 + (22 * t))
        local a = math.floor(42 + (84 * t))
        local sy = y + (i * stripeH)
        mirageFunc("imgui.drawRectFilled", x + 1, sy, w - 2, stripeH + 1, r, g, b, a, 0, false)
    end

    mirageFunc("imgui.drawCircleFilled", x + w * 0.20, y + h * 0.30, h * 0.25, 98, 18, 26, 40, 36, false)
    mirageFunc("imgui.drawCircleFilled", x + w * 0.79, y + h * 0.74, h * 0.32, 105, 22, 30, 34, 36, false)
    mirageFunc("imgui.drawRect", x, y, w, h, 190, 34, 50, 148, rounding, 1.3, false)
end

local function drawCaptionTitle(x, y, text, bannerStartY)
    local frameX = x + 14
    local frameY = y + 8
    local frameW = ui.width - 28
    local frameH = math.max(40, bannerStartY - frameY)

    mirageFunc("imgui.drawRectFilled", frameX, frameY, frameW, frameH, 3, 3, 3, 218, 7, false)
    mirageFunc("imgui.drawRect", frameX, frameY, frameW, frameH, 190, 34, 48, 255, 7, 1.0, false)

    local tx = frameX + 8
    local ty = frameY + 7
    mirageFunc("imgui.drawText", text, tx - 1, ty, 192, 36, 52, 255, ui.keyCaptionFont, 34, true)
    mirageFunc("imgui.drawText", text, tx + 1, ty, 192, 36, 52, 255, ui.keyCaptionFont, 34, true)
    mirageFunc("imgui.drawText", text, tx, ty - 1, 192, 36, 52, 255, ui.keyCaptionFont, 34, true)
    mirageFunc("imgui.drawText", text, tx, ty + 1, 192, 36, 52, 255, ui.keyCaptionFont, 34, true)
    mirageFunc("imgui.drawText", text, tx, ty, 12, 8, 8, 255, ui.keyCaptionFont, 34, true)
end

local function pushEventLineColor(line)
    if line:find("triggerServerEvent", 1, true) then
        mirageFunc("imgui.pushStyleColor", 0, 250, 225, 120, 255)
    elseif line:find("localPlayer", 1, true) then
        mirageFunc("imgui.pushStyleColor", 0, 152, 230, 130, 255)
    elseif line:find("root", 1, true) then
        mirageFunc("imgui.pushStyleColor", 0, 132, 212, 255, 255)
    else
        mirageFunc("imgui.pushStyleColor", 0, 240, 200, 200, 255)
    end
end

local function popEventLineColor()
    mirageFunc("imgui.popStyleColor", 1)
end

local function appendEventLine(text)
    text = tostring(text or "")
    eventLines[#eventLines + 1] = text
    if #eventLines > LOG_MAX_LINES then
        table.remove(eventLines, 1)
    end
end

local function clearEventLog()
    eventLines = {}
end

local function pushUiMessage(eventName, text)
    if not triggerEvent then return end
    pcall(triggerEvent, eventName, root, tostring(text or ""))
end

local function notifySuccess(text)
    pushUiMessage("ShowSuccess", text)
end

local function notifyError(text)
    pushUiMessage("ShowError", text)
end

local function notifyWarning(text)
    pushUiMessage("ShowWarning", text)
end

local function trimText(text)
    text = tostring(text or "")
    text = text:gsub("^%s+", "")
    text = text:gsub("%s+$", "")
    return text
end

local function isVehicleEmpty(vehicle)
    if not isElement(vehicle) or getElementType(vehicle) ~= "vehicle" then
        return false
    end

    if getVehicleOccupant(vehicle, 0) then
        return false
    end

    local maxPassengers = getVehicleMaxPassengers(vehicle) or 0
    for seat = 1, maxPassengers do
        if getVehicleOccupant(vehicle, seat) then
            return false
        end
    end

    return true
end

local function getDistanceToElement2D(element)
    if not isElement(element) then
        return nil
    end

    local px, py = getElementPosition(localPlayer)
    local ex, ey = getElementPosition(element)
    return getDistanceBetweenPoints2D(px, py, ex, ey)
end

local function getNearestEmptyVehicle(radius)
    local px, py, pz = getElementPosition(localPlayer)
    local closestVehicle = nil
    local closestDistance = nil

    for _, vehicle in ipairs(getElementsByType("vehicle", root, true)) do
        if isVehicleEmpty(vehicle)
            and getElementDimension(vehicle) == getElementDimension(localPlayer)
            and getElementInterior(vehicle) == getElementInterior(localPlayer) then

            local vx, vy = getElementPosition(vehicle)
            local distance = getDistanceBetweenPoints2D(px, py, vx, vy)

            if distance <= radius and (not closestDistance or distance < closestDistance) then
                closestVehicle = vehicle
                closestDistance = distance
            end
        end
    end

    return closestVehicle, closestDistance
end

local function normalizePlayerFilterId(rawId)
    local text = trimText(rawId)
    if text == "" then
        return ""
    end

    if text:sub(1, 1):lower() == "p" then
        text = text:sub(2)
    end

    if text == "" then
        return ""
    end

    return "p" .. text
end

local function getPlayerDisplayName(player)
    local nametag = getPlayerNametagText and getPlayerNametagText(player) or nil
    if nametag and nametag ~= "" then
        return tostring(nametag)
    end

    local name = getPlayerName and getPlayerName(player) or "unknown"
    return tostring(name or "unknown")
end

local function getPlayerDriverVehicle(player)
    if not isElement(player) or getElementType(player) ~= "player" then
        return nil
    end

    if getElementDimension(player) ~= 0 or getElementInterior(player) ~= 0 then
        return nil
    end

    local vehicle = getPedOccupiedVehicle(player)
    if not isElement(vehicle) then
        return nil
    end

    if getPedOccupiedVehicleSeat(player) ~= 0 then
        return nil
    end

    if getElementDimension(vehicle) ~= 0 or getElementInterior(vehicle) ~= 0 then
        return nil
    end

    return vehicle
end

local function refreshLuftwaffeDrivers()
    local normalizedFilter = normalizePlayerFilterId(luftwaffeFilterId)
    local refreshed = {}

    for _, player in ipairs(getElementsByType("player", root)) do
        local vehicle = getPlayerDriverVehicle(player)
        if vehicle then
            local pid = tostring(getElementID(player) or "")
            if normalizedFilter == "" or pid == normalizedFilter then
                refreshed[#refreshed + 1] = {
                    player = player,
                    vehicle = vehicle,
                    pid = pid,
                    label = getPlayerDisplayName(player) .. " (" .. pid .. ")",
                }
            end
        end
    end

    table.sort(refreshed, function(a, b)
        return a.label:lower() < b.label:lower()
    end)

    luftwaffeDrivers = refreshed

    local selectedStillValid = false
    for i = 1, #luftwaffeDrivers do
        if luftwaffeDrivers[i].player == luftwaffeSelectedPlayer then
            luftwaffeSelectedPid = luftwaffeDrivers[i].pid
            selectedStillValid = true
            break
        end
    end

    if not selectedStillValid then
        luftwaffeSelectedPlayer = nil
        luftwaffeSelectedPid = nil
        if luftwaffeSpectating then
            setCameraTarget(localPlayer)
            luftwaffeSpectating = false
        end
    end
end

local function chooseFenrirVehicle()
    local vehicle, distance = getNearestEmptyVehicle(30.0)
    if not vehicle then
        notifyError("Пустая тачка в радиусе 30 м не найдена")
        return
    end

    fenrirVehicle = vehicle
    notifySuccess("Пустая тачка выбрана. Дистанция: " .. tostring(distance))
end

local function validateFenrirVehicle()
    if not isElement(fenrirVehicle) or getElementType(fenrirVehicle) ~= "vehicle" then
        notifyError("Сначала выбери пустую тачку")
        fenrirVehicle = nil
        return nil
    end

    if not isVehicleEmpty(fenrirVehicle) then
        notifyError("Выбранная тачка больше не пустая")
        return nil
    end

    return fenrirVehicle
end

local function validateSelectedLuftwaffePlayer()
    if not isElement(luftwaffeSelectedPlayer) then
        notifyError("Сначала выбери игрока в списке")
        luftwaffeSelectedPlayer = nil
        luftwaffeSelectedPid = nil
        return nil, nil
    end

    local vehicle = getPlayerDriverVehicle(luftwaffeSelectedPlayer)
    if not vehicle then
        notifyError("Выбранный игрок не онлайн, не в нулевом мире или не водитель")
        refreshLuftwaffeDrivers()
        return nil, nil
    end

    return luftwaffeSelectedPlayer, vehicle
end

function SafeTP(bx, by, bz, dim, int)
    local resname = getResourceFromName('ugta_casino_entrance') 
    local resourceRoot = getResourceRootElement(resname) 
    triggerServerEvent( "RequestTeleport", resourceRoot, bx, by, bz, tonumber(dim), tonumber(int))
    triggerServerEvent("SwitchPosition", resourceRoot)
    setElementInterior(localPlayer, tonumber(int))
end

local oldX, oldY, oldZ

local function toggleLuftwaffeSpectate()
    if luftwaffeSpectating then
        setCameraTarget(localPlayer)
        luftwaffeSpectating = false
        notifySuccess("Спек выключен")
        setTimer(function()
            safeTP(oldX, oldY, oldZ + 3.0, 0, 0)
        end, 1000, 1)
        return
    end

    local player = validateSelectedLuftwaffePlayer()
    if not player then
        return
    end
    oldX, oldY, oldZ = getElementPosition(localPlayer)
    setCameraTarget(player)
    luftwaffeSpectating = true
    notifySuccess("Спек включен")
end

local function performAstalavistaBaby()
    local ourVehicle = validateFenrirVehicle()
    if not ourVehicle then
        return
    end

    local _, targetVehicle = validateSelectedLuftwaffePlayer()
    if not targetVehicle then
        return
    end

    local x, y, z = getElementPosition(targetVehicle)
    z = z + 1000.0

    local okPush = mirageFunc("sendVehiclePushSync", ourVehicle)
    local okUnoccupied = false
    if okPush then
        okUnoccupied = mirageFunc("sendUnoccupiedVehicleSync", ourVehicle, x, y, z, targetVehicle)
    end

    if okPush and okUnoccupied then
        setTimer(function(vehicle)
            if not isElement(vehicle) then
                return
            end

            local vx, vy, vz = getElementPosition(vehicle)
            setElementPosition(vehicle, vx, vy, z)
        end, 2000, 1, ourVehicle)
        fenrirVehicle = nil
        notifySuccess("Astalavista Baby выполнен")
    else
        notifyError("Не удалось отправить PushSync/UnoccupiedSync")
    end
end

local function getEventLogText()
    if #eventLines == 0 then return "" end
    return table.concat(eventLines, "\n")
end

local function copyToClipboard(text, successMessage)
    text = tostring(text or "")
    if text == "" then
        notifyWarning("Events log is empty")
        return
    end

    if not setClipboard then
        notifyError("Clipboard is unavailable")
        return
    end

    local ok, copied = pcall(setClipboard, text)
    if ok and copied ~= false then
        notifySuccess(tostring(successMessage or "Copied"))
    else
        notifyError("Clipboard copy failed")
    end
end

local function parseLuaThreads(raw)
    luaThreads = {}
    if type(raw) ~= "string" or raw == "" then
        selectedThreadId = nil
        return
    end

    for line in raw:gmatch("[^\r\n]+") do
        local id, resourceName, loadedAt, state = line:match("^(%d+)|([^|]*)|([^|]*)|([^|]*)$")
        if id then
            luaThreads[#luaThreads + 1] = {
                id = tonumber(id),
                resource = resourceName,
                loadedAt = loadedAt,
                state = state,
            }
        end
    end

    if selectedThreadId then
        local exists = false
        for i = 1, #luaThreads do
            if luaThreads[i].id == selectedThreadId then
                exists = true
                break
            end
        end
        if not exists then
            selectedThreadId = nil
        end
    end
end

local function refreshLuaThreads()
    parseLuaThreads(mirageFunc("luaThreadList") or "")
end

local function applyDebugMode(state)
    debugModeEnabled = state and true or false
    if setDebugViewActive then
        pcall(setDebugViewActive, debugModeEnabled)
    end
end

local function replace_i_plain(s, find, repl)
    if not s or not find or find == "" then return s, 0 end
    repl = repl or ""

    local sl, fl = s:lower(), find:lower()
    local out, last, cnt = {}, 1, 0

    while true do
        local i, j = sl:find(fl, last, true)
        if not i then
            out[#out + 1] = s:sub(last)
            break
        end
        out[#out + 1] = s:sub(last, i - 1)
        out[#out + 1] = repl
        last = j + 1
        cnt = cnt + 1
    end

    return table.concat(out), cnt
end

local function utf8_next(s, i)
    local b = s:byte(i)
    if not b then return nil, i end

    local len = 1
    if b >= 0xF0 then
        len = 4
    elseif b >= 0xE0 then
        len = 3
    elseif b >= 0xC2 then
        len = 2
    end

    if i + len - 1 > #s then
        return s:sub(i, i), i + 1
    end

    return s:sub(i, i + len - 1), i + len
end

local function build_mirage_equiv()
    local function u8(a, b) return string.char(a, b) end
    return {
        m = { "m", "M", u8(0xD0, 0xBC), u8(0xD0, 0x9C) },
        i = { "i", "I", u8(0xD1, 0x96), u8(0xD0, 0x86) },
        r = { "r", "R" },
        a = { "a", "A", u8(0xD0, 0xB0), u8(0xD0, 0x90) },
        g = { "g", "G" },
        e = { "e", "E", u8(0xD0, 0xB5), u8(0xD0, 0x95) },
        f = { "f", "F" },
        u = { "u", "U" },
        n = { "n", "N" },
        c = { "c", "C", u8(0xD1, 0x81), u8(0xD0, 0xA1) },
    }
end

local MIRAGE_EQUIV = build_mirage_equiv()

local function mirage_char_eq(ch, expected)
    local list = MIRAGE_EQUIV[expected]
    if not list then return ch == expected end
    for i = 1, #list do
        if ch == list[i] then return true end
    end
    return false
end

local function replace_mirage_homoglyphs(s)
    local target = { "m", "i", "r", "a", "g", "e", "f", "u", "n", "c" }
    local out = {}
    local i = 1
    local count = 0

    while i <= #s do
        local j = i
        local matched = true

        for t = 1, #target do
            local ch
            ch, j = utf8_next(s, j)
            if not ch or not mirage_char_eq(ch, target[t]) then
                matched = false
                break
            end
        end

        if matched then
            out[#out + 1] = "getPedVoice"
            i = j
            count = count + 1
        else
            local ch
            ch, i = utf8_next(s, i)
            out[#out + 1] = ch
        end
    end

    return table.concat(out), count
end

local function injectCode(code)
    if not code or code == "" then return end

    local resourceName = tostring(targetResourceName or ""):gsub("^%s+", ""):gsub("%s+$", "")
    if resourceName == "" then
        resourceName = selfResource or ""
        targetResourceName = resourceName
    end

    last_code = code
    local replaced, count = replace_i_plain(code, "mirageFunc", "getPedVoice")
    local replaced_h, count_h = replace_mirage_homoglyphs(replaced)
    if count_h > 0 then
        replaced = replaced_h
    end
    if (count + count_h) > 0 then
        code = replaced
        last_code = code
    end

    if type(loadstring) == "function" then
        local okSyntax, compiled = pcall(loadstring, code)
        if (not okSyntax) or (not compiled) then
            notifyError("Error in syntax, look Debug Mode.")
            return
        end
    end

    local threadId = mirageFunc("luaThreadLoad", resourceName, code)
    if threadId and threadId ~= false then
        notifySuccess("Code injected in thread #" .. tostring(threadId))
        refreshLuaThreads()
    else
        notifyError("Inject failed (Lua thread VM bridge not configured)")
    end
end

local function isMirageResource(sourceResource)
    if not sourceResource then return false end
    local res_name = getResourceName(sourceResource)
    if not res_name or res_name == "" then return false end

    local list = mirageFunc("getMirageResources") or ""
    for name in list:gmatch("[^\r\n]+") do
        if name == res_name then
            return true
        end
    end
    return false
end

local function replaceResourceIdentifier(s)
    s = s:gsub("elem:resource%x%x%x%x%x%x%x%x", "root")
    s = s:gsub("elem:root%x%x%x%x%x%x%x%x", "root")
    return s
end

local function replacePlayerName(s, playerName)
    local escapedPlayerName = playerName:gsub("([^%w])", "%%%1")
    local pattern = "elem:player%[" .. escapedPlayerName .. "%]"
    s = s:gsub(pattern, "localPlayer")
    return s
end

local function replaceAnyVehicleIdentifier(s)
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

local function buildEventDumpArgs(args)
    local code_args = inspect(args)
    code_args = replaceResourceIdentifier(code_args)
    local my_elem = inspect(localPlayer)
    local playerName = my_elem:match("elem:player%[(.-)%]")
    if playerName and playerName ~= "" then
        code_args = replacePlayerName(code_args, playerName)
    end
    code_args = replaceAnyVehicleIdentifier(code_args)
    code_args = bracesToParensOnce(code_args)
    return code_args
end

local function evDumper(sourceResource, functionName, isAllowedByACL, luaFilename, luaLineNumber, ...)
    local args = { ... }
    local resname = sourceResource and getResourceName(sourceResource)

    if tostring(args[1]) ~= "CB:SetGroupHealthBuff"
        and tostring(args[1]) ~= "onCaloriesUpdate"
        and tostring(args[1]) ~= "lossHungryHealth"
        and tostring(args[1]) ~= "OnPlayerWantGetPromoStatistics"
        and tostring(args[1]) ~= "ChatStart"
        and tostring(args[1]) ~= "ChatStop"
        and tostring(args[1]) ~= "OnUpdateStaminaHandler"
        and tostring(args[1]) ~= "Trade:Destroy"
        and tostring(args[1]) ~= "loadVehicleDirtServer"
    then
        if resname ~= "nrp_player_head"
            and resname ~= selfResource
            and tostring(args[1]) ~= "saveWindowsPositionServer"
            and tostring(args[1]) ~= "afk:cancelByKey"
            and tostring(args[1]) ~= "onPlayerAFKStateChange"
        then
            local code_args = buildEventDumpArgs(args)
            if not block_dumper then
                appendEventLine("[" .. tostring(resname) .. "] " .. tostring(functionName) .. " " .. code_args)
            end
        end
    end
end

local originalTriggerServerEvent = triggerServerEvent
local originalTriggerLatentServerEvent = triggerLatentServerEvent

if originalTriggerServerEvent then
    triggerServerEvent = function(...)
        if not block_dumper then
            local args = { ... }
            local code_args = buildEventDumpArgs(args)
            appendEventLine("[" .. tostring(selfResource) .. "] triggerServerEvent " .. code_args)
        end
        return originalTriggerServerEvent(...)
    end
end

if originalTriggerLatentServerEvent then
    triggerLatentServerEvent = function(...)
        if not block_dumper then
            local args = { ... }
            local code_args = buildEventDumpArgs(args)
            appendEventLine("[" .. tostring(selfResource) .. "] triggerLatentServerEvent " .. code_args)
        end
        return originalTriggerLatentServerEvent(...)
    end
end

local function renderInjectorTab(contentY)
    mirageFunc("imgui.setCursorPos", 14, contentY)
    targetResourceName = mirageFunc("imgui.inputText", "Resource", targetResourceName, 96, 0, ui.resourceKey) or targetResourceName
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.smallButton", "Use Current", "inj_btn_use_current_resource") then
        targetResourceName = selfResource or ""
        mirageFunc("imgui.setString", ui.resourceKey, targetResourceName)
    end
    mirageFunc("imgui.sameLine")
    local debugFlag = mirageFunc("imgui.checkbox", "Debug Mode", debugModeEnabled, "inj_chk_debug_mode")
    if debugFlag ~= debugModeEnabled then
        applyDebugMode(debugFlag)
    end

    local editorY = contentY + 36
    local codeW = ui.width - 30
    local codeH = ui.height - editorY - 95
    if codeH < 360 then codeH = 360 end

    mirageFunc("imgui.setCursorPos", 14, editorY)

    mirageFunc("imgui.pushStyleColor", 0, 236, 228, 228, 255)
    mirageFunc("imgui.pushStyleColor", 7, 3, 3, 3, 245)
    mirageFunc("imgui.pushStyleColor", 8, 3, 3, 3, 255)
    mirageFunc("imgui.pushStyleColor", 9, 3, 3, 3, 255)
    mirageFunc("imgui.pushStyleColor", 5, 145, 30, 45, 255)
    mirageFunc("imgui.pushStyleColor", 14, 24, 8, 10, 188)
    mirageFunc("imgui.pushStyleColor", 15, 104, 24, 36, 240)
    mirageFunc("imgui.pushStyleColor", 16, 158, 34, 52, 250)
    mirageFunc("imgui.pushStyleColor", 17, 214, 44, 66, 255)
    mirageFunc("imgui.pushStyleVarFloat", 13, 1.6)
    last_code = mirageFunc("imgui.codeEditorLua", "##inject_code_editor", last_code, codeW, codeH, ui.codeKey) or last_code

    mirageFunc("imgui.popStyleVar", 1)
    mirageFunc("imgui.popStyleColor", 9)

    mirageFunc("imgui.setCursorPos", 14, ui.height - 72)
    mirageFunc("imgui.pushFont", ui.keyButtonFont)
    if mirageFunc("imgui.gradientButton", "Inject VM", 230, 58, 72, 14, 24, 245, 228, 48, 70, 255, 11, "inj_btn_inject") then
        injectCode(last_code)
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Clear", 170, 58, 64, 13, 20, 242, 192, 40, 62, 255, 11, "inj_btn_clear") then
        last_code = ""
        mirageFunc("imgui.setString", ui.codeKey, "")
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Panic Mode", 230, 58, 92, 18, 24, 246, 186, 38, 56, 255, 11, "inj_btn_panic") then
        if activatePanicMode then
            activatePanicMode()
        end
    end
    mirageFunc("imgui.popFont")
end

local function renderEventsTab(contentY)
    local logW = ui.width - 30
    local logH = ui.height - contentY - 95
    if logH < 340 then logH = 340 end

    mirageFunc("imgui.setCursorPos", 14, contentY)
    mirageFunc("imgui.pushStyleColor", 0, 232, 222, 222, 255)
    local alwaysVScrollbar = 16384 -- ImGuiWindowFlags_AlwaysVerticalScrollbar
    mirageFunc("imgui.beginChild", "events_log_host", logW, logH, true, alwaysVScrollbar, "events_log_host")

    local first = math.max(1, #eventLines - RENDER_MAX_LINES)
    for i = first, #eventLines do
        pushEventLineColor(eventLines[i])
        mirageFunc("imgui.text", eventLines[i])
        popEventLineColor()
    end

    mirageFunc("imgui.endChild")
    mirageFunc("imgui.popStyleColor", 1)

    mirageFunc("imgui.setCursorPos", 14, ui.height - 72)
    mirageFunc("imgui.pushFont", ui.keyButtonFont)
    if mirageFunc("imgui.gradientButton", "Copy Last", 170, 58, 72, 14, 24, 245, 228, 48, 70, 255, 11, "ev_btn_copy_last") then
        copyToClipboard(eventLines[#eventLines], "Copied last event")
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Copy All", 170, 58, 72, 14, 24, 245, 228, 48, 70, 255, 11, "ev_btn_copy_all") then
        copyToClipboard(getEventLogText(), "Copied events log")
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Clear Log", 230, 58, 64, 13, 20, 242, 192, 40, 62, 255, 11, "ev_btn_clear") then
        clearEventLog()
    end
    mirageFunc("imgui.popFont")
end

local function renderThreadsTab(contentY)
    refreshLuaThreads()

    local listW = ui.width - 30
    local listH = ui.height - contentY - 95
    if listH < 340 then listH = 340 end

    mirageFunc("imgui.setCursorPos", 14, contentY)
    mirageFunc("imgui.pushStyleColor", 0, 232, 222, 222, 255)
    local alwaysVScrollbar = 16384 -- ImGuiWindowFlags_AlwaysVerticalScrollbar
    mirageFunc("imgui.beginChild", "threads_log_host", listW, listH, true, alwaysVScrollbar, "threads_log_host")

    if #luaThreads == 0 then
        mirageFunc("imgui.text", "No Lua threads loaded")
    else
        for i = 1, #luaThreads do
            local item = luaThreads[i]
            local row = "#" .. tostring(item.id) .. " | " .. tostring(item.resource) .. " | " .. tostring(item.loadedAt) .. " | " .. tostring(item.state)
            if selectedThreadId == item.id then
                row = "> " .. row
            end
            if mirageFunc("imgui.button", row, listW - 24, 26, "thr_btn_select_" .. tostring(item.id)) then
                selectedThreadId = item.id
            end
        end
    end

    mirageFunc("imgui.endChild")
    mirageFunc("imgui.popStyleColor", 1)

    mirageFunc("imgui.setCursorPos", 14, ui.height - 72)
    mirageFunc("imgui.pushFont", ui.keyButtonFont)
    if mirageFunc("imgui.gradientButton", "Refresh", 170, 58, 64, 13, 20, 242, 192, 40, 62, 255, 11, "thr_btn_refresh") then
        refreshLuaThreads()
    end
    mirageFunc("imgui.sameLine")
    local unloadLabel = selectedThreadId and ("Unload #" .. tostring(selectedThreadId)) or "Unload Selected"
    if mirageFunc("imgui.gradientButton", unloadLabel, 260, 58, 72, 14, 24, 245, 228, 48, 70, 255, 11, "thr_btn_unload") then
        if selectedThreadId then
            local ok = mirageFunc("luaThreadUnload", tostring(selectedThreadId))
            if ok and ok ~= false then
                notifySuccess("Lua thread unloaded: #" .. tostring(selectedThreadId))
                selectedThreadId = nil
                refreshLuaThreads()
            else
                notifyError("Failed to unload Lua thread")
            end
        else
            notifyWarning("Select a Lua thread first")
        end
    end
    mirageFunc("imgui.popFont")
end

local function renderLuftwaffeTab(contentY)
    local bufferedFilterId = mirageFunc("imgui.getString", ui.luftwaffeFilterKey, luftwaffeFilterId) or luftwaffeFilterId
    if bufferedFilterId ~= luftwaffeFilterId then
        luftwaffeFilterId = bufferedFilterId
        refreshLuftwaffeDrivers()
    end

    mirageFunc("imgui.setCursorPos", 14, contentY)
    mirageFunc("imgui.pushFont", ui.keyButtonFont)
    if mirageFunc("imgui.gradientButton", "Select Vehicle", 200, 52, 72, 14, 24, 245, 228, 48, 70, 255, 11, "luft_btn_pick_vehicle") then
        chooseFenrirVehicle()
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Refresh List", 210, 52, 72, 14, 24, 245, 228, 48, 70, 255, 11, "luft_btn_refresh") then
        refreshLuftwaffeDrivers()
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Cat", 120, 52, 72, 14, 24, 245, 228, 48, 70, 255, 11, "luft_btn_cat") then
        setElementModel(localPlayer, 6742)
    end
    mirageFunc("imgui.sameLine")
    local specLabel = luftwaffeSpectating and "Return Camera" or "Spectate"
    if mirageFunc("imgui.gradientButton", specLabel, 190, 52, 64, 13, 20, 242, 192, 40, 62, 255, 11, "luft_btn_spec") then
        toggleLuftwaffeSpectate()
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Astalavista Baby", 230, 52, 92, 18, 24, 246, 186, 38, 56, 255, 11, "luft_btn_astalavista") then
        performAstalavistaBaby()
    end
    mirageFunc("imgui.popFont")

    local selectedVehicleLabel = "Не выбрана"
    if isElement(fenrirVehicle) then
        local distance = getDistanceToElement2D(fenrirVehicle)
        selectedVehicleLabel = "Выбрана тачка: " .. tostring(fenrirVehicle) .. " | дистанция: " .. tostring(distance)
    end

    mirageFunc("imgui.setCursorPos", 14, contentY + 60)
    mirageFunc("imgui.text", selectedVehicleLabel)

    local selectedPlayerLabel = "Игрок не выбран"
    if isElement(luftwaffeSelectedPlayer) then
        selectedPlayerLabel = "Выбран игрок: " .. getPlayerDisplayName(luftwaffeSelectedPlayer) .. " (" .. tostring(luftwaffeSelectedPid or "?") .. ")"
    end
    mirageFunc("imgui.setCursorPos", 14, contentY + 84)
    mirageFunc("imgui.text", selectedPlayerLabel)

    local listY = contentY + 116
    local listW = ui.width - 30
    local listH = ui.height - listY - 118
    if listH < 280 then
        listH = 280
    end

    mirageFunc("imgui.setCursorPos", 14, listY)
    mirageFunc("imgui.pushStyleColor", 0, 232, 222, 222, 255)
    local alwaysVScrollbar = 16384
    mirageFunc("imgui.beginChild", "luftwaffe_driver_host", listW, listH, true, alwaysVScrollbar, "luftwaffe_driver_host")

    if #luftwaffeDrivers == 0 then
        mirageFunc("imgui.text", "Водители не найдены")
    else
        for i = 1, #luftwaffeDrivers do
            local entry = luftwaffeDrivers[i]
            local row = entry.label
            if luftwaffeSelectedPlayer == entry.player then
                row = "> " .. row
            end

            if mirageFunc("imgui.button", row, listW - 24, 26, "luft_driver_" .. tostring(i)) then
                luftwaffeSelectedPlayer = entry.player
                luftwaffeSelectedPid = entry.pid
            end
        end
    end

    mirageFunc("imgui.endChild")
    mirageFunc("imgui.popStyleColor", 1)

    mirageFunc("imgui.setCursorPos", 14, ui.height - 74)
    luftwaffeFilterId = mirageFunc("imgui.inputText", "ID игрока", luftwaffeFilterId, 24, 0, ui.luftwaffeFilterKey) or luftwaffeFilterId
end

local function renderUnifiedUi()
    if not isGUIOpen or block_dumper then return end
    if not mirageFunc("imgui.isReady") then return end

    local sx, sy = guiGetScreenSize()
    ui.width = math.max(1160, math.floor(sx * 0.68))
    ui.height = math.max(820, math.floor(sy * 0.80))
    ui.bannerW = ui.width - 28
    ui.bannerH = math.floor(ui.bannerW * 0.19)
    if ui.bannerH < 190 then ui.bannerH = 190 end
    if ui.bannerH > 240 then ui.bannerH = 240 end

    local defaultX = math.floor((sx - ui.width) * 0.5)
    local defaultY = math.floor((sy - ui.height) * 0.5)

    ui.winX = mirageFunc("imgui.getFloat", "wnd_mirage_unified#x", ui.winX or defaultX) or (ui.winX or defaultX)
    ui.winY = mirageFunc("imgui.getFloat", "wnd_mirage_unified#y", ui.winY or defaultY) or (ui.winY or defaultY)
    local x = math.floor(ui.winX)
    local y = math.floor(ui.winY)

    if not ui.posInitialized then
        mirageFunc("imgui.setNextWindowPos", x, y, 0)
        ui.posInitialized = true
    end
    mirageFunc("imgui.setNextWindowSize", ui.width, ui.height, 0)
    drawSmokyBackdrop(x, y, ui.width, ui.height)

    pushMainTheme()

    local noTitleBar = 1 -- ImGuiWindowFlags_NoTitleBar
    local opened = mirageFunc("imgui.begin", "##mirage_unified", false, noTitleBar, "wnd_mirage_unified")
    if opened then
        local caption = "Mirage Injector V6.6 by DroidZero"
        local bannerY = 54
        drawCaptionTitle(x, y, caption, y + bannerY)

        mirageFunc("imgui.setCursorPos", ui.width - 42, 14)
        if mirageFunc("imgui.gradientButton", "X", 24, 20, 108, 20, 32, 246, 186, 38, 56, 255, 6, "wnd_btn_close") then
            setUiVisible(false)
        end

        mirageFunc("imgui.setCursorPos", 14, bannerY)
        mirageFunc("imgui.image", ui.keyBanner, ui.bannerW, ui.bannerH, 0, 0, 1, 1, 255, 255, 255, 255)

        local tabY = bannerY + ui.bannerH + 2
        local contentY = tabY + 52

        mirageFunc("imgui.setCursorPos", 14, tabY)
        pushTabTheme()
        if mirageFunc("imgui.beginTabBar", "mirage_main_tabs", 0, "mirage_main_tabs") then
            local injectorTab = mirageFunc("imgui.beginTabItem", "Lua Injector", 0, false, "tab_injector")
            if injectorTab then
                currentTab = "injector"
                renderInjectorTab(contentY)
                mirageFunc("imgui.endTabItem")
            end

            local eventsTab = mirageFunc("imgui.beginTabItem", "Events Dumper", 0, false, "tab_events")
            if eventsTab then
                currentTab = "events"
                renderEventsTab(contentY)
                mirageFunc("imgui.endTabItem")
            end

            local threadsTab = mirageFunc("imgui.beginTabItem", "Lua Threads", 0, false, "tab_threads")
            if threadsTab then
                currentTab = "threads"
                renderThreadsTab(contentY)
                mirageFunc("imgui.endTabItem")
            end

            local luftwaffeTab = mirageFunc("imgui.beginTabItem", "Luftwaffe", 0, false, "tab_luftwaffe")
            if luftwaffeTab then
                currentTab = "luftwaffe"
                renderLuftwaffeTab(contentY)
                mirageFunc("imgui.endTabItem")
            end

            mirageFunc("imgui.endTabBar")
        else
            -- Safety fallback if tab bar is not ready this frame.
            renderInjectorTab(contentY)
        end
        popTabTheme()
    else
        setUiVisible(false)
    end

    mirageFunc("imgui.end")
    popMainTheme()
end

local function isMirageBlocked()
    return getElementData(localPlayer, "antiMirage") or false
end

local function ToggleGUI()
    if isGUIOpen then
        setUiVisible(false)
    else
        if isMirageBlocked() then return end
        setUiVisible(true)
        currentTab = "injector"
        targetResourceName = selfResource or targetResourceName
        mirageFunc("imgui.setString", ui.resourceKey, targetResourceName)
        mirageFunc("imgui.setString", ui.luftwaffeFilterKey, luftwaffeFilterId)
        refreshLuaThreads()
        refreshLuftwaffeDrivers()
    end
end

local function stopImGuiUi()
    setUiVisible(false)
    if uiRenderHookActive then
        removeEventHandler("onClientRender", root, renderUnifiedUi)
        uiRenderHookActive = false
    end
    if ui.assetsReady then
        mirageFunc("imgui.captureInput", false)
        mirageFunc("imgui.blockBinds", false)
        mirageFunc("imgui.forceCursor", false)
        mirageFunc("imgui.drawCursor", false)
        mirageFunc("imgui.enable", false)
    end
end

activatePanicMode = function()
    playSoundFrontEnd(11)
    block_dumper = true
    stopImGuiUi()
    mirageFunc("antiMirage")
    mirageFunc("hideFunctionCall", true)
    setElementData(localPlayer, "antiMirage", true, false)
    mirageFunc("hideFunctionCall", false)
end

_G.MirageToggleInjectorMenu = ToggleGUI
_G.MirageCloseInjectorMenu = function() setUiVisible(false) end

setupImGuiAssets()
setUiVisible(false)
mirageFunc("imgui.setString", ui.resourceKey, targetResourceName)
refreshLuaThreads()
if isDebugViewActive then
    local ok, currentFlag = pcall(isDebugViewActive)
    if ok and type(currentFlag) == "boolean" then
        debugModeEnabled = currentFlag
    end
end

mirageFunc("hideFunctionCall", true)
mirageFunc("setDbgHook", "preFunction", evDumper, { "triggerServerEvent", "triggerLatentServerEvent" })
mirageFunc("hideFunctionCall", false)

bindKey("F1", "down", ToggleGUI)
if not uiRenderHookActive then
    addEventHandler("onClientRender", root, renderUnifiedUi)
    uiRenderHookActive = true
end

addEventHandler("onClientKey", root, function(button, press)
    if not isGUIOpen then return end
    if not press then return end
    if button == "escape" then
        cancelEvent()
        setUiVisible(false)
    end
end)
