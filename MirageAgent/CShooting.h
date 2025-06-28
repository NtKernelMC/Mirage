
std::string end_script_r = R"STUB(


local pAnimal
local pBones = {}

local BODY_OFFSETS = 
{
	[88] = 
	{
		body = { 0, -0.1, -0.3, 1.1 },
		head = { 0, 1.2, -0.1, 0.65 },
	},
	[242] =
	{
		body = { 0, -0.05, -0.7, 0.6 },
		head = { 0, 0.7, -0.2, 0.35 },
	},
	[244] =
	{
		body = { 0.1, -0.25, -0.2, 0.8 },
		head = { 0.1, 0.8, 0.3, 0.45 },
	},
}

function RifleShoot()
	local elem = getElementData(localPlayer, "animalAim") or 0
	if elem == true then
		return "head"
	end
	local wx, wy, wz = getWorldFromScreenPosition( scx/2, scy/2, 200 )

	local vecDirection = getCamera().position - Vector3(wx,wy,wz)
	vecDirection:normalize()

	local sHit = false

	for i = 1, 400 do
		local vecPoint = getCamera().position - vecDirection*0.5*i
		for k,v in pairs(pBones) do
			if isInsideColShape( v, vecPoint ) then
				sHit = k
				break
			end
		end
	end

	return sHit
end

function OnAnimalCreated()
	pAnimal = source
	setElementData(localPlayer, 'AnimalElement', pAnimal, false)
	for bone, offsets in pairs(BODY_OFFSETS[pAnimal.model]) do
		pBones[bone] = createColSphere( 0, 0, 0, offsets[4] )
		pBones[bone].dimension = localPlayer.dimension

		attachElements( pBones[bone], pAnimal, offsets[1], offsets[2], offsets[3] )
	end
end
addEvent("OnAnimalCreated", true)
addEventHandler("OnAnimalCreated", root, OnAnimalCreated)

function OnAnimalDestroyed()
	for k,v in pairs(pBones) do
		if isElement(v) then
			destroyElement( v )
		end
	end

	pBones = {}
end
addEvent("OnAnimalDestroyed", true)
addEventHandler("OnAnimalDestroyed", root, OnAnimalDestroyed)

)STUB";