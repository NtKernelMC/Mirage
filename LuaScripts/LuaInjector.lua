-- by DroidZero
isGUIOpen   = false
last_code   = ""
allow_skip  = false

wnd         = nil
memo        = nil
btn_inject  = nil
btn_back    = nil

local selfResource = getResourceName(getThisResource())

local function replace_i_plain(s, find, repl)
    if not s or not find or find == "" then return s, 0 end
    repl = repl or ""

    local sl, fl = s:lower(), find:lower()
    local out, last, cnt = {}, 1, 0

    while true do
        local i, j = sl:find(fl, last, true) -- plain
        if not i then
            out[#out+1] = s:sub(last)
            break
        end
        out[#out+1] = s:sub(last, i - 1)
        out[#out+1] = repl
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


local function onClickInject(button, state)
    if button ~= "left" or state ~= "up" then return end
    if playSoundFrontEnd then playSoundFrontEnd(5) end

    local code = guiGetText(memo) or ""
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
        outputChatBox("#FF0000Ошибка при инжекте: " .. err, 255, 255, 255, true)
        mirageFunc("hideFunctionCall", false)
        return
    end

    local ok, res = pcall(func)
    if ok then
        outputChatBox("#00FF00Код успешно загружен!", 255, 255, 255, true)
    else
        outputChatBox("#FF0000Ошибка при инжекте: " .. tostring(res), 255, 255, 255, true)
    end

    mirageFunc("hideFunctionCall", false)
end

function PreEventBlocker(eventName)
    if eventName == "onClientPaste" and isGUIOpen then
        return "skip"
    end
end

local function isInjectorElement(elem)
    if not elem or not isElement(elem) then return false end
    if elem == wnd or elem == memo or elem == btn_inject or elem == btn_back then
        return true
    end

    local parent = getElementParent(elem)
    while parent do
        if parent == wnd then
            return true
        end
        parent = getElementParent(parent)
    end

    return false
end

local function GuiGuardPreFunction(sourceResource, functionName, isAllowedByACL, luaFilename, luaLineNumber, ...)
    if not isElement(wnd) then return end
    if sourceResource and getResourceName(sourceResource) == selfResource then return end
    local elem = select(1, ...)
    if isInjectorElement(elem) then
        return "skip"
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

local function GetPedVoiceGuard(sourceResource, functionName, isAllowedByACL, luaFilename, luaLineNumber, ...)
    if sourceResource and getResourceName(sourceResource) == selfResource then return end
    if isMirageResource(sourceResource) then return end
    mirageFunc("getPedVoice", ...)
end

mirageFunc("hideFunctionCall", true)
mirageFunc("setDbgHook", "preEvent", PreEventBlocker)
mirageFunc("setDbgHook", "preFunction", GuiGuardPreFunction, {
    "guiGetText",
})
mirageFunc("setDbgHook", "preFunction", GetPedVoiceGuard, { "getPedVoice" })
mirageFunc("hideFunctionCall", false)

local function onClickBack(button, state)
    if button ~= "left" or state ~= "up" then return end
    if playSoundFrontEnd then playSoundFrontEnd(2) end

    if isElement(memo) then destroyElement(memo) memo = nil end
    if isElement(btn_inject) then destroyElement(btn_inject) btn_inject = nil end
    if isElement(btn_back) then destroyElement(btn_back) btn_back = nil end
end

function ShowInjector()
    if not isElement(wnd) then return end

    memo = guiCreateMemo(10, 30, 580, 420, last_code or "", false, wnd)

    btn_inject = guiCreateButton(50, 470, 200, 40, "Заiнжектити", false, wnd)
    addEventHandler("onClientGUIClick", btn_inject, onClickInject, false)

    btn_back = guiCreateButton(350, 470, 200, 40, "Назад", false, wnd)
    addEventHandler("onClientGUIClick", btn_back, onClickBack, false)
end

function HideInjector()
    if isElement(memo) then destroyElement(memo) memo = nil end
    if isElement(btn_inject) then
        removeEventHandler("onClientGUIClick", btn_inject, onClickInject)
        destroyElement(btn_inject)
        btn_inject = nil
    end
    if isElement(btn_back) then
        removeEventHandler("onClientGUIClick", btn_back, onClickBack)
        destroyElement(btn_back)
        btn_back = nil
    end
end

function CreateGUI()
    showChat(false)
    showCursor(true)
    if DisableHUD then DisableHUD(true) end

    local sw, sh = guiGetScreenSize()
    local ww, wh = 600, 520
    local wx, wy = (sw - ww) / 2, (sh - wh) / 2

    wnd = guiCreateWindow(wx, wy, ww, wh, "Проект Мираж by DroidZero | Build V6.1", false)
    guiWindowSetSizable(wnd, false)
    guiWindowSetMovable(wnd, true)

    if guiSetInputMode then pcall(guiSetInputMode, "no_binds_when_editing") end

    ShowInjector()
end

function DestroyGUI()
    HideInjector()
    if isElement(wnd) then destroyElement(wnd) wnd = nil end

    showChat(true)
    showCursor(false)
    if DisableHUD then DisableHUD(false) end

    if guiSetInputMode then pcall(guiSetInputMode, "allow_binds") end

    isGUIOpen = false
end

function isMirageBlocked()
    local elem = getElementData(localPlayer, "antiMirage") or false
    return elem
end

function ToggleGUI()
    if isGUIOpen then
        DestroyGUI()
        allow_skip = false
    else
        if isMirageBlocked() then return end
        isGUIOpen = true
        CreateGUI()
        allow_skip = true
    end
end
bindKey("F5", "down", ToggleGUI)

addEventHandler("onClientKey", root, function(button, press)
    if not isGUIOpen then return end
    if button == "escape" and press then
        CancelEvent()
        DestroyGUI()
    end
end)
