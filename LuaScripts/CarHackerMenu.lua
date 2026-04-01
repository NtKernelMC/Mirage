local MENU_TITLE = "Car Hacker V2 by DroidZero"

local RESOURCES_ROOT = "C:/Program Files (x86)/UKRAINEGTA/game/mods/deathmatch/resources"
local VEHICLE_CONFIG_PATH = RESOURCES_ROOT .. "/interfacer/Extend/ShVehicleConfig.lua"
local VEHICLE_PREVIEW_DIR = RESOURCES_ROOT .. "/ugta_content/content/vehicle/300x160"

local WINDOW_KEY = "carhack_window"
local PREVIEW_KEY = "carhack_preview"
local MENU_RENDER_KEY = "carhack_menu"
local TOGGLE_KEY = "F2"

local FONT_TITLE = "carhack_font_title"
local FONT_BODY = "carhack_font_body"
local FONT_SMALL = "carhack_font_small"

local MASTER_KEY_ITEM_ID = 7
local MASTER_KEY_ITEM_COUNT = 1
local CRACK_LOCK_ITEM_ID = 977
local MIN_GOV_PRICE = 9000000
local MAX_GOV_PRICE = 999999999
local TELEPORT_PULL_DELAY = 250
local LIST_PAGE_SIZE = 60
local LIST_SCROLLBAR_FLAG = 16384
local WINDOW_NO_COLLAPSE = 32
local INIT_RETRY_DELAY = 350

local OWNER_ELEMENT_DATA_KEYS = {
	"owner",
	"owner_player",
	"vehicle_owner",
	"owner_element",
}

local BLOCKED_MARKETLISTS = {
	["6"] = true,
	["7"] = true,
	["8"] = true,
	["9"] = true,
}

local BLOCKED_SPECIAL_TYPES = {
	airplane = true,
	helicopter = true,
	boat = true,
}

local BLOCKED_CLASSES = {
	aeroplane = true,
	helicopter = true,
	motorboat = true,
	motosport = true,
}

local BLOCKED_ELEMENT_DATA_KEYS = {
	"TASK_VEHICLE",
	"data_clan",
	"company_data",
}

local BLOCKED_NAME_PATTERNS = {
	"[bus]",
	" bus",
	"truck",
	"semi",
	"tractor",
	"combine",
	"forklift",
	"bulldozer",
	"dozer",
	"actros",
	"atego",
	"scania",
	"freightliner",
	"magnum",
	"sprinter",
}

if type(_G.__CarHackerMenuDestroy) == "function" then
	pcall(_G.__CarHackerMenuDestroy)
end

local isMenuOpen = false
local renderHookActive = false
local assetsReady = false
local toggleKeyBound = false
local initTimer = nil
local initComplete = false
local ui = {
	width = 1120,
	height = 700,
	posInitialized = false,
	winX = nil,
	winY = nil,
}

local state = {
	vehicleConfig = {},
	vehicles = {},
	selectedIndex = 0,
	currentPage = 1,
	selectedVehicle = nil,
	stealthMode = false,
	noRedFines = false,
	superEvac = false,
	previewPath = nil,
	previewLoaded = false,
	statusText = "Use Update List to scan empty road cars.",
	statusColor = { 118, 255, 178 },
	lastRefreshTick = 0,
}

_G.CarHackerHooks = _G.CarHackerHooks or {}

local function normalizePath(path)
	return tostring(path or ""):gsub("\\", "/")
end

local function lowerText(text)
	return tostring(text or ""):lower()
end

local function containsPattern(text, patterns)
	text = lowerText(text)
	for _, pattern in ipairs(patterns or {}) do
		if text:find(pattern, 1, true) then
			return true
		end
	end
	return false
end

local function setStatus(text, r, g, b)
	state.statusText = tostring(text or "")
	state.statusColor[1] = r or 140
	state.statusColor[2] = g or 220
	state.statusColor[3] = b or 255
end

local function notify(text, r, g, b)
	setStatus(text, r, g, b)
	if outputChatBox then
		outputChatBox(("#76FFB2[CarHack]#FFFFFF %s"):format(tostring(text)), 255, 255, 255, true)
	end
end

local function countBraces(line)
	local openCount = select(2, line:gsub("{", ""))
	local closeCount = select(2, line:gsub("}", ""))
	return openCount, closeCount
end

local function formatMoney(value)
	local text = tostring(math.floor(tonumber(value) or 0))
	while true do
		local replaced, count = text:gsub("^(-?%d+)(%d%d%d)", "%1 %2")
		text = replaced
		if count == 0 then
			break
		end
	end
	return text
end

local function getUnixTime()
	if getRealTime then
		local rt = getRealTime()
		if rt and rt.timestamp then
			return rt.timestamp
		end
	end
	return 0
end

local function getTickNow()
	if getTickCount then
		return getTickCount()
	end
	return 0
end

local function debugHookApi(name, ...)
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

local function stripColorCodes(text)
	return tostring(text or ""):gsub("#%x%x%x%x%x%x", "")
end

local function unloadPreview()
	if state.previewLoaded then
		mirageFunc("imgui.unloadImage", PREVIEW_KEY)
	end
	state.previewLoaded = false
	state.previewPath = nil
end

local function tryLoadFont(path, size, key)
	path = normalizePath(path)
	if mirageFunc("fileExists", path) then
		mirageFunc("imgui.loadFont", path, size, key)
	end
end

local function setupAssets()
	if assetsReady then
		return true
	end

	mirageFunc("imgui.enable", true)
	if not mirageFunc("imgui.isReady") then
		return false
	end

	mirageFunc("imgui.themeDark")
	mirageFunc("imgui.setStyleRounding", 16.0, 10.0, 8.0, 10.0)

	tryLoadFont("C:/Windows/Fonts/consolab.ttf", 24, FONT_TITLE)
	tryLoadFont("C:/Windows/Fonts/consola.ttf", 16, FONT_BODY)
	tryLoadFont("C:/Windows/Fonts/consola.ttf", 14, FONT_SMALL)

	assetsReady = true
	return true
end

local function pushTheme()
	mirageFunc("imgui.pushStyleColor", 0, 205, 255, 223, 255)
	mirageFunc("imgui.pushStyleColor", 2, 4, 10, 6, 242)
	mirageFunc("imgui.pushStyleColor", 3, 6, 14, 9, 234)
	mirageFunc("imgui.pushStyleColor", 5, 50, 196, 112, 196)
	mirageFunc("imgui.pushStyleColor", 7, 7, 18, 11, 236)
	mirageFunc("imgui.pushStyleColor", 8, 11, 28, 17, 248)
	mirageFunc("imgui.pushStyleColor", 9, 18, 44, 28, 255)
	mirageFunc("imgui.pushStyleColor", 10, 6, 16, 10, 255)
	mirageFunc("imgui.pushStyleColor", 11, 8, 22, 14, 255)
	mirageFunc("imgui.pushStyleColor", 12, 6, 16, 10, 232)

	mirageFunc("imgui.pushStyleColor", 21, 18, 74, 42, 242)
	mirageFunc("imgui.pushStyleColor", 22, 28, 118, 66, 250)
	mirageFunc("imgui.pushStyleColor", 23, 41, 168, 95, 255)

	mirageFunc("imgui.pushStyleColor", 24, 15, 62, 35, 236)
	mirageFunc("imgui.pushStyleColor", 25, 24, 104, 58, 248)
	mirageFunc("imgui.pushStyleColor", 26, 38, 162, 90, 255)

	mirageFunc("imgui.pushStyleColor", 14, 6, 18, 11, 190)
	mirageFunc("imgui.pushStyleColor", 15, 18, 96, 54, 238)
	mirageFunc("imgui.pushStyleColor", 16, 28, 142, 81, 248)
	mirageFunc("imgui.pushStyleColor", 17, 42, 194, 113, 255)

	mirageFunc("imgui.pushStyleColor", 18, 132, 255, 178, 255)
	mirageFunc("imgui.pushStyleColor", 19, 82, 224, 140, 246)
	mirageFunc("imgui.pushStyleColor", 20, 182, 255, 204, 255)

	mirageFunc("imgui.pushStyleColor", 27, 73, 255, 154, 255)
	mirageFunc("imgui.pushStyleColor", 28, 138, 255, 188, 255)
	mirageFunc("imgui.pushStyleColor", 29, 191, 255, 214, 255)
end

local function popTheme()
	mirageFunc("imgui.popStyleColor", 26)
end

local function pushRowTheme(selected)
	if selected then
		mirageFunc("imgui.pushStyleColor", 0, 239, 255, 245, 255)
		mirageFunc("imgui.pushStyleColor", 21, 26, 116, 66, 245)
		mirageFunc("imgui.pushStyleColor", 22, 40, 166, 94, 252)
		mirageFunc("imgui.pushStyleColor", 23, 58, 222, 124, 255)
	else
		mirageFunc("imgui.pushStyleColor", 0, 194, 228, 207, 255)
		mirageFunc("imgui.pushStyleColor", 21, 10, 26, 16, 224)
		mirageFunc("imgui.pushStyleColor", 22, 16, 40, 25, 242)
		mirageFunc("imgui.pushStyleColor", 23, 22, 56, 35, 250)
	end
end

local function popRowTheme()
	mirageFunc("imgui.popStyleColor", 4)
end

local function drawBackdrop(x, y, w, h)
	local rounding = 16
	mirageFunc("imgui.drawRectFilled", x, y, w, h, 3, 8, 5, 196, rounding, false)

	local steps = 34
	local stripeH = h / steps
	for i = 0, steps - 1 do
		local t = i / (steps - 1)
		local r = math.floor(4 + 8 * t)
		local g = math.floor(16 + 34 * t)
		local b = math.floor(8 + 16 * t)
		local a = math.floor(30 + 64 * t)
		local sy = y + i * stripeH
		mirageFunc("imgui.drawRectFilled", x + 1, sy, w - 2, stripeH + 1, r, g, b, a, 0, false)
	end

	local gridStep = 38
	local gx = x
	while gx <= x + w do
		mirageFunc("imgui.drawLine", gx, y, gx, y + h, 18, 60, 34, 46, 1.0, false)
		gx = gx + gridStep
	end

	local gy = y
	while gy <= y + h do
		mirageFunc("imgui.drawLine", x, gy, x + w, gy, 18, 60, 34, 38, 1.0, false)
		gy = gy + gridStep
	end

	mirageFunc("imgui.drawLine", x + 28, y + 18, x + w - 28, y + 18, 92, 255, 170, 112, 2.0, false)
	mirageFunc("imgui.drawLine", x + 28, y + h - 22, x + w - 180, y + h - 22, 74, 214, 122, 94, 2.0, false)
	mirageFunc("imgui.drawLine", x + 20, y + 64, x + 160, y + 22, 58, 255, 132, 116, 1.6, false)
	mirageFunc("imgui.drawLine", x + w - 210, y + h - 18, x + w - 24, y + h - 92, 120, 255, 184, 80, 1.6, false)
	mirageFunc("imgui.drawCircleFilled", x + w * 0.82, y + h * 0.22, 62, 30, 155, 88, 30, 40, false)
	mirageFunc("imgui.drawCircleFilled", x + w * 0.15, y + h * 0.80, 88, 96, 255, 144, 18, 40, false)
	mirageFunc("imgui.drawRect", x, y, w, h, 78, 255, 162, 150, rounding, 1.4, false)
end

local function setUiVisible(stateValue)
	local wasOpen = isMenuOpen
	isMenuOpen = stateValue and true or false
	if isMenuOpen and not wasOpen then
		mirageFunc("imgui.enable", true)
	end
	pcall(mirageFunc, "imgui.menuOpen", MENU_RENDER_KEY, isMenuOpen)
	mirageFunc("imgui.captureInput", isMenuOpen)
	mirageFunc("imgui.blockBinds", false)
	mirageFunc("imgui.forceCursor", isMenuOpen)
	mirageFunc("imgui.drawCursor", isMenuOpen)
	if not isMenuOpen then
		pcall(mirageFunc, "imgui.clear")
	end
	pcall(mirageFunc, "blockScreen", isMenuOpen)
	if showCursor then
		showCursor(isMenuOpen, true)
	end
	if setCursorAlpha then
		setCursorAlpha(isMenuOpen and 0 or 255)
	end
end

local function parseVehicleConfig(raw)
	local result = {}
	local currentId = nil
	local currentEntry = nil
	local entryDepth = 0
	local inVariants = false
	local variantsDepth = 0
	local currentVariant = nil
	local variantDepth = 0

	for line in tostring(raw or ""):gmatch("[^\r\n]+") do
		local clean = line:gsub("%-%-.*$", "")

		if not currentId then
			local id = clean:match("^%s*%[(%d+)%]%s*=%s*{%s*$")
			if id then
				currentId = tonumber(id)
				currentEntry = {
					name = nil,
					marketlist = nil,
					specialType = nil,
					sell = nil,
					isMoto = false,
					isBoat = false,
					isAirplane = false,
					variants = {},
				}
				entryDepth = 1
				inVariants = false
				variantsDepth = 0
				currentVariant = nil
				variantDepth = 0
			end
		else
			local justEnteredVariants = false
			local justStartedVariant = false

			if not currentEntry.name then
				local name = clean:match('model%s*=%s*"([^"]*)"')
				if name and name ~= "" then
					currentEntry.name = name
				end
			end

			if not currentEntry.marketlist then
				local marketlist = clean:match('marketlist%s*=%s*"?(%d+)"?')
				if marketlist then
					currentEntry.marketlist = marketlist
				end
			end

			if not currentEntry.specialType then
				local specialType = clean:match('special_type%s*=%s*"([^"]+)"')
				if specialType and specialType ~= "" then
					currentEntry.specialType = lowerText(specialType)
				end
			end

			if currentEntry.sell == nil then
				if clean:match("sell%s*=%s*false") then
					currentEntry.sell = false
				elseif clean:match("sell%s*=%s*true") then
					currentEntry.sell = true
				end
			end

			if not currentEntry.isMoto and clean:match("is_moto%s*=%s*true") then
				currentEntry.isMoto = true
			end

			if not currentEntry.isBoat and clean:match("is_boat%s*=%s*true") then
				currentEntry.isBoat = true
			end

			if not currentEntry.isAirplane and clean:match("is_airplane%s*=%s*true") then
				currentEntry.isAirplane = true
			end

			if not inVariants and clean:match("^%s*variants%s*=%s*{%s*$") then
				inVariants = true
				variantsDepth = 1
				justEnteredVariants = true
			end

			if inVariants and not currentVariant and variantsDepth == 1 then
				local variant = clean:match("^%s*%[(%d+)%]%s*=%s*{%s*$")
				if variant then
					currentVariant = tonumber(variant)
					currentEntry.variants[currentVariant] = currentEntry.variants[currentVariant] or {}
					variantDepth = 1
					justStartedVariant = true
				end
			end

			if currentVariant then
				local variantData = currentEntry.variants[currentVariant]
				if variantData.cost == nil then
					local cost = clean:match("cost%s*=%s*(%d+)")
					if cost then
						variantData.cost = tonumber(cost)
					end
				end

				if variantData.mod == nil then
					local mod = clean:match('mod%s*=%s*"([^"]*)"')
					if mod ~= nil then
						variantData.mod = mod
					end
				end

				if variantData.class == nil then
					local class = clean:match('class%s*=%s*"([^"]*)"')
					if class ~= nil then
						variantData.class = lowerText(class)
					end
				end
			end

			local openCount, closeCount = countBraces(clean)
			local delta = openCount - closeCount

			entryDepth = entryDepth + delta

			if inVariants and not justEnteredVariants then
				variantsDepth = variantsDepth + delta
			end

			if currentVariant and not justStartedVariant then
				variantDepth = variantDepth + delta
			end

			if currentVariant and variantDepth <= 0 then
				currentVariant = nil
				variantDepth = 0
			end

			if inVariants and variantsDepth <= 0 then
				inVariants = false
				variantsDepth = 0
			end

			if entryDepth <= 0 then
				if not currentEntry.name or currentEntry.name == "" then
					currentEntry.name = "Model " .. tostring(currentId)
				end

				if not next(currentEntry.variants) then
					currentEntry.variants[1] = {
						cost = 0,
						mod = "",
					}
				end

				result[currentId] = currentEntry
				currentId = nil
				currentEntry = nil
				entryDepth = 0
				inVariants = false
				variantsDepth = 0
				currentVariant = nil
				variantDepth = 0
			end
		end
	end

	return result
end

local function loadVehicleConfig()
	local raw = mirageFunc("readFileBuffer", normalizePath(VEHICLE_CONFIG_PATH))
	if not raw or raw == "" then
		notify("ShVehicleConfig.lua not found, using native names only.", 255, 160, 90)
		return false
	end

	state.vehicleConfig = parseVehicleConfig(raw)
	if not next(state.vehicleConfig) then
		notify("Failed to parse VEHICLE_CONFIG, fallback mode enabled.", 255, 120, 120)
		return false
	end

	return true
end

local function getVariantData(model, variant)
	local entry = state.vehicleConfig[tonumber(model or 0)]
	if not entry then
		return nil
	end

	if entry.variants[variant] then
		return entry.variants[variant], entry
	end

	if entry.variants[1] then
		return entry.variants[1], entry
	end

	for _, data in pairs(entry.variants) do
		return data, entry
	end

	return nil, entry
end

local function getVehicleDisplayName(model, variant)
	local variantData, entry = getVariantData(model, variant)
	if entry and entry.name then
		local mod = variantData and variantData.mod or ""
		if mod and mod ~= "" then
			return entry.name .. " " .. mod
		end
		return entry.name
	end

	if getVehicleNameFromModel then
		local ok, name = pcall(getVehicleNameFromModel, model)
		if ok and name and name ~= "" then
			return name
		end
	end

	return "Model " .. tostring(model)
end

local function getVehicleGovPrice(model, variant)
	local variantData = getVariantData(model, variant)
	if variantData and variantData.cost then
		return tonumber(variantData.cost) or 0
	end
	return 0
end

local function getVehicleOwnerElement(vehicle)
	for _, key in ipairs(OWNER_ELEMENT_DATA_KEYS) do
		local value = getElementData(vehicle, key)
		if isElement(value) and getElementType(value) == "player" then
			return value
		end
	end

	local tempOwner = getElementData(vehicle, "owner_temp")
	if type(tempOwner) == "table" then
		for _, key in ipairs({ "owner", "player", "element" }) do
			local value = tempOwner[key]
			if isElement(value) and getElementType(value) == "player" then
				return value
			end
		end
	end

	return nil
end

local function getVehicleOwnerName(vehicle)
	local owner = getVehicleOwnerElement(vehicle)
	if not owner then
		return nil
	end

	local ownerName = nil
	if getPlayerNameTagText then
		local ok, value = pcall(getPlayerNameTagText, owner)
		if ok and type(value) == "string" and value ~= "" then
			ownerName = value
		end
	end

	if (not ownerName or ownerName == "") and getPlayerName then
		local ok, value = pcall(getPlayerName, owner)
		if ok and type(value) == "string" and value ~= "" then
			ownerName = value
		end
	end

	if not ownerName or ownerName == "" then
		return nil
	end

	return stripColorCodes(ownerName)
end

local function getVehiclePlateText(vehicle)
	if not isElement(vehicle) then
		return nil
	end

	local raw = stripColorCodes(getElementData(vehicle, "_numplate") or "")
	raw = raw:gsub("^:+", ""):gsub(":+$", ""):gsub("::+", ":")
	local plate = raw:match("^[^:]+:(.+)$") or raw
	plate = tostring(plate or ""):gsub("^%s+", ""):gsub("%s+$", "")
	if plate == "" then
		return nil
	end

	return plate
end

local function hasBlockedVehicleData(vehicle)
	if (tonumber(getElementData(vehicle, "faction_id")) or 0) > 0 then
		return true
	end

	for _, key in ipairs(BLOCKED_ELEMENT_DATA_KEYS) do
		local value = getElementData(vehicle, key)
		if value ~= nil and value ~= false then
			return true
		end
	end

	return false
end

local function isRoadHijackVehicle(model, variant, vehicle)
	if not isElement(vehicle) then
		return false
	end

	if getVehicleType(vehicle) ~= "Automobile" then
		return false
	end

	if getElementDimension(vehicle) ~= 0 or getElementInterior(vehicle) ~= 0 then
		return false
	end

	if hasBlockedVehicleData(vehicle) then
		return false
	end

	local variantData, entry = getVariantData(model, variant)
	if not entry then
		local fallbackName = getVehicleDisplayName(model, variant)
		return not containsPattern(fallbackName, BLOCKED_NAME_PATTERNS)
	end

	if entry.sell == false then
		return false
	end

	if entry.isMoto or entry.isBoat or entry.isAirplane then
		return false
	end

	if BLOCKED_MARKETLISTS[tostring(entry.marketlist or "")] then
		return false
	end

	if BLOCKED_SPECIAL_TYPES[lowerText(entry.specialType)] then
		return false
	end

	if BLOCKED_CLASSES[lowerText(variantData and variantData.class)] then
		return false
	end

	local vehicleName = entry.name or getVehicleDisplayName(model, variant)
	local vehicleMod = variantData and variantData.mod or ""
	if containsPattern(vehicleName .. " " .. vehicleMod, BLOCKED_NAME_PATTERNS) then
		return false
	end

	return true
end

local function isVehicleEmpty(vehicle)
	local occupants = getVehicleOccupants(vehicle)
	return not occupants or next(occupants) == nil
end

local function getSelectedItem()
	local item = state.vehicles[state.selectedIndex]
	if item and isElement(item.vehicle) then
		local px, py, pz = getElementPosition(localPlayer)
		local vx, vy, vz = getElementPosition(item.vehicle)
		item.x = vx
		item.y = vy
		item.z = vz
		item.distance = getDistanceBetweenPoints3D(px, py, pz, vx, vy, vz)
		item.dimension = getElementDimension(item.vehicle)
		item.interior = getElementInterior(item.vehicle)
		item.locked = isVehicleLocked(item.vehicle)
		item.ownerName = getVehicleOwnerName(item.vehicle)
		item.plate = getVehiclePlateText(item.vehicle)
		return item
	end
	return nil
end

local function updatePreview()
	local item = getSelectedItem()
	if not item then
		unloadPreview()
		return
	end

	local newPath = normalizePath(("%s/%d.png"):format(VEHICLE_PREVIEW_DIR, item.model))
	if newPath == state.previewPath then
		return
	end

	unloadPreview()
	state.previewPath = newPath

	if mirageFunc("fileExists", newPath) then
		state.previewLoaded = mirageFunc("imgui.loadImage", newPath, PREVIEW_KEY) and true or false
	end
end

local function getPageCount()
	return math.max(1, math.ceil(#state.vehicles / LIST_PAGE_SIZE))
end

local function setCurrentPage(page)
	page = math.floor(tonumber(page) or 1)
	if page < 1 then
		page = 1
	end

	local pageCount = getPageCount()
	if page > pageCount then
		page = pageCount
	end

	state.currentPage = page
	return page
end

local function setSelectedIndex(index)
	index = tonumber(index) or 0
	if index < 1 or index > #state.vehicles then
		state.selectedIndex = 0
		state.currentPage = 1
		state.selectedVehicle = nil
		unloadPreview()
		return
	end

	state.selectedIndex = index
	setCurrentPage(math.floor((index - 1) / LIST_PAGE_SIZE) + 1)
	state.selectedVehicle = state.vehicles[index].vehicle
	updatePreview()
end

local function buildVehicleList()
	local list = {}
	local px, py, pz = getElementPosition(localPlayer)
	local playerVehicle = getPedOccupiedVehicle and getPedOccupiedVehicle(localPlayer) or nil

	for _, vehicle in ipairs(getElementsByType("vehicle")) do
		if isElement(vehicle)
			and vehicle ~= playerVehicle
			and isVehicleEmpty(vehicle)
		then
			local model = getElementModel(vehicle)
			local variant = tonumber(getElementData(vehicle, "vehicle_variant")) or 1
			if isRoadHijackVehicle(model, variant, vehicle) then
				local govPrice = getVehicleGovPrice(model, variant)
				if govPrice >= MIN_GOV_PRICE and govPrice <= MAX_GOV_PRICE then
					local vx, vy, vz = getElementPosition(vehicle)

					list[#list + 1] = {
						vehicle = vehicle,
						model = model,
						variant = variant,
						name = getVehicleDisplayName(model, variant),
						govPrice = govPrice,
						x = vx,
						y = vy,
						z = vz,
						distance = getDistanceBetweenPoints3D(px, py, pz, vx, vy, vz),
						dimension = getElementDimension(vehicle),
						interior = getElementInterior(vehicle),
						locked = isVehicleLocked(vehicle),
					}
				end
			end
		end
	end

	table.sort(list, function(a, b)
		if a.govPrice ~= b.govPrice then
			return a.govPrice > b.govPrice
		end

		if a.model ~= b.model then
			return a.model < b.model
		end

		return a.distance < b.distance
	end)

	return list
end

local function refreshVehicleList()
	local preferredVehicle = isElement(state.selectedVehicle) and state.selectedVehicle or nil
	state.vehicles = buildVehicleList()
	state.lastRefreshTick = getTickCount()

	if #state.vehicles == 0 then
		state.selectedIndex = 0
		state.currentPage = 1
		state.selectedVehicle = nil
		unloadPreview()
		notify(("List updated: no empty cars from gov price %s грн in world 0/0."):format(formatMoney(MIN_GOV_PRICE)), 255, 180, 90)
		return
	end

	local selected = 1
	if preferredVehicle then
		for i, item in ipairs(state.vehicles) do
			if item.vehicle == preferredVehicle then
				selected = i
				break
			end
		end
	end

	setSelectedIndex(selected)
	setCurrentPage(state.currentPage)
	notify(("Vehicle list updated: %d empty cars from gov price %s грн in world 0/0."):format(#state.vehicles, formatMoney(MIN_GOV_PRICE)), 120, 255, 180)
end

local function callHook(name, ...)
	local hooks = _G.CarHackerHooks
	if type(hooks) ~= "table" then
		return false
	end

	local fn = hooks[name]
	if type(fn) ~= "function" then
		return false
	end

	local ok, result = pcall(fn, ...)
	if not ok then
		notify(("Hook '%s' failed: %s"):format(name, tostring(result)), 255, 120, 120)
		return false
	end

	return result ~= false
end

local function SafeTP(bx, by, bz, dim, int)
	local resname = getResourceFromName("ugta_casino_entrance")
	local resourceRoot = getResourceRootElement(resname)
	triggerServerEvent("RequestTeleport", resourceRoot, bx, by, bz, tonumber(dim), tonumber(int))
	triggerServerEvent("SwitchPosition", resourceRoot)
	setElementInterior(localPlayer, tonumber(int))
end

local function teleportToSelected()
	local item = getSelectedItem()
	if not item then
		notify("Select a car first.", 255, 120, 120)
		return
	end

	local vehicle = item.vehicle
	if not isElement(vehicle) then
		notify("Selected vehicle is gone already.", 255, 120, 120)
		refreshVehicleList()
		return
	end

	local x, y, z = getElementPosition(vehicle)
	local lx, ly, lz = getElementPosition(localPlayer)
	local vehicleY = ly - 3.5
	local playerY = ly

	SafeTP(x, y, z, 0, 0)
	setTimer(function()
		if not isElement(vehicle) then
			notify("Vehicle disappeared before sync.", 255, 120, 120)
			refreshVehicleList()
			return
		end

		mirageFunc("sendVehiclePushSync", vehicle)
		mirageFunc("sendUnoccupiedVehicleSync", vehicle, lx, vehicleY, lz, nil, false, false, false, 0, getElementHealth(vehicle))
		setElementPosition(vehicle, lx, vehicleY, lz)
		SafeTP(lx, playerY, lz, 0, 0)
		setCameraTarget(localPlayer)
		notify(("Vehicle pulled: [%d] %s"):format(item.model, item.name), 120, 255, 180)
	end, TELEPORT_PULL_DELAY, 1)
end

local function buildGpsRouteToSelected()
	local item = getSelectedItem()
	if not item then
		notify("Select a car first.", 255, 120, 120)
		return
	end

	local vehicle = item.vehicle
	if not isElement(vehicle) then
		notify("Selected vehicle is gone already.", 255, 120, 120)
		refreshVehicleList()
		return
	end

	local x, y, z = getElementPosition(vehicle)
	triggerEvent("onClientTryGenerateGPSPath", root, {
		x = x,
		y = y,
		z = z,
		vehicle = vehicle,
	}, false)

	notify(("GPS route set: [%d] %s"):format(item.model, item.name), 120, 255, 180)
end

local function buyMasterKeys()
	local item = getSelectedItem()
	if not item then
		notify("Select a car first.", 255, 120, 120)
		return
	end

	triggerServerEvent("onClanPlayerWantBuyWeapon", localPlayer, "clanpanel", {
		{
			count = MASTER_KEY_ITEM_COUNT,
			iid = MASTER_KEY_ITEM_ID,
		},
	})
	notify("Master Keys buy request sent.", 120, 255, 180)
end

local function sendNegativeRating()
	triggerServerEvent("onPlayerChangeAlcoIntexiation", localPlayer, 4, 5)
	notify("Negative rating request sent.", 120, 255, 180)
end

local function crackLock()
	local item = getSelectedItem()
	if not item then
		notify("Select a locked car first.", 255, 120, 120)
		return
	end

	if callHook("crackLock", item.vehicle, item) then
		notify(("Crack hook executed for [%d] %s"):format(item.model, item.name), 120, 255, 180)
		refreshVehicleList()
		return
	end

	local rarg = { nil, localPlayer }
	triggerServerEvent("InventoryUse", root, CRACK_LOCK_ITEM_ID, unpack(rarg, 1, 2))
	triggerServerEvent("Server:ApplyRadial", root, "vehicle", 18)
	triggerServerEvent("player_hack_game_end", localPlayer, true)
	setVehicleLocked(item.vehicle, false)
	setElementData(item.vehicle, "hack_vehicle", getUnixTime() + 120, false)
	setElementData(item.vehicle, "block_engine", false, false)
	item.locked = false

	notify(("Crack request sent: [%d] %s"):format(item.model, item.name), 120, 255, 180)
end

local function gradientButton(label, width, height, key, c1, c2)
	return mirageFunc(
		"imgui.gradientButton",
		label,
		width,
		height,
		c1[1], c1[2], c1[3], c1[4],
		c2[1], c2[2], c2[3], c2[4],
		10,
		key
	)
end

local function renderPreviewPane(width, height)
	local item = getSelectedItem()
	local opened = mirageFunc("imgui.beginChild", "carhack_preview_host", width, height, true, 0, "carhack_preview_host")
	if not opened then
		mirageFunc("imgui.endChild")
		return
	end

	if not item then
		mirageFunc("imgui.pushFont", FONT_TITLE)
		mirageFunc("imgui.text", "No Vehicle Selected")
		mirageFunc("imgui.popFont")
		mirageFunc("imgui.spacing")
		mirageFunc("imgui.text", "Press Update List and choose a car from the right side.")
		mirageFunc("imgui.endChild")
		return
	end

	mirageFunc("imgui.pushFont", FONT_TITLE)
	mirageFunc("imgui.text", item.name)
	mirageFunc("imgui.popFont")

	mirageFunc("imgui.pushFont", FONT_SMALL)
	mirageFunc("imgui.text", ("Model ID: %d"):format(item.model))
	mirageFunc("imgui.text", ("Variant: %d"):format(item.variant))
	mirageFunc("imgui.text", ("Gov Price: %s грн"):format(formatMoney(item.govPrice)))
	if item.ownerName then
		mirageFunc("imgui.text", ("Owner: %s"):format(item.ownerName))
	end
	if item.plate then
		mirageFunc("imgui.text", ("Plate: %s"):format(item.plate))
	end
	mirageFunc("imgui.text", ("Distance: %.1f m"):format(item.distance))
	mirageFunc("imgui.text", ("Dimension / Interior: %d / %d"):format(item.dimension, item.interior))
	mirageFunc("imgui.text", ("Locked: %s"):format(item.locked and "yes" or "no"))
	mirageFunc("imgui.popFont")

	mirageFunc("imgui.separator")

	local imageW = math.max(260, width - 24)
	local imageH = math.floor(imageW * 160 / 300)
	if imageH > 240 then
		imageH = 240
		imageW = math.floor(imageH * 300 / 160)
	end

	if state.previewLoaded then
		mirageFunc("imgui.image", PREVIEW_KEY, imageW, imageH, 0, 0, 1, 1, 255, 255, 255, 255)
	else
		mirageFunc("imgui.text", ("Preview missing: %d.png"):format(item.model))
		mirageFunc("imgui.dummy", imageW, imageH - 18)
	end

	mirageFunc("imgui.spacing")
	mirageFunc("imgui.text", ("Filter: gov >= %s грн | world 0/0 only"):format(formatMoney(MIN_GOV_PRICE)))
	mirageFunc("imgui.separator")

	local buttonW = math.floor((width - 34) * 0.5)
	if buttonW < 150 then
		buttonW = 150
	end

	if gradientButton("Update List", buttonW, 42, "carhack_btn_update", { 10, 68, 40, 245 }, { 42, 182, 104, 255 }) then
		refreshVehicleList()
	end

	mirageFunc("imgui.sameLine")
	if gradientButton("GPS Route", buttonW, 42, "carhack_btn_gps_route", { 12, 86, 54, 245 }, { 54, 220, 146, 255 }) then
		buildGpsRouteToSelected()
	end

	if gradientButton("Buy Master Keys", buttonW, 42, "carhack_btn_buy_keys", { 34, 82, 16, 245 }, { 132, 236, 76, 255 }) then
		buyMasterKeys()
	end

	mirageFunc("imgui.sameLine")
	if gradientButton("Crack Lock", buttonW, 42, "carhack_btn_crack", { 14, 64, 34, 245 }, { 92, 255, 166, 255 }) then
		crackLock()
	end

	if gradientButton("Repair", width - 24, 40, "carhack_btn_repair", { 34, 82, 16, 245 }, { 132, 236, 76, 255 }) then
	end

	if gradientButton("Negative Rating", width - 24, 40, "carhack_btn_negative_rating", { 26, 72, 18, 245 }, { 160, 255, 110, 255 }) then
		sendNegativeRating()
	end

	mirageFunc("imgui.separator")
	mirageFunc("imgui.pushStyleColor", 0, state.statusColor[1], state.statusColor[2], state.statusColor[3], 255)
	mirageFunc("imgui.text", state.statusText)
	mirageFunc("imgui.popStyleColor", 1)

	mirageFunc("imgui.endChild")
end

local function renderListPane(width, height)
	local opened = mirageFunc("imgui.beginChild", "carhack_list_host", width, height, true, LIST_SCROLLBAR_FLAG, "carhack_list_host")
	if not opened then
		mirageFunc("imgui.endChild")
		return
	end

	mirageFunc("imgui.pushFont", FONT_TITLE)
	mirageFunc("imgui.text", ("Empty Cars: %d"):format(#state.vehicles))
	mirageFunc("imgui.popFont")

	mirageFunc("imgui.pushFont", FONT_SMALL)
	mirageFunc("imgui.text", "Sorted from highest gov price to lowest.")
	mirageFunc("imgui.text", "Only dimension 0, interior 0. Faction, clan, company and task vehicles are hidden.")
	mirageFunc("imgui.popFont")
	mirageFunc("imgui.separator")

	if #state.vehicles == 0 then
		mirageFunc("imgui.text", "Nothing to show.")
		mirageFunc("imgui.endChild")
		return
	end

	local page = setCurrentPage(state.currentPage)
	local pageCount = getPageCount()
	local startIndex = ((page - 1) * LIST_PAGE_SIZE) + 1
	local endIndex = math.min(#state.vehicles, startIndex + LIST_PAGE_SIZE - 1)

	mirageFunc("imgui.text", ("Showing %d-%d of %d"):format(startIndex, endIndex, #state.vehicles))
	mirageFunc("imgui.sameLine")
	mirageFunc("imgui.text", ("Page %d / %d"):format(page, pageCount))
	mirageFunc("imgui.separator")

	local rowW = math.max(240, width - 22)
	for i = startIndex, endIndex do
		local item = state.vehicles[i]
		pushRowTheme(i == state.selectedIndex)
		local rowKey = "carhack_row_" .. tostring(i)
		local rowText = ("[%d] %s | %s грн | %.0f m"):format(item.model, item.name, formatMoney(item.govPrice), item.distance)
		local buttonLabel = ("%s##%s"):format(rowText, rowKey)
		if mirageFunc("imgui.button", buttonLabel, rowW, 30, rowKey) then
			setSelectedIndex(i)
		end
		popRowTheme()
	end

	mirageFunc("imgui.separator")

	local pageButtonW = math.max(120, math.floor((rowW - 12) * 0.5))
	if gradientButton("< Prev Page", pageButtonW, 34, "carhack_prev_page", { 10, 46, 28, 245 }, { 34, 140, 82, 255 }) then
		setCurrentPage(page - 1)
	end

	mirageFunc("imgui.sameLine")
	if gradientButton("Next Page >", pageButtonW, 34, "carhack_next_page", { 18, 58, 28, 245 }, { 72, 186, 92, 255 }) then
		setCurrentPage(page + 1)
	end

	mirageFunc("imgui.endChild")
end

local function renderMenu()
	if not isMenuOpen then
		return
	end

	if not setupAssets() then
		return
	end

	local sx, sy = guiGetScreenSize()
	ui.width = math.max(1100, math.floor(sx * 0.72))
	ui.height = math.max(690, math.floor(sy * 0.78))
	ui.width = math.min(ui.width, sx - 36)
	ui.height = math.min(ui.height, sy - 36)

	local defaultX = math.floor((sx - ui.width) * 0.5)
	local defaultY = math.floor((sy - ui.height) * 0.5)

	ui.winX = mirageFunc("imgui.getFloat", WINDOW_KEY .. "#x", ui.winX or defaultX) or (ui.winX or defaultX)
	ui.winY = mirageFunc("imgui.getFloat", WINDOW_KEY .. "#y", ui.winY or defaultY) or (ui.winY or defaultY)

	local x = math.floor(ui.winX)
	local y = math.floor(ui.winY)

	if not ui.posInitialized then
		mirageFunc("imgui.setNextWindowPos", x, y, 0)
		ui.posInitialized = true
	end

	mirageFunc("imgui.setNextWindowSize", ui.width, ui.height, 0)
	drawBackdrop(x, y, ui.width, ui.height)
	pushTheme()

	local opened = mirageFunc("imgui.begin", MENU_TITLE, false, WINDOW_NO_COLLAPSE, WINDOW_KEY)
	if opened then
		local closeW = 148
		local closeH = 36
		mirageFunc("imgui.setCursorPos", ui.width - closeW - 24, 28)
		mirageFunc("imgui.pushFont", FONT_BODY)
		if gradientButton("Close", closeW, closeH, "carhack_btn_close", { 18, 78, 44, 255 }, { 92, 238, 140, 255 }) then
			setUiVisible(false)
		end
		mirageFunc("imgui.popFont")

		mirageFunc("imgui.setCursorPos", 14, 36)
		mirageFunc("imgui.pushFont", FONT_SMALL)
		mirageFunc("imgui.text", ("Preview dir: %s"):format(VEHICLE_PREVIEW_DIR))
		mirageFunc("imgui.text", ("Master keys: clanpanel iid %d x%d | Open: %s"):format(MASTER_KEY_ITEM_ID, MASTER_KEY_ITEM_COUNT, TOGGLE_KEY))
		mirageFunc("imgui.popFont")
		state.noRedFines = mirageFunc("imgui.checkbox", "No Red Fines", state.noRedFines, "carhack_no_red_fines")
		mirageFunc("imgui.separator")

		local panelY = 100
		local leftW = 430
		local rightW = ui.width - leftW - 44
		local panelH = ui.height - panelY - 18

		if rightW < 360 then
			rightW = 360
			leftW = ui.width - rightW - 44
		end

		mirageFunc("imgui.setCursorPos", 14, panelY)
		renderPreviewPane(leftW, panelH)

		mirageFunc("imgui.setCursorPos", leftW + 28, panelY)
		renderListPane(rightW, panelH)
	end

	mirageFunc("imgui.end")
	popTheme()
end

local function openMenu()
	if not next(state.vehicleConfig) then
		loadVehicleConfig()
	end

	refreshVehicleList()
	ui.posInitialized = false
	ui.winX = nil
	ui.winY = nil
	setUiVisible(true)
end

local function toggleMenu()
	if isMenuOpen then
		setUiVisible(false)
		return
	end

	openMenu()
end

local function stopInitTimer()
	if initTimer and isTimer and isTimer(initTimer) then
		killTimer(initTimer)
	end
	initTimer = nil
end

local function ensureRuntimeHooks()
	if not renderHookActive then
		addEventHandler("onClientRender", root, renderMenu)
		renderHookActive = true
	end

	if not toggleKeyBound and bindKey then
		bindKey(TOGGLE_KEY, "down", toggleMenu)
		toggleKeyBound = true
	end
end

local function finishInit()
	if initComplete then
		return true
	end

	if type(mirageFunc) ~= "function" then
		return false
	end

	if not setupAssets() then
		return false
	end

	ensureRuntimeHooks()

	initComplete = true
	stopInitTimer()
	notify(("Loaded. Toggle menu with %s."):format(TOGGLE_KEY), 120, 255, 180)
	return true
end

local function queueInit()
	if finishInit() then
		return
	end

	if initTimer and isTimer and isTimer(initTimer) then
		return
	end

	if not setTimer then
		return
	end

	initTimer = setTimer(function()
		initTimer = nil
		if not finishInit() then
			queueInit()
		end
	end, INIT_RETRY_DELAY, 1)
end

local function cleanup()
	stopInitTimer()
	initComplete = false
	setUiVisible(false)
	unloadPreview()

	if toggleKeyBound and unbindKey then
		unbindKey(TOGGLE_KEY, "down", toggleMenu)
		toggleKeyBound = false
	end

	if renderHookActive then
		removeEventHandler("onClientRender", root, renderMenu)
		renderHookActive = false
	end

	mirageFunc("imgui.captureInput", false)
	mirageFunc("imgui.blockBinds", false)
	mirageFunc("imgui.forceCursor", false)
	mirageFunc("imgui.drawCursor", false)
	pcall(mirageFunc, "imgui.clear")
end

ensureRuntimeHooks()
queueInit()

addEventHandler("onClientResourceStop", resourceRoot, function()
	cleanup()
end)

_G.__CarHackerMenuDestroy = cleanup

function onMirageThreadUnload()
	cleanup()
end
