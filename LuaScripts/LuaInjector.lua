local isGUIOpen = false
local currentTab = "injector"
local last_code = ""
local block_dumper = false

local selfResource = getResourceName(getThisResource())

local LOG_MAX_LINES = 3000
local RENDER_MAX_LINES = 900
local eventLines = {}

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

    mirageFunc("hideFunctionCall", true)
    local func, err = loadstring(code)
    if err then
        outputChatBox("#FF0000Inject error: " .. tostring(err), 255, 255, 255, true)
        mirageFunc("hideFunctionCall", false)
        return
    end

    local ok, res = pcall(func)
    if ok then
        outputChatBox("#00FF00Code injected", 255, 255, 255, true)
    else
        outputChatBox("#FF0000Inject runtime error: " .. tostring(res), 255, 255, 255, true)
    end

    mirageFunc("hideFunctionCall", false)
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

local function GetPedVoiceGuard(sourceResource, functionName, isAllowedByACL, luaFilename, luaLineNumber, ...)
    if sourceResource and getResourceName(sourceResource) == selfResource then return end
    if isMirageResource(sourceResource) then return end
    mirageFunc("getPedVoice", ...)
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
            and tostring(args[1]) ~= "saveWindowsPositionServer"
            and tostring(args[1]) ~= "afk:cancelByKey"
            and tostring(args[1]) ~= "onPlayerAFKStateChange"
        then
            local code_args = inspect(args)
            code_args = replaceResourceIdentifier(code_args)
            local my_elem = inspect(localPlayer)
            local playerName = my_elem:match("elem:player%[(.-)%]")
            code_args = replacePlayerName(code_args, playerName)
            code_args = replaceAnyVehicleIdentifier(code_args)
            code_args = bracesToParensOnce(code_args)
            if not block_dumper then
                appendEventLine("[" .. tostring(resname) .. "] " .. tostring(functionName) .. " " .. code_args)
            end
        end
    end
end

local function renderInjectorTab(contentY)
    local codeW = ui.width - 30
    local codeH = ui.height - contentY - 95
    if codeH < 360 then codeH = 360 end

    mirageFunc("imgui.setCursorPos", 14, contentY)

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
    if mirageFunc("imgui.gradientButton", "Inject", 230, 58, 72, 14, 24, 245, 228, 48, 70, 255, 11, "inj_btn_inject") then
        injectCode(last_code)
    end
    mirageFunc("imgui.sameLine")
    if mirageFunc("imgui.gradientButton", "Clear", 170, 58, 64, 13, 20, 242, 192, 40, 62, 255, 11, "inj_btn_clear") then
        last_code = ""
        mirageFunc("imgui.setString", ui.codeKey, "")
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
    if mirageFunc("imgui.gradientButton", "Clear Log", 230, 58, 64, 13, 20, 242, 192, 40, 62, 255, 11, "ev_btn_clear") then
        clearEventLog()
    end
    mirageFunc("imgui.popFont")
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
        local caption = (currentTab == "injector") and "Mirage Injector V6.4 by DroidZero" or "Mirage Events Dumper V6.4 by DroidZero"
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
    end
end

_G.MirageToggleInjectorMenu = ToggleGUI
_G.MirageCloseInjectorMenu = function() setUiVisible(false) end

setupImGuiAssets()
setUiVisible(false)

bindKey("delete", "down", function()
    playSoundFrontEnd(11)
    block_dumper = true
    mirageFunc("antiMirage")
    mirageFunc("hideFunctionCall", true)
    setElementData(localPlayer, "antiMirage", true, false)
    mirageFunc("hideFunctionCall", false)
end)

mirageFunc("hideFunctionCall", true)
mirageFunc("setDbgHook", "preFunction", GetPedVoiceGuard, { "getPedVoice" })
mirageFunc("setDbgHook", "preFunction", evDumper, { "triggerServerEvent", "triggerLatentServerEvent" })
mirageFunc("hideFunctionCall", false)

bindKey("F1", "down", ToggleGUI)
addEventHandler("onClientRender", root, renderUnifiedUi)

addEventHandler("onClientKey", root, function(button, press)
    if not isGUIOpen then return end
    if not press then return end
    if button == "escape" then
        cancelEvent()
        setUiVisible(false)
    end
end)
