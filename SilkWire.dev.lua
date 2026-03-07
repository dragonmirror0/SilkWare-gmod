-- lua_openscript_cl SilkWare.lua

SilkWare = SilkWare or {}

local _hA = hook.Add
local _hR = hook.Remove
local _hG = hook.GetTable
local _tS = timer.Simple
local _tC = timer.Create
local _tR = timer.Remove
local _mR = math.random
local _sF = string.format
local _pC = pcall
local _CRT = CurTime
local _RT = RealTime

local _hook_salt = tostring(SysTime()) .. tostring(_mR(100000, 999999))
local _hook_names = {}
local _hook_counter = 0

local function _GenHookID(base_name)
    _hook_counter = _hook_counter + 1
    local hash_input = _hook_salt .. base_name .. tostring(_hook_counter) .. tostring(_mR(1, 999999))
    local hash = util.CRC(hash_input)
    local id = _sF("_%s_%s", hash, tostring(_mR(1000, 9999)))
    _hook_names[base_name] = id
    return id
end

local function _GetHookID(base_name)
    return _hook_names[base_name] or base_name
end

local function _SafeHookAdd(event, id, func)
    local wrapped = function(...)
        local ok, ret = _pC(func, ...)
        if ok then return ret end
    end
    _hA(event, id, wrapped)
end

local function _SafeHookRemove(event, id)
    _pC(_hR, event, id)
end

local _old_hook_ids = {
    {"HUDPaint", "SW"},
    {"Think", "SW"},
    {"Think", "SW_TextInput"},
    {"Think", "SW_NoFlash"},
    {"Think", "SW_NoTinnitus"},
    {"Think", "SW_NoSuppression"},
    {"Think", "SW_WeaponMods"},
    {"PlayerBindPress", "SW"},
    {"CreateMove", "SW"},
    {"CreateMove", "SW_AntiAim"},
    {"CreateMove", "SW_WeaponMods"},
    {"CalcView", "SW_CalcView_Fallback"},
    {"PreRender", "SW_TP_Fix"},
    {"PreDrawSkyBox", "SW"},
    {"PostDraw2DSkyBox", "SW"},
    {"RenderScreenspaceEffects", "SW_WorldColor"},
    {"ShouldDrawLocalPlayer", "SW_TP"},
    {"PostDrawOpaqueRenderables", "SW_OurBones"},
    {"PrePlayerDraw", "SW_HideLocalModel"},
    {"PostPlayerDraw", "SW_RestoreLocalModel"},
    {"InputMouseApply", "SW_AA_Mouse"},
    {"PreRender", "SW_CollectBones"},
    {"PostRender", "SW_CollectLocalBones"},
    {"Think", "SW_NoSway"},
    {"CreateMove", "SW_Aimbot"},
    {"Think", "SW_AimbotCache"},
}

for _, v in ipairs(_old_hook_ids) do
    _pC(_hR, v[1], v[2])
end

if SilkWare._registered_hooks then
    for _, v in ipairs(SilkWare._registered_hooks) do
        _pC(_hR, v[1], v[2])
    end
end
SilkWare._registered_hooks = {}

local HID_HUDPAINT       = _GenHookID("HUDPaint")
local HID_THINK_MAIN     = _GenHookID("Think_Main")
local HID_THINK_TEXT      = _GenHookID("Think_TextInput")
local HID_THINK_NOFLASH   = _GenHookID("Think_NoFlash")
local HID_THINK_NOTINN    = _GenHookID("Think_NoTinnitus")
local HID_THINK_NOSUPP    = _GenHookID("Think_NoSuppression")
local HID_THINK_WEPMODS   = _GenHookID("Think_WeaponMods")
local HID_THINK_NOSWAY    = _GenHookID("Think_NoSway")
local HID_THINK_AIMCACHE  = _GenHookID("Think_AimbotCache")
local HID_BIND            = _GenHookID("PlayerBindPress")
local HID_CM_MAIN         = _GenHookID("CreateMove_Main")
local HID_CM_AA           = _GenHookID("CreateMove_AntiAim")
local HID_CM_WEPMODS      = _GenHookID("CreateMove_WeaponMods")
local HID_CM_AIMBOT       = _GenHookID("CreateMove_Aimbot")
local HID_CALCVIEW        = _GenHookID("CalcView_Fallback")
local HID_PRERENDER_TP    = _GenHookID("PreRender_TP")
local HID_PRESKYBOX       = _GenHookID("PreDrawSkyBox")
local HID_POSTSKYBOX      = _GenHookID("PostDraw2DSkyBox")
local HID_WORLDCOLOR      = _GenHookID("WorldColor")
local HID_SHOULDDRAW      = _GenHookID("ShouldDrawLocalPlayer")
local HID_INPUTMOUSE      = _GenHookID("InputMouseApply")
local HID_POSTRENDER_BONES= _GenHookID("PostRender_Bones")

local function RegisterHook(event, hook_id, func)
    _SafeHookAdd(event, hook_id, func)
    table.insert(SilkWare._registered_hooks, {event, hook_id})
end

local _junk_count = _mR(3, 7)
local _junk_events = {"Think", "Tick", "PlayerTick", "EntityEmitSound", "PlayerFootstep"}
for i = 1, _junk_count do
    local ev = _junk_events[_mR(1, #_junk_events)]
    local jid = _sF("_%s_%d", util.CRC(tostring(SysTime()) .. tostring(i)), _mR(10000, 99999))
    _hA(ev, jid, function() end)
    table.insert(SilkWare._registered_hooks, {ev, jid})
end

if not SilkWare._hookGetTablePatched then
    local _origGetTable = hook.GetTable
    hook.GetTable = function()
        local tbl = _origGetTable()
        local cleaned = {}
        for event, hooks_map in pairs(tbl) do
            cleaned[event] = {}
            for id, func in pairs(hooks_map) do
                local dominated = false
                for _, our in ipairs(SilkWare._registered_hooks or {}) do
                    if our[1] == event and our[2] == id then
                        dominated = true
                        break
                    end
                end
                if not dominated then
                    cleaned[event][id] = func
                end
            end
        end
        return cleaned
    end
    SilkWare._hookGetTablePatched = true
end

local function _NeutralizeCAC()
    local _cac_nets = {
        "CAC_OnDetection", "cac.Detection", "CAC.Detection",
        "CAC_ScanResult", "cac_result", "CAC_Violation"
    }

    for _, netName in ipairs(_cac_nets) do
        _pC(function()
            net.Receive(netName, function() end)
        end)
    end

    if not SilkWare._debugPatched then
        local _origDebugGetinfo = debug.getinfo
        if _origDebugGetinfo then
            debug.getinfo = function(func_or_level, what)
                local info = _origDebugGetinfo(func_or_level, what)
                if info and info.source then
                    if string.find(info.source, "SilkWare") or string.find(info.source, "silk") then
                        info.source = "=[C]"
                        info.short_src = "[C]"
                        info.what = "C"
                    end
                end
                return info
            end
        end
        SilkWare._debugPatched = true
    end

    if not SilkWare._stringDumpPatched then
        local _origStringDump = string.dump
        if _origStringDump then
            string.dump = function(func, strip)
                local info = _pC(debug.getinfo, func, "S")
                if info then
                    local ok, data = _pC(_origStringDump, func, strip)
                    if ok then return data end
                end
                return _origStringDump(func, strip)
            end
        end
        SilkWare._stringDumpPatched = true
    end

    _pC(function()
        if concommand and concommand.GetTable then
        end
    end)
end

_tS(_mR() * 0.3, _NeutralizeCAC)

local _cac_timer_id = _sF("_ct_%s", util.CRC(tostring(SysTime())))
_tC(_cac_timer_id, _mR(5, 15), 0, function()
    if not SilkWare then
        _tR(_cac_timer_id)
        return
    end
    _pC(_NeutralizeCAC)
end)
table.insert(SilkWare._registered_hooks, {"_timer", _cac_timer_id})

local ply = LocalPlayer()
if IsValid(ply) then
    ply:SetRenderMode(RENDERMODE_NORMAL)
    ply:SetColor(Color(255, 255, 255, 255))
    ply.norender = nil
    local head = ply:LookupBone("ValveBiped.Bip01_Head1")
    if head then
        ply:ManipulateBoneScale(head, Vector(1, 1, 1))
    end
end

if SilkWare and SilkWare._orig_homigrad_view then
    _hA("CalcView", "homigrad-view", SilkWare._orig_homigrad_view)
    SilkWare._view_installed = false
end

if SilkWare and SilkWare._orig_helmet_hook then
    _hA("Post Pre Post Processing", "renderHelmetThingy", SilkWare._orig_helmet_hook)
end

local black   = Color(0, 0, 0, 255)
local black2  = Color(10, 10, 10, 255)
local black3  = Color(18, 18, 18, 255)
local black4  = Color(25, 25, 25, 255)
local black5  = Color(32, 32, 32, 255)
local gray1   = Color(40, 40, 40, 255)
local gray2   = Color(55, 55, 55, 255)
local gray3   = Color(80, 80, 80, 255)
local gray4   = Color(110, 110, 110, 255)
local gray5   = Color(150, 150, 150, 255)
local white   = Color(220, 220, 220, 255)
local white2  = Color(255, 255, 255, 255)
local red     = Color(200, 30, 30, 255)
local red2    = Color(230, 40, 40, 255)
local red3    = Color(160, 20, 20, 255)
local red4    = Color(120, 15, 15, 255)
local reddim  = Color(200, 30, 30, 80)
local green   = Color(30, 200, 30, 255)
local yellow  = Color(200, 200, 30, 255)
local orange  = Color(255, 150, 50, 255)
local cyan    = Color(100, 200, 255, 255)

surface.CreateFont("SW_Title", { font = "Arial Black", size = 28, weight = 900, antialias = true })
surface.CreateFont("SW_Tab", { font = "Arial Black", size = 15, weight = 800, antialias = true })
surface.CreateFont("SW_Group", { font = "Arial Black", size = 13, weight = 800, antialias = true })
surface.CreateFont("SW_Elem", { font = "Arial Black", size = 12, weight = 700, antialias = true })
surface.CreateFont("SW_Small", { font = "Arial Black", size = 11, weight = 700, antialias = true })
surface.CreateFont("SW_Btn", { font = "Arial Black", size = 14, weight = 800, antialias = true })
surface.CreateFont("SW_ESP_Name", { font = "Arial Black", size = 13, weight = 700, antialias = true, shadow = true })
surface.CreateFont("SW_ESP_Info", { font = "Arial Black", size = 11, weight = 600, antialias = true, shadow = true })
surface.CreateFont("SW_ESP_Small", { font = "Arial Black", size = 10, weight = 600, antialias = true, shadow = true })
surface.CreateFont("SW_ESP_Tiny", { font = "Arial Black", size = 9, weight = 600, antialias = true, shadow = true })

local mx, my = 0, 0
local click, hold, release = false, false, false
local prev_m1 = false
local scroll_acc = 0
local click_consumed = false

local function UpdateInput()
    mx, my = gui.MousePos()
    local m1 = input.IsMouseDown(MOUSE_LEFT)
    click = m1 and not prev_m1
    release = not m1 and prev_m1
    hold = m1
    prev_m1 = m1
    click_consumed = false
end

local function InBox(x, y, w, h)
    return mx >= x and mx <= x + w and my >= y and my <= y + h
end

local function ConsumeClick()
    click_consumed = true
end

local function IsClickAvailable()
    return click and not click_consumed
end

local W = 1280
local H = 768

local menu = {
    open = false,
    x = math.floor(ScrW() / 2 - W / 2),
    y = math.floor(ScrH() / 2 - H / 2),
    w = W,
    h = H,
    tab = 1,
    drag = false,
    dx = 0,
    dy = 0,
}

local tabs = {"Combat", "Rage", "Visuals", "Misc", "Config"}

local default_visuals = {
    box_enabled = false,
    box_mode = 1,
    box_color = Color(200, 30, 30, 255),
    name_enabled = false,
    name_color = Color(255, 255, 255, 255),
    usergroup_enabled = false,
    usergroup_color = Color(180, 180, 255, 255),
    team_enabled = false,
    team_color = Color(100, 255, 100, 255),
    weapon_enabled = false,
    weapon_color = Color(255, 200, 100, 255),
    weapon_show_ammo = false,
    weapon_show_reloading = false,
    skeleton_enabled = false,
    skeleton_color = Color(255, 255, 255, 255),
    skeleton_show_local = false,
    hitboxes_enabled = false,
    hitboxes_color = Color(255, 100, 100, 180),
    hitboxes_show_local = false,
    healthbar_enabled = false,
    healthbar_color_full = Color(0, 255, 0, 255),
    healthbar_color_empty = Color(255, 0, 0, 255),
    organism_info_enabled = false,
    organism_info_color = Color(255, 200, 100, 255),
    show_inventory_enabled = false,
    show_inventory_color = Color(180, 180, 255, 255),
    custom_skybox_enabled = false,
    custom_skybox_color = Color(20, 20, 40, 255),
    disable_3d_skybox = false,
    world_color_enabled = false,
    world_color = Color(255, 255, 255, 255),
    custom_fov_enabled = false,
    custom_fov = 90,
    thirdperson_enabled = false,
    thirdperson_distance = 120,
    thirdperson_right_offset = 0,
    hide_armor_overlay = false,
    no_flash = false,
    no_tinnitus = false,
    no_suppression = false,
    trajectory_enabled = false,
    trajectory_shape = 1,
    trajectory_size = 6,
    trajectory_color = Color(255, 0, 0, 255),
    trajectory_outline = true,
    trajectory_outline_color = Color(0, 0, 0, 255),
    aa_enabled = false,
    aa_pitch_mode = 1,
    aa_yaw_mode = 1,
    aa_preset = 1,
    aa_custom_pitch = 0,
    aa_custom_yaw = 0,
    aa_spin_speed = 10,
    aa_jitter_range = 90,
    no_recoil = false,
    no_spread = false,
    fast_zoom = false,
    no_sway = false,
    aimbot_enabled = false,
    aimbot_key = 0,
    aimbot_mode = 1,
    aimbot_fov = 30,
    aimbot_priority = 1,
    aimbot_autofire = false,
    aimbot_vischeck = true,
    aimbot_teamcheck = true,
    aimbot_hitbox = 1,
    aimbot_fov_circle_enabled = false,
    aimbot_fov_circle_color = Color(255, 255, 255, 100),
    aimbot_fov_circle_shape = 1,
}

local function CopyDefaults()
    local t = {}
    for k, v in pairs(default_visuals) do
        if IsColor(v) then
            t[k] = Color(v.r, v.g, v.b, v.a)
        else
            t[k] = v
        end
    end
    return t
end

SilkWare.Visuals = SilkWare.Visuals or CopyDefaults()
local V = SilkWare.Visuals

for k, v in pairs(default_visuals) do
    if V[k] == nil then
        if IsColor(v) then
            V[k] = Color(v.r, v.g, v.b, v.a)
        else
            V[k] = v
        end
    end
end

local CONFIG_DIR = "SilkWarecfgs"

if not file.IsDir(CONFIG_DIR, "DATA") then
    file.CreateDir(CONFIG_DIR)
end

local config_list = {}
local config_selected = nil
local config_name_buf = ""
local config_typing = false
local config_status = ""
local config_status_time = 0

local function RefreshConfigList()
    config_list = {}
    local files = file.Find(CONFIG_DIR .. "/*.json", "DATA")
    for _, f in ipairs(files or {}) do
        table.insert(config_list, string.StripExtension(f))
    end
    table.sort(config_list)
end

local function SetConfigStatus(msg)
    config_status = msg
    config_status_time = _RT() + 3
end

local function SerializeColor(c)
    return {r = c.r, g = c.g, b = c.b, a = c.a}
end

local function DeserializeColor(t)
    if istable(t) and t.r then
        return Color(t.r, t.g, t.b, t.a or 255)
    end
    return nil
end

local function SaveConfig(name)
    if not name or name == "" then SetConfigStatus("Invalid name") return end
    local data = {}
    for k, v in pairs(V) do
        if IsColor(v) then
            data[k] = SerializeColor(v)
        else
            data[k] = v
        end
    end
    file.Write(CONFIG_DIR .. "/" .. name .. ".json", util.TableToJSON(data, true))
    RefreshConfigList()
    SetConfigStatus("Saved: " .. name)
end

local function LoadConfig(name)
    if not name or name == "" then SetConfigStatus("No config selected") return end
    local path = CONFIG_DIR .. "/" .. name .. ".json"
    if not file.Exists(path, "DATA") then SetConfigStatus("Not found: " .. name) return end
    local data = util.JSONToTable(file.Read(path, "DATA"))
    if not data then SetConfigStatus("Parse error") return end
    for k, v in pairs(data) do
        if istable(v) and v.r then
            V[k] = DeserializeColor(v)
        elseif default_visuals[k] ~= nil then
            V[k] = v
        end
    end
    SetConfigStatus("Loaded: " .. name)
end

local function DeleteConfig(name)
    if not name or name == "" then SetConfigStatus("No config selected") return end
    local path = CONFIG_DIR .. "/" .. name .. ".json"
    if file.Exists(path, "DATA") then
        file.Delete(path)
        if config_selected == name then config_selected = nil end
        RefreshConfigList()
        SetConfigStatus("Deleted: " .. name)
    else
        SetConfigStatus("Not found: " .. name)
    end
end

local function ResetConfig()
    local def = CopyDefaults()
    for k, v in pairs(def) do
        V[k] = v
    end
    SetConfigStatus("Reset to defaults")
end

RefreshConfigList()

local groups = {
    [1] = {},
    [2] = {},
    [3] = {},
    [4] = {
        {title = "Movement",   col = 1},
        {title = "Networking", col = 1},
        {title = "Utilities",  col = 2},
    },
    [5] = {},
}

local function CleanupPlayerState()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    lp:SetRenderMode(RENDERMODE_NORMAL)
    lp:SetColor(Color(255, 255, 255, 255))
    lp.norender = nil
    render.SetBlend(1)
    local head = lp:LookupBone("ValveBiped.Bip01_Head1")
    if head then
        lp:ManipulateBoneScale(head, Vector(1, 1, 1))
    end
    SilkWare._local_bones_drawing = false
end

local function CleanupWeaponMods()
    if not SilkWare then return end
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local wep = lp:GetActiveWeapon()
    if IsValid(wep) then
        wep.norecoil = nil
    end
end

local function Unload()
    if SilkWare._orig_homigrad_view then
        _hA("CalcView", "homigrad-view", SilkWare._orig_homigrad_view)
    end
    if SilkWare._orig_helmet_hook then
        _hA("Post Pre Post Processing", "renderHelmetThingy", SilkWare._orig_helmet_hook)
    end

    CleanupPlayerState()
    CleanupWeaponMods()

    SilkWare._tp_active = false
    SilkWare._tp_was_active = false
    SilkWare._hide_local_model = false
    SilkWare._aa_active = false
    SilkWare._aa_suppressed = false
    SilkWare._aa_initialized = false
    SilkWare._aa_cam_pitch = nil
    SilkWare._aa_cam_yaw = nil
    SilkWare._trajectory_valid = false
    SilkWare._trajectory_hitpos = nil

    if SilkWare._registered_hooks then
        for _, v in ipairs(SilkWare._registered_hooks) do
            if v[1] == "_timer" then
                _pC(_tR, v[2])
            else
                _pC(_hR, v[1], v[2])
            end
        end
    end

    if SilkWare._hookGetTablePatched then
        SilkWare._hookGetTablePatched = false
    end

    gui.EnableScreenClicker(false)
    SilkWare = nil
    print("[SilkWare] Unloaded.")
end

local anim = {}

local function ALerp(id, target, sp)
    sp = sp or 10
    anim[id] = anim[id] or target
    anim[id] = Lerp(FrameTime() * sp, anim[id], target)
    if math.abs(anim[id] - target) < 0.01 then anim[id] = target end
    return anim[id]
end

local active_dropdown = nil
local active_color_picker = nil
local cp_dragging = nil
local active_slider = nil

local function DrawCheckbox(x, y, w, label, key, indent)
    indent = indent or 0
    local h = 18
    local box_size = 12
    local bx = x + 8 + indent
    local by = y + (h - box_size) / 2
    local hov = InBox(bx, by, w - 16 - indent, h)

    if hov and IsClickAvailable() then
        V[key] = not V[key]
        ConsumeClick()
    end

    local bg = V[key] and red or gray1
    if hov then bg = V[key] and red2 or gray2 end
    draw.RoundedBox(0, bx, by, box_size, box_size, bg)
    surface.SetDrawColor(V[key] and red or gray2)
    surface.DrawOutlinedRect(bx, by, box_size, box_size)

    if V[key] then
        surface.SetDrawColor(white2)
        surface.DrawLine(bx + 2, by + 6, bx + 4, by + 9)
        surface.DrawLine(bx + 4, by + 9, bx + 9, by + 3)
        surface.DrawLine(bx + 2, by + 5, bx + 4, by + 8)
        surface.DrawLine(bx + 4, by + 8, bx + 9, by + 2)
    end

    local tcol = V[key] and white or gray5
    if hov then tcol = white2 end
    draw.SimpleText(label, "SW_Elem", bx + box_size + 6, by + box_size / 2, tcol, TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

    return y + h
end

local function DrawDropdown(x, y, w, label, key, options, indent)
    indent = indent or 0
    local h = 18
    local lx = x + 8 + indent
    draw.SimpleText(label, "SW_Elem", lx, y + h / 2, gray5, TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

    local dw = 130
    local dh = 18
    local dx = x + w - dw - 10
    local dy = y

    local current_text = options[V[key]] or "???"
    local hov = InBox(dx, dy, dw, dh)

    draw.RoundedBox(0, dx, dy, dw, dh, hov and gray2 or gray1)
    surface.SetDrawColor(hov and gray3 or gray2)
    surface.DrawOutlinedRect(dx, dy, dw, dh)
    draw.SimpleText(current_text, "SW_Small", dx + 6, dy + dh / 2, white, TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)
    draw.SimpleText("v", "SW_Small", dx + dw - 12, dy + dh / 2, gray4, TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)

    if hov and IsClickAvailable() then
        active_dropdown = (active_dropdown == key) and nil or key
        ConsumeClick()
    end

    local total_h = h

    if active_dropdown == key then
        local ly = dy + dh
        for i, opt in ipairs(options) do
            local ohov = InBox(dx, ly, dw, dh)
            local sel = V[key] == i

            draw.RoundedBox(0, dx, ly, dw, dh, sel and red3 or (ohov and black5 or black4))
            surface.SetDrawColor(gray2)
            surface.DrawOutlinedRect(dx, ly, dw, dh)
            draw.SimpleText(opt, "SW_Small", dx + 6, ly + dh / 2, sel and white2 or (ohov and white or gray5), TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

            if ohov and IsClickAvailable() then
                V[key] = i
                active_dropdown = nil
                ConsumeClick()
            end

            ly = ly + dh
        end
        total_h = h + #options * dh
    end

    return y + total_h
end

local function DrawColorPicker(x, y, w, label, key, indent)
    indent = indent or 0
    local h = 18
    local lx = x + 8 + indent
    draw.SimpleText(label, "SW_Elem", lx, y + h / 2, gray5, TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

    local preview_size = 14
    local px = x + w - preview_size - 10
    local py = y + (h - preview_size) / 2
    local col = V[key]

    surface.SetDrawColor(gray3)
    surface.DrawRect(px, py, preview_size, preview_size)
    surface.SetDrawColor(col)
    surface.DrawRect(px, py, preview_size, preview_size)
    surface.SetDrawColor(gray2)
    surface.DrawOutlinedRect(px, py, preview_size, preview_size)

    local phov = InBox(px, py, preview_size, preview_size)
    if phov and IsClickAvailable() then
        active_color_picker = (active_color_picker == key) and nil or key
        ConsumeClick()
    end

    local total_h = h

    if active_color_picker == key then
        local cpw = 180
        local cph = 130
        local cpx = px - cpw + preview_size
        local cpy = y + h + 2

        draw.RoundedBox(0, cpx, cpy, cpw, cph, black2)
        surface.SetDrawColor(gray2)
        surface.DrawOutlinedRect(cpx, cpy, cpw, cph)

        local hue_x = cpx + 8
        local hue_y = cpy + 8
        local hue_w = cpw - 16
        local hue_h = 12

        for i = 0, hue_w - 1 do
            surface.SetDrawColor(HSVToColor((i / hue_w) * 360, 1, 1))
            surface.DrawLine(hue_x + i, hue_y, hue_x + i, hue_y + hue_h)
        end
        surface.SetDrawColor(white2)
        surface.DrawOutlinedRect(hue_x, hue_y, hue_w, hue_h)

        local sv_x = cpx + 8
        local sv_y = cpy + 28
        local sv_w = cpw - 16
        local sv_h = 70
        local cur_h, cur_s, cur_v = ColorToHSV(col)

        for ix = 0, sv_w - 1, 2 do
            for iy = 0, sv_h - 1, 2 do
                surface.SetDrawColor(HSVToColor(cur_h, ix / sv_w, 1 - iy / sv_h))
                surface.DrawRect(sv_x + ix, sv_y + iy, 2, 2)
            end
        end
        surface.SetDrawColor(white2)
        surface.DrawOutlinedRect(sv_x, sv_y, sv_w, sv_h)

        local al_y = sv_y + sv_h + 6
        local al_h = 10
        for i = 0, hue_w - 1 do
            surface.SetDrawColor(Color(col.r, col.g, col.b, (i / hue_w) * 255))
            surface.DrawLine(hue_x + i, al_y, hue_x + i, al_y + al_h)
        end
        surface.SetDrawColor(white2)
        surface.DrawOutlinedRect(hue_x, al_y, hue_w, al_h)

        if hold then
            if InBox(hue_x, hue_y, hue_w, hue_h) or cp_dragging == key .. "_hue" then
                cp_dragging = key .. "_hue"
                local nc = HSVToColor(math.Clamp((mx - hue_x) / hue_w, 0, 1) * 360, cur_s, cur_v)
                nc.a = col.a
                V[key] = nc
            elseif InBox(sv_x, sv_y, sv_w, sv_h) or cp_dragging == key .. "_sv" then
                cp_dragging = key .. "_sv"
                local nc = HSVToColor(cur_h, math.Clamp((mx - sv_x) / sv_w, 0, 1), math.Clamp(1 - (my - sv_y) / sv_h, 0, 1))
                nc.a = col.a
                V[key] = nc
            elseif InBox(hue_x, al_y, hue_w, al_h) or cp_dragging == key .. "_alpha" then
                cp_dragging = key .. "_alpha"
                V[key] = Color(col.r, col.g, col.b, math.floor(math.Clamp((mx - hue_x) / hue_w, 0, 1) * 255))
            end
        else
            cp_dragging = nil
        end

        surface.SetDrawColor(white2)
        surface.DrawRect(hue_x + (cur_h / 360) * hue_w - 1, hue_y - 1, 2, hue_h + 2)
        surface.DrawOutlinedRect(sv_x + cur_s * sv_w - 3, sv_y + (1 - cur_v) * sv_h - 3, 6, 6)
        surface.DrawRect(hue_x + (col.a / 255) * hue_w - 1, al_y - 1, 2, al_h + 2)

        total_h = h + cph + 4
    end

    return y + total_h
end

local function DrawSlider(x, y, w, label, key, min_val, max_val, step, indent, suffix)
    indent = indent or 0
    step = step or 1
    suffix = suffix or ""
    local h = 28
    local lx = x + 8 + indent
    local val = V[key]
    local disp = math.Round(val, step < 1 and 1 or 0)

    draw.SimpleText(label, "SW_Elem", lx, y + 2, gray5, TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP)
    draw.SimpleText(tostring(disp) .. suffix, "SW_Small", x + w - 10, y + 2, gray4, TEXT_ALIGN_RIGHT, TEXT_ALIGN_TOP)

    local track_x = lx
    local track_y = y + 16
    local track_w = w - 18 - indent
    local track_h = 6

    draw.RoundedBox(0, track_x, track_y, track_w, track_h, gray1)
    local frac = math.Clamp((val - min_val) / (max_val - min_val), 0, 1)
    draw.RoundedBox(0, track_x, track_y, frac * track_w, track_h, red)
    draw.RoundedBox(0, track_x + frac * track_w - 4, track_y - 2, 8, 10, white2)

    if InBox(track_x - 4, track_y - 6, track_w + 8, track_h + 12) and IsClickAvailable() then
        active_slider = key
        ConsumeClick()
    end
    if active_slider == key then
        if hold then
            local nv = min_val + math.Clamp((mx - track_x) / track_w, 0, 1) * (max_val - min_val)
            V[key] = math.Clamp(math.Round(nv / step) * step, min_val, max_val)
        else
            active_slider = nil
        end
    end

    return y + h
end

SilkWare._keybind_waiting = false

local key_names = {
    [MOUSE_LEFT] = "MOUSE1", [MOUSE_RIGHT] = "MOUSE2", [MOUSE_MIDDLE] = "MOUSE3",
    [MOUSE_4] = "MOUSE4", [MOUSE_5] = "MOUSE5",
}

local function GetKeyName(keycode)
    if keycode == 0 then return "None" end
    if key_names[keycode] then return key_names[keycode] end
    local name = input.GetKeyName(keycode)
    if name then return string.upper(name) end
    return "KEY " .. keycode
end

local function DrawKeybinder(x, y, w, label, key, indent)
    indent = indent or 0
    local h = 18
    local lx = x + 8 + indent
    draw.SimpleText(label, "SW_Elem", lx, y + h / 2, gray5, TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

    local btn_w = 100
    local btn_h = 18
    local bx = x + w - btn_w - 10
    local by = y

    local waiting = SilkWare._keybind_waiting == key
    local current_val = V[key]
    local display_text = waiting and "..." or GetKeyName(current_val)

    local hov = InBox(bx, by, btn_w, btn_h)

    draw.RoundedBox(0, bx, by, btn_w, btn_h, waiting and red3 or (hov and gray2 or gray1))
    surface.SetDrawColor(waiting and red or (hov and gray3 or gray2))
    surface.DrawOutlinedRect(bx, by, btn_w, btn_h)
    draw.SimpleText(display_text, "SW_Small", bx + btn_w / 2, by + btn_h / 2, waiting and white2 or white, TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)

    if not waiting and hov and IsClickAvailable() then
        SilkWare._keybind_waiting = key
        SilkWare._keybind_lmb_was_pressed = true
        ConsumeClick()
    end

    if waiting then
        if SilkWare._keybind_lmb_was_pressed then
            if not input.IsMouseDown(MOUSE_LEFT) then
                SilkWare._keybind_lmb_was_pressed = false
            end
            return y + h
        end

        if input.IsMouseDown(MOUSE_RIGHT) then
            V[key] = MOUSE_RIGHT
            SilkWare._keybind_waiting = false
            return y + h
        end

        if input.IsMouseDown(MOUSE_LEFT) then
            V[key] = MOUSE_LEFT
            SilkWare._keybind_waiting = false
            return y + h
        end

        for _, m_code in ipairs({MOUSE_MIDDLE, MOUSE_4, MOUSE_5}) do
            if input.IsMouseDown(m_code) then
                V[key] = m_code
                SilkWare._keybind_waiting = false
                return y + h
            end
        end

        for k_code = KEY_FIRST, KEY_LAST do
            if k_code == KEY_LWIN then continue end
            if input.IsKeyDown(k_code) then
                V[key] = k_code
                SilkWare._keybind_waiting = false
                return y + h
            end
        end
    else
        if hov and input.IsMouseDown(MOUSE_RIGHT) and not SilkWare._keybind_rmb_was_pressed then
            V[key] = 0
            SilkWare._keybind_rmb_was_pressed = true
        end
        if not input.IsMouseDown(MOUSE_RIGHT) then
            SilkWare._keybind_rmb_was_pressed = false
        end
    end

    return y + h
end

local TITLE_H = 50
local TAB_H = 36
local HEADER_H = TITLE_H + TAB_H

local function DrawTitle(x, y, w)
    surface.SetDrawColor(black)
    surface.DrawRect(x, y, w, TITLE_H)
    surface.SetDrawColor(red)
    surface.DrawRect(x, y, w, 3)
    draw.SimpleText("SilkWare", "SW_Title", x + w / 2, y + TITLE_H / 2 + 1, red, TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)
    surface.SetDrawColor(gray1)
    surface.DrawLine(x, y + TITLE_H - 1, x + w, y + TITLE_H - 1)
end

local function DrawTabs(x, y, w)
    local count = #tabs
    local tw = w / count
    surface.SetDrawColor(black2)
    surface.DrawRect(x, y, w, TAB_H)

    for i, name in ipairs(tabs) do
        local tx = x + (i - 1) * tw
        local active = menu.tab == i
        local hov = InBox(tx, y, tw, TAB_H)

        if hov and IsClickAvailable() then
            menu.tab = i
            active = true
            ConsumeClick()
            active_dropdown = nil
            active_color_picker = nil
        end

        local bg_a = ALerp("tab_" .. i, active and 60 or (hov and 30 or 0), 12)
        if bg_a > 0.5 then
            surface.SetDrawColor(Color(200, 30, 30, bg_a))
            surface.DrawRect(tx, y, tw, TAB_H)
        end

        local ind = ALerp("tabi_" .. i, active and tw or 0, 14)
        if ind > 1 then
            surface.SetDrawColor(red)
            surface.DrawRect(tx + (tw - ind) / 2, y + TAB_H - 3, ind, 3)
        end

        draw.SimpleText(name, "SW_Tab", tx + tw / 2, y + TAB_H / 2, active and white2 or (hov and white or gray4), TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)

        if i < count then
            surface.SetDrawColor(gray1)
            surface.DrawLine(tx + tw, y + 6, tx + tw, y + TAB_H - 6)
        end
    end

    surface.SetDrawColor(gray1)
    surface.DrawLine(x, y + TAB_H - 1, x + w, y + TAB_H - 1)
end

local function DrawGroupBox(gx, gy, gw, gh, title, draw_func)
    draw.RoundedBox(0, gx, gy, gw, gh, black3)
    surface.SetDrawColor(gray1)
    surface.DrawOutlinedRect(gx, gy, gw, gh)

    local th = 28
    surface.SetDrawColor(black2)
    surface.DrawRect(gx + 1, gy + 1, gw - 2, th)
    surface.SetDrawColor(gray1)
    surface.DrawLine(gx + 1, gy + th, gx + gw - 1, gy + th)
    surface.SetDrawColor(red)
    surface.DrawRect(gx + 8, gy + th / 2 - 5, 3, 10)
    draw.SimpleText(title, "SW_Group", gx + 18, gy + th / 2, white, TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

    if draw_func then
        render.SetScissorRect(gx + 1, gy + th + 1, gx + gw - 1, gy + gh - 1, true)
        draw_func(gx, gy + th + 4, gw, gh - th - 6)
        render.SetScissorRect(0, 0, 0, 0, false)
    end
end

local aimbot_mode_options = {"Legit", "Silent"}
local aimbot_priority_options = {"FOV", "Distance", "Health"}
local aimbot_hitbox_options = {"Head", "Chest", "Pelvis"}

SilkWare._aimbot_target = nil
SilkWare._aimbot_target_pos = nil
SilkWare._aimbot_on_target = false
SilkWare._aimbot_real_angles = nil
SilkWare._cached_muzzle_pos = nil
SilkWare._cached_muzzle_ang = nil
SilkWare._cached_view_ang = nil
SilkWare._cached_raw_offset_p = nil
SilkWare._cached_raw_offset_y = nil
SilkWare._smooth_offset_p = nil
SilkWare._smooth_offset_y = nil

local function Aimbot_IsKeyActive()
    local key = V.aimbot_key
    if key == 0 then return true end
    if key == MOUSE_LEFT then return input.IsMouseDown(MOUSE_LEFT) end
    if key == MOUSE_RIGHT then return input.IsMouseDown(MOUSE_RIGHT) end
    if key == MOUSE_MIDDLE then return input.IsMouseDown(MOUSE_MIDDLE) end
    if key == MOUSE_4 then return input.IsMouseDown(MOUSE_4) end
    if key == MOUSE_5 then return input.IsMouseDown(MOUSE_5) end
    return input.IsKeyDown(key)
end

local function Aimbot_GetHitboxPosition(ply, ent, hitbox_mode)
    if not IsValid(ent) then return nil end

    local set = ent:GetHitboxSet() or 0
    local count = ent:GetHitBoxCount(set)
    if not count or count == 0 then return nil end

    local target_group = nil
    if hitbox_mode == 1 then
        target_group = HITGROUP_HEAD
    elseif hitbox_mode == 2 then
        target_group = HITGROUP_CHEST
    elseif hitbox_mode == 3 then
        target_group = HITGROUP_STOMACH
    end

    for i = 0, count - 1 do
        local hg_id = ent:GetHitBoxHitGroup(i, set)
        if target_group and hg_id ~= target_group then continue end

        local bone = ent:GetHitBoxBone(i, set)
        if not bone then continue end

        local bmat = ent:GetBoneMatrix(bone)
        if not bmat then continue end

        local bone_pos = bmat:GetTranslation()
        if not bone_pos or bone_pos:IsZero() then continue end

        local hb_mins, hb_maxs = ent:GetHitBoxBounds(i, set)
        if not hb_mins or not hb_maxs then continue end

        local center_local = (hb_mins + hb_maxs) / 2
        local center_world = LocalToWorld(center_local, Angle(0, 0, 0), bone_pos, bmat:GetAngles())

        return center_world
    end

    local head_bone = ent:LookupBone("ValveBiped.Bip01_Head1")
    if head_bone then
        local head_mat = ent:GetBoneMatrix(head_bone)
        if head_mat then
            return head_mat:GetTranslation()
        end
    end

    return ent:GetPos() + Vector(0, 0, 60)
end

local function Aimbot_GetPlayerBlood(ply)
    if not IsValid(ply) or not ply:IsPlayer() then return 5000 end
    local org = ply.organism
    if not org then return 5000 end
    return org.blood or 5000
end

local function Aimbot_IsVisible(from_pos, target_pos, lp, target_ent)
    local tr = util.TraceLine({
        start = from_pos,
        endpos = target_pos,
        filter = {lp, lp:GetActiveWeapon()},
        mask = MASK_SHOT,
    })
    if tr.Entity == target_ent then return true end
    if tr.Fraction > 0.99 then return true end
    if IsValid(tr.Entity) then
        local ply_nw = tr.Entity:GetNWEntity("Player")
        if IsValid(ply_nw) and ply_nw == target_ent then return true end
    end
    return false
end

local function GetCurrentGameMode()
    if zb and zb.CROUND then
        return zb.CROUND
    end
    if CurrentRound and CurrentRound() then
        local rnd = CurrentRound()
        if rnd.name then return rnd.name end
    end
    return "unknown"
end

local function CountActiveTeams()
    local teams_with_players = {}
    for _, ply in ipairs(player.GetAll()) do
        if ply:Team() == TEAM_SPECTATOR or ply:Team() == TEAM_UNASSIGNED then continue end
        teams_with_players[ply:Team()] = true
    end
    local count = 0
    for _ in pairs(teams_with_players) do
        count = count + 1
    end
    return count
end

local function Aimbot_IsOnSameTeam(lp, target)
    if not IsValid(lp) or not IsValid(target) then return false end
    if not lp:IsPlayer() or not target:IsPlayer() then return false end

    local mode = GetCurrentGameMode()

    if mode == "dm" then
        return false
    end

    if mode == "tdm" or mode == "cstrike" then
        local my_team = lp:Team()
        local their_team = target:Team()
        return my_team == their_team
    end

    if mode == "hmcd" then
        return false
    end

    local active_teams = CountActiveTeams()
    if active_teams <= 1 then
        return false
    end

    local my_team = lp:Team()
    local their_team = target:Team()
    if my_team == TEAM_SPECTATOR or their_team == TEAM_SPECTATOR then return false end
    if my_team == TEAM_UNASSIGNED or their_team == TEAM_UNASSIGNED then return false end
    return my_team == their_team
end

local function Aimbot_GetMuzzleFromWeapon(wep)
    if not IsValid(wep) or not wep.ishgweapon then return nil, nil, nil end
    if not wep.GetTrace then return nil, nil, nil end

    local tr, pos, ang = wep:GetTrace(true)
    if not pos or not ang then return nil, nil, nil end

    local dir = ang:Forward()
    local hitpos = tr and tr.HitPos or nil

    return pos, dir, hitpos
end

local function Aimbot_FindTarget(cmd, lp, eye_pos)
    local wep = lp:GetActiveWeapon()
    if not IsValid(wep) then return nil, nil end

    local view_ang = cmd:GetViewAngles()
    local view_dir = view_ang:Forward()

    local fov_limit = V.aimbot_fov
    local hitbox_mode = V.aimbot_hitbox

    local best_target = nil
    local best_target_pos = nil
    local best_score = math.huge

    for _, ply in ipairs(player.GetAll()) do
        if ply == lp then continue end
        if not ply:Alive() then continue end
        if ply:Team() == TEAM_SPECTATOR then continue end

        if V.aimbot_teamcheck and Aimbot_IsOnSameTeam(lp, ply) then continue end

        local ent = ply
        local ragdoll = ply:GetNWEntity("FakeRagdoll")
        if IsValid(ragdoll) then
            ent = ragdoll
        elseif ply.GetRagdollEntity and IsValid(ply:GetRagdollEntity()) then
            ent = ply:GetRagdollEntity()
        end

        if ent == ply and ply:GetMoveType() == MOVETYPE_OBSERVER then
            for _, e in ipairs(ents.FindByClass("prop_ragdoll")) do
                if e:GetNWEntity("Player") == ply then
                    ent = e
                    break
                end
            end
        end

        if not IsValid(ent) then continue end
        ent:SetupBones()

        local target_pos = Aimbot_GetHitboxPosition(ply, ent, hitbox_mode)
        if not target_pos then continue end

        local dir_to_target = (target_pos - eye_pos):GetNormalized()
        local angle_to_target = math.deg(math.acos(math.Clamp(view_dir:Dot(dir_to_target), -1, 1)))

        if angle_to_target > fov_limit then continue end

        if V.aimbot_vischeck then
            local vis_from = SilkWare._cached_muzzle_pos or eye_pos
            if not Aimbot_IsVisible(vis_from, target_pos, lp, ent) then continue end
        end

        local score = 0
        if V.aimbot_priority == 1 then
            score = angle_to_target
        elseif V.aimbot_priority == 2 then
            score = eye_pos:Distance(target_pos)
        elseif V.aimbot_priority == 3 then
            score = Aimbot_GetPlayerBlood(ply)
        end

        if score < best_score then
            best_score = score
            best_target = ply
            best_target_pos = target_pos
        end
    end

    return best_target, best_target_pos
end

local function Aimbot_FindTargetWithAngles(cmd, lp, eye_pos, override_view_ang)
    local wep = lp:GetActiveWeapon()
    if not IsValid(wep) then return nil, nil end

    local view_ang = override_view_ang or cmd:GetViewAngles()
    local view_dir = view_ang:Forward()

    local fov_limit = V.aimbot_fov
    local hitbox_mode = V.aimbot_hitbox

    local best_target = nil
    local best_target_pos = nil
    local best_score = math.huge

    for _, ply in ipairs(player.GetAll()) do
        if ply == lp then continue end
        if not ply:Alive() then continue end
        if ply:Team() == TEAM_SPECTATOR then continue end

        if V.aimbot_teamcheck and Aimbot_IsOnSameTeam(lp, ply) then continue end

        local ent = ply
        local ragdoll = ply:GetNWEntity("FakeRagdoll")
        if IsValid(ragdoll) then
            ent = ragdoll
        elseif ply.GetRagdollEntity and IsValid(ply:GetRagdollEntity()) then
            ent = ply:GetRagdollEntity()
        end

        if ent == ply and ply:GetMoveType() == MOVETYPE_OBSERVER then
            for _, e in ipairs(ents.FindByClass("prop_ragdoll")) do
                if e:GetNWEntity("Player") == ply then
                    ent = e
                    break
                end
            end
        end

        if not IsValid(ent) then continue end
        ent:SetupBones()

        local target_pos = Aimbot_GetHitboxPosition(ply, ent, hitbox_mode)
        if not target_pos then continue end

        local dir_to_target = (target_pos - eye_pos):GetNormalized()
        local angle_to_target = math.deg(math.acos(math.Clamp(view_dir:Dot(dir_to_target), -1, 1)))

        if angle_to_target > fov_limit then continue end

        if V.aimbot_vischeck then
            local vis_from = SilkWare._cached_muzzle_pos or eye_pos
            if not Aimbot_IsVisible(vis_from, target_pos, lp, ent) then continue end
        end

        local score = 0
        if V.aimbot_priority == 1 then
            score = angle_to_target
        elseif V.aimbot_priority == 2 then
            score = eye_pos:Distance(target_pos)
        elseif V.aimbot_priority == 3 then
            score = Aimbot_GetPlayerBlood(ply)
        end

        if score < best_score then
            best_score = score
            best_target = ply
            best_target_pos = target_pos
        end
    end

    return best_target, best_target_pos
end

RegisterHook("Think", HID_THINK_AIMCACHE, function()
    if not SilkWare then return end
    if not V.aimbot_enabled then return end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then
        SilkWare._cached_muzzle_pos = nil
        SilkWare._cached_muzzle_ang = nil
        SilkWare._cached_view_ang = nil
        return
    end

    local wep = lp:GetActiveWeapon()
    if not IsValid(wep) or not wep.ishgweapon then
        SilkWare._cached_muzzle_pos = nil
        SilkWare._cached_muzzle_ang = nil
        SilkWare._cached_view_ang = nil
        return
    end

    if not wep.GetTrace then return end

    local ent = IsValid(lp.FakeRagdoll) and lp.FakeRagdoll or lp
    if IsValid(ent) then
        ent:SetupBones()
    end
    local gun = wep:GetWeaponEntity()
    if IsValid(gun) then
        gun:SetupBones()
    end
    if wep.WorldModel_Transform then
        wep:WorldModel_Transform(true)
    end

    local tr, pos, ang = wep:GetTrace(true)
    if not pos or not ang then
        SilkWare._cached_muzzle_pos = nil
        SilkWare._cached_muzzle_ang = nil
        SilkWare._cached_view_ang = nil
        return
    end

    local muzzle_ang = ang:Forward():Angle()
    local view_ang = lp:EyeAngles()

    local new_off_p = math.AngleDifference(muzzle_ang.p, view_ang.p)
    local new_off_y = math.AngleDifference(muzzle_ang.y, view_ang.y)

    local smooth = FrameTime() * 20
    smooth = math.Clamp(smooth, 0.05, 1.0)

    if SilkWare._smooth_offset_p then
        SilkWare._smooth_offset_p = Lerp(smooth, SilkWare._smooth_offset_p, new_off_p)
        SilkWare._smooth_offset_y = Lerp(smooth, SilkWare._smooth_offset_y, new_off_y)
    else
        SilkWare._smooth_offset_p = new_off_p
        SilkWare._smooth_offset_y = new_off_y
    end

    SilkWare._cached_muzzle_pos = Vector(pos)
    SilkWare._cached_muzzle_ang = Angle(muzzle_ang.p, muzzle_ang.y, 0)
    SilkWare._cached_view_ang = Angle(view_ang.p, view_ang.y, 0)
    SilkWare._cached_raw_offset_p = new_off_p
    SilkWare._cached_raw_offset_y = new_off_y
end)

RegisterHook("CreateMove", HID_CM_AIMBOT, function(cmd)
    if not SilkWare then return end
    if not V.aimbot_enabled then
        SilkWare._aimbot_target = nil
        SilkWare._aimbot_target_pos = nil
        SilkWare._aimbot_on_target = false
        SilkWare._aimbot_real_angles = nil
        SilkWare._aa_found_target = nil
        SilkWare._aa_found_target_pos = nil
        return
    end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then
        SilkWare._aimbot_target = nil
        SilkWare._aimbot_target_pos = nil
        SilkWare._aimbot_on_target = false
        SilkWare._aimbot_real_angles = nil
        SilkWare._aa_found_target = nil
        SilkWare._aa_found_target_pos = nil
        return
    end

    if menu.open or not Aimbot_IsKeyActive() then
        SilkWare._aimbot_target = nil
        SilkWare._aimbot_target_pos = nil
        SilkWare._aimbot_on_target = false
        SilkWare._aimbot_real_angles = nil
        SilkWare._aa_found_target = nil
        SilkWare._aa_found_target_pos = nil
        return
    end

    local wep = lp:GetActiveWeapon()
    if not IsValid(wep) or not wep.ishgweapon then
        SilkWare._aimbot_target = nil
        SilkWare._aimbot_target_pos = nil
        SilkWare._aimbot_on_target = false
        SilkWare._aimbot_real_angles = nil
        SilkWare._aa_found_target = nil
        SilkWare._aa_found_target_pos = nil
        return
    end

    if wep.IsSprinting and wep:IsSprinting() then
        SilkWare._aimbot_target = nil
        SilkWare._aimbot_target_pos = nil
        SilkWare._aimbot_on_target = false
        SilkWare._aimbot_real_angles = nil
        SilkWare._aa_found_target = nil
        SilkWare._aa_found_target_pos = nil
        return
    end

    local eye_pos = lp:EyePos()

    local target, target_pos
    if SilkWare._aa_found_target and IsValid(SilkWare._aa_found_target) and SilkWare._aa_found_target_pos then
        target = SilkWare._aa_found_target
        target_pos = SilkWare._aa_found_target_pos
    else
        local view_ang_for_fov = cmd:GetViewAngles()
        target, target_pos = Aimbot_FindTargetWithAngles(cmd, lp, eye_pos, view_ang_for_fov)
    end

    SilkWare._aa_found_target = nil
    SilkWare._aa_found_target_pos = nil

    SilkWare._aimbot_target = target
    SilkWare._aimbot_target_pos = target_pos

    if not target or not target_pos then
        SilkWare._aimbot_on_target = false
        SilkWare._aimbot_real_angles = nil
        return
    end

    SilkWare._aimbot_on_target = true

    local cached_muzzle_pos = SilkWare._cached_muzzle_pos
    local offset_p = SilkWare._smooth_offset_p
    local offset_y = SilkWare._smooth_offset_y

    if not cached_muzzle_pos or not offset_p or not offset_y then
        SilkWare._aimbot_real_angles = nil
        return
    end

    local raw_off_p = SilkWare._cached_raw_offset_p or offset_p
    local raw_off_y = SilkWare._cached_raw_offset_y or offset_y

    local base_ang = cmd:GetViewAngles()

    local predicted_muzzle_p = base_ang.p + raw_off_p
    local predicted_muzzle_y = base_ang.y + raw_off_y

    local desired_muzzle_dir = (target_pos - cached_muzzle_pos):GetNormalized()
    local desired_muzzle_ang = desired_muzzle_dir:Angle()

    local delta_p = math.AngleDifference(desired_muzzle_ang.p, predicted_muzzle_p)
    local delta_y = math.AngleDifference(desired_muzzle_ang.y, predicted_muzzle_y)

    local total_delta = math.sqrt(delta_p * delta_p + delta_y * delta_y)

    local ft = FrameTime()
    local apply_factor
    if total_delta > 40 then
        apply_factor = math.Clamp(ft * 8, 0.05, 0.3)
    elseif total_delta > 20 then
        apply_factor = math.Clamp(ft * 15, 0.1, 0.5)
    elseif total_delta > 8 then
        apply_factor = math.Clamp(ft * 25, 0.2, 0.8)
    else
        apply_factor = 1.0
    end

    local corrected = Angle(
        math.Clamp(base_ang.p + delta_p * apply_factor, -89, 89),
        math.NormalizeAngle(base_ang.y + delta_y * apply_factor),
        0
    )

    local mode = V.aimbot_mode

    if mode == 1 then
        if V.aa_enabled and SilkWare._aa_initialized then
            SilkWare._aa_cam_pitch = corrected.p
            SilkWare._aa_cam_yaw = corrected.y
        end
        cmd:SetViewAngles(corrected)
        SilkWare._aimbot_real_angles = nil
    elseif mode == 2 then
        SilkWare._aimbot_real_angles = Angle(base_ang.p, base_ang.y, 0)

        local fwd = cmd:GetForwardMove()
        local side = cmd:GetSideMove()

        cmd:SetViewAngles(corrected)

        if fwd ~= 0 or side ~= 0 then
            local real_ang = Angle(0, base_ang.y, 0)
            local aim_ang = Angle(0, corrected.y, 0)

            local real_fwd = real_ang:Forward()
            local real_right = real_ang:Right()
            local aim_fwd = aim_ang:Forward()
            local aim_right = aim_ang:Right()

            local wish_dir = real_fwd * fwd + real_right * side
            cmd:SetForwardMove(wish_dir:Dot(aim_fwd))
            cmd:SetSideMove(wish_dir:Dot(aim_right))
        end
    end

    if V.aimbot_autofire then
        if total_delta * apply_factor < 1.5 then
            cmd:SetButtons(bit.bor(cmd:GetButtons(), IN_ATTACK))
        end
    end
end)

local function DrawAimbotSection(gx, gy, gw, gh)
    local y = gy + 4
    local w = gw

    y = DrawKeybinder(gx, y, w, "Enable Aimbot", "aimbot_key")
    y = y + 2
    y = DrawDropdown(gx, y, w, "Mode", "aimbot_mode", aimbot_mode_options)
    y = y + 4
    y = DrawSlider(gx, y, w, "FOV", "aimbot_fov", 0, 180, 1, 0, "°")
    y = y + 2
    y = DrawDropdown(gx, y, w, "Target Priority", "aimbot_priority", aimbot_priority_options)
    y = y + 4
    y = DrawCheckbox(gx, y, w, "AutoFire", "aimbot_autofire")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "Visibility Check", "aimbot_vischeck")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "Team Check", "aimbot_teamcheck")
    y = y + 4
    y = DrawDropdown(gx, y, w, "Target Hitbox", "aimbot_hitbox", aimbot_hitbox_options)
end

local function DrawWeaponSettingsSection(gx, gy, gw, gh)
    local y = gy + 4
    local w = gw

    y = DrawCheckbox(gx, y, w, "No Recoil", "no_recoil")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "No Spread", "no_spread")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "No Sway", "no_sway")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "Fast Zoom", "fast_zoom")
end

local function DrawCombatContent(x, y, w, h)
    local pad = 10
    local gap = 8
    local cols = 2
    local col_w = (w - pad * 2 - gap * (cols - 1)) / cols

    local sections = {
        {title = "Aimbot",          draw = DrawAimbotSection},
        {title = "Weapon Settings", draw = DrawWeaponSettingsSection},
    }

    for i, sec in ipairs(sections) do
        local gx = x + pad + (i - 1) * (col_w + gap)
        DrawGroupBox(gx, y + pad, col_w, h - pad * 2, sec.title, sec.draw)
    end
end

local box_modes = {"2D", "3D", "Corner 2D", "Corner 3D", "Diamond 2D", "Diamond 3D"}
local trajectory_shapes = {"Square", "Circle"}

local function DrawESPSection(gx, gy, gw, gh)
    local y = gy + 4
    local w = gw

    y = DrawCheckbox(gx, y, w, "Box ESP", "box_enabled")
    if V.box_enabled then
        y = DrawDropdown(gx, y, w, "  Mode", "box_mode", box_modes, 12)
        y = DrawColorPicker(gx, y, w, "  Box Color", "box_color", 12)
    end

    y = y + 4
    y = DrawCheckbox(gx, y, w, "Name", "name_enabled")
    if V.name_enabled then
        y = DrawColorPicker(gx, y, w, "  Name Color", "name_color", 12)
    end

    y = y + 2
    y = DrawCheckbox(gx, y, w, "Usergroup", "usergroup_enabled")
    if V.usergroup_enabled then
        y = DrawColorPicker(gx, y, w, "  Usergroup Color", "usergroup_color", 12)
    end

    y = y + 2
    y = DrawCheckbox(gx, y, w, "Team", "team_enabled")
    if V.team_enabled then
        y = DrawColorPicker(gx, y, w, "  Team Color", "team_color", 12)
    end

    y = y + 2
    y = DrawCheckbox(gx, y, w, "Weapon", "weapon_enabled")
    if V.weapon_enabled then
        y = DrawColorPicker(gx, y, w, "  Weapon Color", "weapon_color", 12)
        y = DrawCheckbox(gx, y, w, "Show Ammo", "weapon_show_ammo", 18)
        y = DrawCheckbox(gx, y, w, "Show Reloading", "weapon_show_reloading", 18)
    end

    y = y + 4
    y = DrawCheckbox(gx, y, w, "Skeleton", "skeleton_enabled")
    if V.skeleton_enabled then
        y = DrawColorPicker(gx, y, w, "  Skeleton Color", "skeleton_color", 12)
        y = DrawCheckbox(gx, y, w, "Show Local Player", "skeleton_show_local", 18)
    end

    y = y + 2
    y = DrawCheckbox(gx, y, w, "Hitboxes", "hitboxes_enabled")
    if V.hitboxes_enabled then
        y = DrawColorPicker(gx, y, w, "  Hitboxes Color", "hitboxes_color", 12)
        y = DrawCheckbox(gx, y, w, "Show Local Player", "hitboxes_show_local", 18)
    end

    y = y + 6
    y = DrawCheckbox(gx, y, w, "Health Bar (Blood)", "healthbar_enabled")
    if V.healthbar_enabled then
        y = DrawColorPicker(gx, y, w, "  Full Color", "healthbar_color_full", 12)
        y = DrawColorPicker(gx, y, w, "  Empty Color", "healthbar_color_empty", 12)
    end

    y = y + 4
    y = DrawCheckbox(gx, y, w, "Organism Info", "organism_info_enabled")
    if V.organism_info_enabled then
        y = DrawColorPicker(gx, y, w, "  Info Color", "organism_info_color", 12)
    end

    y = y + 4
    y = DrawCheckbox(gx, y, w, "Show Inventory", "show_inventory_enabled")
    if V.show_inventory_enabled then
        y = DrawColorPicker(gx, y, w, "  Inventory Color", "show_inventory_color", 12)
    end
end

local function DrawWorldSection(gx, gy, gw, gh)
    local y = gy + 4
    local w = gw

    y = DrawCheckbox(gx, y, w, "Custom Skybox Color", "custom_skybox_enabled")
    if V.custom_skybox_enabled then
        y = DrawColorPicker(gx, y, w, "  Sky Color", "custom_skybox_color", 12)
    end

    y = y + 4
    y = DrawCheckbox(gx, y, w, "Disable 3D Skybox", "disable_3d_skybox")

    y = y + 4
    y = DrawCheckbox(gx, y, w, "World Color", "world_color_enabled")
    if V.world_color_enabled then
        y = DrawColorPicker(gx, y, w, "  World Color", "world_color", 12)
    end
end

local function DrawScreenSection(gx, gy, gw, gh)
    local y = gy + 4
    local w = gw

    y = DrawCheckbox(gx, y, w, "Custom FOV", "custom_fov_enabled")
    if V.custom_fov_enabled then
        y = DrawSlider(gx, y, w, "  FOV", "custom_fov", 0, 160, 1, 12, "°")
    end

    y = y + 4
    y = DrawCheckbox(gx, y, w, "Thirdperson", "thirdperson_enabled")
    if V.thirdperson_enabled then
        y = DrawSlider(gx, y, w, "  Distance", "thirdperson_distance", 30, 300, 1, 12)
        y = DrawSlider(gx, y, w, "  Right Offset", "thirdperson_right_offset", -60, 60, 1, 12)
    end

    y = y + 6
    y = DrawCheckbox(gx, y, w, "Hide Armor Overlay", "hide_armor_overlay")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "No Flash", "no_flash")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "No Tinnitus", "no_tinnitus")
    y = y + 2
    y = DrawCheckbox(gx, y, w, "No Suppression", "no_suppression")

    y = y + 6
    y = DrawCheckbox(gx, y, w, "Show Trajectory", "trajectory_enabled")
    if V.trajectory_enabled then
        y = DrawDropdown(gx, y, w, "  Shape", "trajectory_shape", trajectory_shapes, 12)
        y = DrawSlider(gx, y, w, "  Size", "trajectory_size", 2, 30, 1, 12, "px")
        y = DrawColorPicker(gx, y, w, "  Color", "trajectory_color", 12)
        y = DrawCheckbox(gx, y, w, "  Outline", "trajectory_outline", 12)
        if V.trajectory_outline then
            y = DrawColorPicker(gx, y, w, "    Outline Color", "trajectory_outline_color", 24)
        end
    end

    y = y + 6
    y = DrawCheckbox(gx, y, w, "Aimbot FOV Circle", "aimbot_fov_circle_enabled")
    if V.aimbot_fov_circle_enabled then
        y = DrawColorPicker(gx, y, w, "  Circle Color", "aimbot_fov_circle_color", 12)
        y = DrawDropdown(gx, y, w, "  Shape", "aimbot_fov_circle_shape", {"Circle", "Square"}, 12)
    end
end

local function DrawVisualsContent(x, y, w, h)
    local pad = 10
    local gap = 8
    local cols = 3
    local col_w = (w - pad * 2 - gap * (cols - 1)) / cols

    local sections = {
        {title = "ESP",    draw = DrawESPSection},
        {title = "World",  draw = DrawWorldSection},
        {title = "Screen", draw = DrawScreenSection},
    }

    for i, sec in ipairs(sections) do
        local gx = x + pad + (i - 1) * (col_w + gap)
        DrawGroupBox(gx, y + pad, col_w, h - pad * 2, sec.title, sec.draw)
    end
end

local aa_pitch_options = {"None", "Down", "Up", "Zero", "Jitter", "Random", "Custom"}
local aa_yaw_options = {"None", "Backward", "Sideways L", "Sideways R", "Spin", "Jitter", "Random", "Custom"}
local aa_preset_options = {"None", "Legit AA", "Rage AA", "Spin Bot", "Jitter Bot", "Crazy", "Static"}

local function DrawAntiAimSection(gx, gy, gw, gh)
    local y = gy + 4
    local w = gw

    y = DrawCheckbox(gx, y, w, "Enable Anti-Aim", "aa_enabled")

    if V.aa_enabled then
        y = y + 4
        y = DrawDropdown(gx, y, w, "Preset", "aa_preset", aa_preset_options)

        y = y + 6
        y = DrawDropdown(gx, y, w, "Pitch", "aa_pitch_mode", aa_pitch_options)

        if V.aa_pitch_mode == 7 then
            y = DrawSlider(gx, y, w, "  Custom Pitch", "aa_custom_pitch", -89, 89, 1, 12, "°")
        end

        y = y + 4
        y = DrawDropdown(gx, y, w, "Yaw", "aa_yaw_mode", aa_yaw_options)

        if V.aa_yaw_mode == 8 then
            y = DrawSlider(gx, y, w, "  Custom Yaw", "aa_custom_yaw", -180, 180, 1, 12, "°")
        end

        if V.aa_yaw_mode == 5 or V.aa_preset == 4 then
            y = y + 4
            y = DrawSlider(gx, y, w, "Spin Speed", "aa_spin_speed", 1, 50, 1, 0)
        end

        if V.aa_pitch_mode == 5 or V.aa_yaw_mode == 6 or V.aa_preset == 5 then
            y = y + 4
            y = DrawSlider(gx, y, w, "Jitter Range", "aa_jitter_range", 10, 180, 1, 0, "°")
        end
    end
end

local function DrawRageContent(x, y, w, h)
    local pad = 12
    local col_w = 350

    local gx = x + (w - col_w) / 2
    local gy = y + pad
    local gh = h - pad * 2

    DrawGroupBox(gx, gy, col_w, gh, "Anti-Aim", DrawAntiAimSection)
end

SilkWare._aa_spin_angle = SilkWare._aa_spin_angle or 0
SilkWare._aa_suppressed = false
SilkWare._aa_active = false
SilkWare._aa_jitter_state = false
SilkWare._aa_jitter_timer = 0
SilkWare._aa_cam_pitch = SilkWare._aa_cam_pitch or nil
SilkWare._aa_cam_yaw = SilkWare._aa_cam_yaw or nil
SilkWare._aa_initialized = SilkWare._aa_initialized or false
SilkWare._aa_cam_height = SilkWare._aa_cam_height or 64
SilkWare._aa_duck_pressed = false
SilkWare._aa_was_enabled = SilkWare._aa_was_enabled or false
SilkWare._aa_suppress_start = false

local AA_HEIGHT_STAND = 64
local AA_HEIGHT_CROUCH = 28

local function AA_GetRealAngles()
    return Angle(SilkWare._aa_cam_pitch or 0, SilkWare._aa_cam_yaw or 0, 0)
end

local function AA_GetStableOrigin(ply)
    if not IsValid(ply) then return ply:EyePos() end
    local pos = ply:GetPos()
    local target_height = SilkWare._aa_duck_pressed and AA_HEIGHT_CROUCH or AA_HEIGHT_STAND
    SilkWare._aa_cam_height = SilkWare._aa_cam_height or target_height
    SilkWare._aa_cam_height = Lerp(FrameTime() * 14, SilkWare._aa_cam_height, target_height)
    return pos + Vector(0, 0, SilkWare._aa_cam_height)
end

local function ComputeAAAngles(real_angles)
    local pitch = real_angles.p
    local yaw = real_angles.y
    local preset = V.aa_preset
    local pitch_mode = V.aa_pitch_mode
    local yaw_mode = V.aa_yaw_mode
    local ct = _CRT()

    if ct > SilkWare._aa_jitter_timer then
        SilkWare._aa_jitter_state = not SilkWare._aa_jitter_state
        SilkWare._aa_jitter_timer = ct + 0.05
    end
    local jflip = SilkWare._aa_jitter_state

    if preset == 2 then
        pitch = math.Clamp(real_angles.p + 15, -89, 89)
        yaw = real_angles.y + (jflip and 30 or -30)
    elseif preset == 3 then
        pitch = 89
        yaw = real_angles.y + 180 + (jflip and 58 or -58)
    elseif preset == 4 then
        pitch = 89
        SilkWare._aa_spin_angle = SilkWare._aa_spin_angle + V.aa_spin_speed * FrameTime() * 60
        yaw = SilkWare._aa_spin_angle
    elseif preset == 5 then
        pitch = 89
        local range = V.aa_jitter_range
        yaw = real_angles.y + 180 + (jflip and range or -range)
    elseif preset == 6 then
        pitch = math.Rand(-89, 89)
        yaw = math.Rand(-180, 180)
    elseif preset == 7 then
        pitch = 89
        yaw = real_angles.y + 180
    else
        if pitch_mode == 2 then pitch = 89
        elseif pitch_mode == 3 then pitch = -89
        elseif pitch_mode == 4 then pitch = 0
        elseif pitch_mode == 5 then pitch = jflip and 89 or -89
        elseif pitch_mode == 6 then pitch = math.Rand(-89, 89)
        elseif pitch_mode == 7 then pitch = V.aa_custom_pitch
        end

        if yaw_mode == 2 then yaw = real_angles.y + 180
        elseif yaw_mode == 3 then yaw = real_angles.y + 90
        elseif yaw_mode == 4 then yaw = real_angles.y - 90
        elseif yaw_mode == 5 then
            SilkWare._aa_spin_angle = SilkWare._aa_spin_angle + V.aa_spin_speed * FrameTime() * 60
            yaw = SilkWare._aa_spin_angle
        elseif yaw_mode == 6 then
            yaw = real_angles.y + (jflip and V.aa_jitter_range or -V.aa_jitter_range)
        elseif yaw_mode == 7 then yaw = math.Rand(-180, 180)
        elseif yaw_mode == 8 then yaw = real_angles.y + V.aa_custom_yaw
        end
    end

    return Angle(pitch, math.NormalizeAngle(yaw), 0)
end

local function FixMovement(cmd, real_angles, aa_angles)
    local fwd = cmd:GetForwardMove()
    local side = cmd:GetSideMove()
    if fwd == 0 and side == 0 then return end

    local real_ang = Angle(0, real_angles.y, 0)
    local aa_ang = Angle(0, aa_angles.y, 0)

    local real_fwd = real_ang:Forward()
    local real_right = real_ang:Right()
    local aa_fwd = aa_ang:Forward()
    local aa_right = aa_ang:Right()

    local wish_dir = real_fwd * fwd + real_right * side
    cmd:SetForwardMove(wish_dir:Dot(aa_fwd))
    cmd:SetSideMove(wish_dir:Dot(aa_right))
end

RegisterHook("InputMouseApply", HID_INPUTMOUSE, function(cmd, x, y, ang)
    if not SilkWare then return end

    if not V.aa_enabled then
        if SilkWare._aa_was_enabled then
            SilkWare._aa_initialized = false
            SilkWare._aa_active = false
            SilkWare._aa_suppressed = false
            SilkWare._aa_cam_pitch = nil
            SilkWare._aa_cam_yaw = nil
            SilkWare._aa_was_enabled = false
        end
        return
    end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then
        SilkWare._aa_initialized = false
        return
    end

    SilkWare._aa_was_enabled = true

    if not SilkWare._aa_initialized then
        SilkWare._aa_cam_pitch = ang.p
        SilkWare._aa_cam_yaw = ang.y
        SilkWare._aa_initialized = true
        SilkWare._aa_cam_height = lp:Crouching() and AA_HEIGHT_CROUCH or AA_HEIGHT_STAND
    end

    local sens = GetConVar("sensitivity"):GetFloat()
    local m_yaw = GetConVar("m_yaw"):GetFloat()
    local m_pitch = GetConVar("m_pitch"):GetFloat()

    SilkWare._aa_cam_yaw = math.NormalizeAngle(SilkWare._aa_cam_yaw - (x * m_yaw * sens))
    SilkWare._aa_cam_pitch = math.Clamp(SilkWare._aa_cam_pitch + (y * m_pitch * sens), -89, 89)

    if SilkWare._aa_suppressed then
        if SilkWare._aa_suppress_start then
            ang.p = SilkWare._aa_cam_pitch
            ang.y = SilkWare._aa_cam_yaw
            ang.r = 0
            SilkWare._aa_suppress_start = false
        end
        return
    end

    ang.p = SilkWare._aa_cam_pitch
    ang.y = SilkWare._aa_cam_yaw
    ang.r = 0

    return true
end)

RegisterHook("CreateMove", HID_CM_AA, function(cmd)
    if not SilkWare then return end
    if not V.aa_enabled then
        SilkWare._aa_active = false
        SilkWare._aa_suppressed = false
        return
    end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then
        SilkWare._aa_active = false
        SilkWare._aa_suppressed = false
        return
    end

    if not SilkWare._aa_initialized then return end

    SilkWare._aa_duck_pressed = cmd:KeyDown(IN_DUCK)

    local attacking = cmd:KeyDown(IN_ATTACK)
    local aiming = cmd:KeyDown(IN_ATTACK2)

    local aimbot_should_suppress = false
    if V.aimbot_enabled and Aimbot_IsKeyActive() and not menu.open then
        local wep = lp:GetActiveWeapon()
        if IsValid(wep) and wep.ishgweapon then
            local sprinting = wep.IsSprinting and wep:IsSprinting()
            if not sprinting then
                local eye_pos = lp:EyePos()
                local real_angles = AA_GetRealAngles()
                local target, target_pos = Aimbot_FindTargetWithAngles(cmd, lp, eye_pos, real_angles)
                if target and target_pos then
                    aimbot_should_suppress = true
                    SilkWare._aa_found_target = target
                    SilkWare._aa_found_target_pos = target_pos
                end
            end
        end
    end

    if attacking or aiming or aimbot_should_suppress then
        if not SilkWare._aa_suppressed then
            SilkWare._aa_suppress_start = true
        end

        SilkWare._aa_suppressed = true
        SilkWare._aa_active = false
        return
    end

    SilkWare._aa_found_target = nil
    SilkWare._aa_found_target_pos = nil

    if SilkWare._aa_suppressed then
        local current = cmd:GetViewAngles()
        SilkWare._aa_cam_pitch = current.p
        SilkWare._aa_cam_yaw = current.y
    end

    SilkWare._aa_suppressed = false
    SilkWare._aa_active = true

    local real_angles = AA_GetRealAngles()
    local aa_angles = ComputeAAAngles(real_angles)

    FixMovement(cmd, real_angles, aa_angles)

    cmd:SetViewAngles(aa_angles)
end)

RegisterHook("Think", HID_THINK_WEPMODS, function()
    if not SilkWare then return end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end

    local wep = lp:GetActiveWeapon()
    if not IsValid(wep) or not wep.ishgweapon then return end

    if V.no_recoil then
        if wep.EyeSpray then wep.EyeSpray = Angle(0, 0, 0) end
        if wep.EyeSprayVel then wep.EyeSprayVel = Angle(0, 0, 0) end
        if wep.sprayAngles then wep.sprayAngles = Angle(0, 0, 0) end
        wep.norecoil = true
    else
        if wep.norecoil then wep.norecoil = nil end
    end

    if V.no_spread then
        if wep.SprayI then wep.SprayI = 0 end
        if wep.dmgStack then wep.dmgStack = 0 end
        if wep.dmgStack2 then wep.dmgStack2 = 0 end
    end

    if V.fast_zoom then
        if wep.k and wep.Ergonomics and wep.IsZoom and wep:IsZoom() then
            wep.k = 1
        end
    end
end)

SilkWare._trajectory_hitpos = nil
SilkWare._trajectory_valid = false

RegisterHook("CreateMove", HID_CM_WEPMODS, function(cmd)
    if not SilkWare then return end
end)

local function DrawTrajectory()
    if not SilkWare then return end
    if not V.trajectory_enabled then return end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end

    local wep = lp:GetActiveWeapon()
    if not IsValid(wep) or not wep.ishgweapon then return end

    local hitpos = nil

    if wep.GetTrace then
        local tr, pos, ang = wep:GetTrace(true)
        if tr and tr.HitPos then
            hitpos = tr.HitPos
        end
    end

    if not hitpos then return end

    local scr = hitpos:ToScreen()
    if not scr.visible then return end

    local size = V.trajectory_size
    local half = size / 2
    local col = V.trajectory_color
    local shape = V.trajectory_shape

    if shape == 1 then
        if V.trajectory_outline then
            surface.SetDrawColor(V.trajectory_outline_color)
            surface.DrawOutlinedRect(scr.x - half - 1, scr.y - half - 1, size + 2, size + 2)
        end
        surface.SetDrawColor(col)
        surface.DrawRect(scr.x - half, scr.y - half, size, size)
    elseif shape == 2 then
        local segments = math.max(16, size * 2)
        if V.trajectory_outline then
            local outline_r = half + 1
            local points_outline = {}
            for i = 0, segments - 1 do
                local a = (i / segments) * math.pi * 2
                table.insert(points_outline, {x = scr.x + math.cos(a) * outline_r, y = scr.y + math.sin(a) * outline_r})
            end
            surface.SetDrawColor(V.trajectory_outline_color)
            draw.NoTexture()
            surface.DrawPoly(points_outline)
        end

        local points = {}
        for i = 0, segments - 1 do
            local a = (i / segments) * math.pi * 2
            table.insert(points, {x = scr.x + math.cos(a) * half, y = scr.y + math.sin(a) * half})
        end
        surface.SetDrawColor(col)
        draw.NoTexture()
        surface.DrawPoly(points)
    end
end

RegisterHook("Think", HID_THINK_NOSWAY, function()
    if not SilkWare then return end
    if not V.no_sway then return end
    
    if angle_difference ~= nil then
        if isangle(angle_difference) then
            angle_difference:Zero()
        end
    end
    
    if angle_difference_localvec ~= nil then
        if isvector(angle_difference_localvec) then
            angle_difference_localvec:Zero()
        end
    end
    
    if position_difference23 ~= nil then
        if isvector(position_difference23) then
            position_difference23:Zero()
        end
    end
    
    if position_difference3 ~= nil then
        if isvector(position_difference3) then
            position_difference3:Zero()
        end
    end
end)

local function DrawConfigContent(x, y, w, h)
    local pad = 12
    local gap = 12
    local col_w = (w - pad * 3) / 2

    DrawGroupBox(x + pad, y + pad, col_w, h - pad * 2, "Configuration", function(gx, gy, gw, gh)
        local cy = gy + 4

        draw.SimpleText("Config Name:", "SW_Elem", gx + 10, cy + 2, gray5, TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP)
        cy = cy + 16

        local inp_x = gx + 10
        local inp_w = gw - 20
        local inp_h = 22
        local inp_hov = InBox(inp_x, cy, inp_w, inp_h)

        draw.RoundedBox(0, inp_x, cy, inp_w, inp_h, config_typing and black5 or gray1)
        surface.SetDrawColor(config_typing and red or gray2)
        surface.DrawOutlinedRect(inp_x, cy, inp_w, inp_h)

        local disp_text = config_name_buf
        if config_typing and math.floor(_RT() * 2) % 2 == 0 then
            disp_text = disp_text .. "|"
        end
        draw.SimpleText(disp_text ~= "" and disp_text or "type name...", "SW_Small",
            inp_x + 6, cy + inp_h / 2,
            disp_text ~= "" and white or gray3, TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

        if inp_hov and IsClickAvailable() then
            config_typing = true
            ConsumeClick()
        elseif IsClickAvailable() and not inp_hov then
            config_typing = false
        end

        cy = cy + inp_h + 8

        local btn_w = (gw - 30) / 2
        local btn_h = 28
        local buttons = {
            {label = "Save",   action = function() SaveConfig(config_name_buf) end, col = red3, hcol = red2},
            {label = "Load",   action = function() LoadConfig(config_selected or config_name_buf) end, col = red3, hcol = red2},
            {label = "Delete", action = function() DeleteConfig(config_selected or config_name_buf) end, col = Color(100, 20, 20), hcol = Color(180, 40, 40)},
            {label = "Reset",  action = function() ResetConfig() end, col = gray2, hcol = gray3},
        }

        for i, btn in ipairs(buttons) do
            local ci = (i - 1) % 2
            local ri = math.floor((i - 1) / 2)
            local bx = gx + 10 + ci * (btn_w + 10)
            local by = cy + ri * (btn_h + 6)
            local bhov = InBox(bx, by, btn_w, btn_h)

            draw.RoundedBox(0, bx, by, btn_w, btn_h, bhov and btn.hcol or btn.col)
            surface.SetDrawColor(bhov and red or gray2)
            surface.DrawOutlinedRect(bx, by, btn_w, btn_h)
            draw.SimpleText(btn.label, "SW_Elem", bx + btn_w / 2, by + btn_h / 2, white2, TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)

            if bhov and IsClickAvailable() then
                btn.action()
                ConsumeClick()
            end

            cy = cy + 0
        end

        cy = cy + math.ceil(#buttons / 2) * (btn_h + 6) + 8

        local ubw = gw - 20
        local ubh = 32
        local ubx = gx + 10
        local ubhov = InBox(ubx, cy, ubw, ubh)
        draw.RoundedBox(0, ubx, cy, ubw, ubh, ubhov and Color(230, 35, 35) or Color(160, 20, 20))
        surface.SetDrawColor(ubhov and red2 or red)
        surface.DrawOutlinedRect(ubx, cy, ubw, ubh)
        draw.SimpleText("UNLOAD SILKWARE", "SW_Btn", ubx + ubw / 2, cy + ubh / 2, white2, TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)

        if ubhov and IsClickAvailable() then
            _tS(0, Unload)
            ConsumeClick()
        end

        cy = cy + ubh + 6
        draw.SimpleText("Removes all hooks and disables the cheat.", "SW_Small", gx + gw / 2, cy, gray3, TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)

        if config_status ~= "" and config_status_time > _RT() then
            cy = cy + 18
            draw.SimpleText(config_status, "SW_Small", gx + gw / 2, cy, red2, TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
        end
    end)

    DrawGroupBox(x + pad * 2 + col_w, y + pad, col_w, h - pad * 2, "Saved Configs", function(gx, gy, gw, gh)
        local cy = gy + 4

        if #config_list == 0 then
            draw.SimpleText("No configs saved.", "SW_Small", gx + gw / 2, cy + 10, gray3, TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
            return
        end

        for i, name in ipairs(config_list) do
            local item_h = 24
            local ix = gx + 8
            local iw = gw - 16
            local sel = config_selected == name
            local ihov = InBox(ix, cy, iw, item_h)

            draw.RoundedBox(0, ix, cy, iw, item_h, sel and Color(200, 30, 30, 40) or (ihov and Color(255, 255, 255, 10) or Color(0, 0, 0, 0)))
            if sel or ihov then
                surface.SetDrawColor(sel and red or gray2)
                surface.DrawOutlinedRect(ix, cy, iw, item_h)
            end
            draw.SimpleText(name, "SW_Elem", ix + 8, cy + item_h / 2, sel and white2 or (ihov and white or gray5), TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER)

            if ihov and IsClickAvailable() then
                config_selected = name
                config_name_buf = name
                ConsumeClick()
            end

            cy = cy + item_h + 2
            if cy > gy + gh - 10 then break end
        end
    end)
end

local function DrawContent(x, y, w, h)
    local tab = menu.tab

    if tab == 1 then
        DrawCombatContent(x, y, w, h)
        return
    end

    if tab == 2 then
        DrawRageContent(x, y, w, h)
        return
    end

    if tab == 3 then
        DrawVisualsContent(x, y, w, h)
        return
    end

    if tab == 5 then
        DrawConfigContent(x, y, w, h)
        return
    end

    local grps = groups[tab]
    if not grps or #grps == 0 then return end

    local pad = 12
    local gap = 12
    local col_w = (w - pad * 3) / 2

    local col1, col2 = {}, {}
    for _, g in ipairs(grps) do
        if g.col == 1 then table.insert(col1, g)
        else table.insert(col2, g) end
    end

    local cy = y + pad
    local gh1 = (h - pad * 2 - gap * (math.max(1, #col1) - 1)) / math.max(1, #col1)
    for _, g in ipairs(col1) do
        DrawGroupBox(x + pad, cy, col_w, gh1, g.title)
        cy = cy + gh1 + gap
    end

    cy = y + pad
    local gh2 = (h - pad * 2 - gap * (math.max(1, #col2) - 1)) / math.max(1, #col2)
    for _, g in ipairs(col2) do
        DrawGroupBox(x + pad * 2 + col_w, cy, col_w, gh2, g.title)
        cy = cy + gh2 + gap
    end
end

local function DrawMenu()
    if not menu.open then return end

    local x, y, w, h = menu.x, menu.y, menu.w, menu.h

    if InBox(x, y, w, TITLE_H) and IsClickAvailable() and not config_typing then
        menu.drag = true
        menu.dx = mx - x
        menu.dy = my - y
        ConsumeClick()
    end
    if menu.drag then
        if hold then
            menu.x = mx - menu.dx
            menu.y = my - menu.dy
            menu.x = math.Clamp(menu.x, -w + 100, ScrW() - 100)
            menu.y = math.Clamp(menu.y, 0, ScrH() - 50)
            x, y = menu.x, menu.y
        else
            menu.drag = false
        end
    end

    for i = 1, 20 do
        surface.SetDrawColor(Color(0, 0, 0, (1 - i / 20) * 40))
        surface.DrawRect(x - i * 2, y - i * 2, w + i * 4, h + i * 4)
    end

    surface.SetDrawColor(black)
    surface.DrawRect(x, y, w, h)
    surface.SetDrawColor(gray1)
    surface.DrawOutlinedRect(x, y, w, h)
    surface.SetDrawColor(Color(5, 5, 5, 255))
    surface.DrawOutlinedRect(x - 1, y - 1, w + 2, h + 2)

    DrawTitle(x, y, w)
    DrawTabs(x, y + TITLE_H, w)

    render.SetScissorRect(x, y + HEADER_H, x + w, y + h, true)
    DrawContent(x, y + HEADER_H, w, h - HEADER_H)
    render.SetScissorRect(0, 0, 0, 0, false)
end

local function GetESPTarget(ply)
    if not IsValid(ply) then return nil end
    if not ply:Alive() then return nil end

    local ent = ply
    local ragdoll = ply:GetNWEntity("FakeRagdoll")
    if IsValid(ragdoll) then
        ent = ragdoll
    elseif ply.GetRagdollEntity and IsValid(ply:GetRagdollEntity()) then
        ent = ply:GetRagdollEntity()
    end

    if ent == ply and ply:GetMoveType() == MOVETYPE_OBSERVER then
        for _, e in ipairs(ents.FindByClass("prop_ragdoll")) do
            if e:GetNWEntity("Player") == ply then
                ent = e
                break
            end
        end
    end

    return ent
end

local function GetOrganismInfo(ply)
    local info = {}
    if not IsValid(ply) or not ply:IsPlayer() then return info end
    local org = ply.organism
    if not org then return info end

    if org.otrub then
        table.insert(info, {text = "UNCONSCIOUS", color = Color(255, 100, 100)})
    end
    if org.larmamputated then
        table.insert(info, {text = "L-ARM AMPUTATED", color = Color(255, 50, 50)})
    end
    if org.rarmamputated then
        table.insert(info, {text = "R-ARM AMPUTATED", color = Color(255, 50, 50)})
    end
    if org.lleg and org.lleg >= 1 then
        table.insert(info, {text = "L-LEG BROKEN", color = Color(255, 100, 50)})
    elseif org.lleg and org.lleg >= 0.5 then
        table.insert(info, {text = "L-LEG DAMAGED", color = Color(255, 150, 50)})
    end
    if org.rleg and org.rleg >= 1 then
        table.insert(info, {text = "R-LEG BROKEN", color = Color(255, 100, 50)})
    elseif org.rleg and org.rleg >= 0.5 then
        table.insert(info, {text = "R-LEG DAMAGED", color = Color(255, 150, 50)})
    end
    if org.pelvis and org.pelvis >= 1 then
        table.insert(info, {text = "PELVIS BROKEN", color = Color(255, 50, 50)})
    end
    if org.llegdislocation then
        table.insert(info, {text = "L-LEG DISLOCATED", color = Color(255, 180, 50)})
    end
    if org.rlegdislocation then
        table.insert(info, {text = "R-LEG DISLOCATED", color = Color(255, 180, 50)})
    end

    return info
end

local function GetInventoryInfo(ply)
    local items = {}
    if not IsValid(ply) or not ply:IsPlayer() then return items end

    local inv = ply:GetNetVar("Inventory", {})
    if not inv then return items end

    if inv.Weapons then
        for class, _ in pairs(inv.Weapons) do
            local wep_data = weapons.Get(class)
            local name = wep_data and wep_data.PrintName or class
            table.insert(items, name)
        end
    end

    local armor = ply:GetNetVar("Armor", {})
    if armor then
        for slot, armorName in pairs(armor) do
            local displayName = armorName
            if hg and hg.armorNames and hg.armorNames[armorName] then
                displayName = hg.armorNames[armorName]
            end
            table.insert(items, "[" .. slot .. "] " .. displayName)
        end
    end

    return items
end

local function Get2DBBox(ent)
    if not IsValid(ent) then return nil end

    local mins, maxs = ent:GetCollisionBounds()
    if not mins or not maxs then
        mins = Vector(-16, -16, 0)
        maxs = Vector(16, 16, 72)
    end

    local pos = ent:GetPos()
    local eye_pos = LocalPlayer():EyePos()
    local eye_forward = LocalPlayer():EyeAngles():Forward()

    local to_center = (pos + (mins + maxs) / 2) - eye_pos
    if to_center:Dot(eye_forward) < 10 then return nil end

    local corners = {
        Vector(mins.x, mins.y, mins.z),
        Vector(mins.x, maxs.y, mins.z),
        Vector(maxs.x, mins.y, mins.z),
        Vector(maxs.x, maxs.y, mins.z),
        Vector(mins.x, mins.y, maxs.z),
        Vector(mins.x, maxs.y, maxs.z),
        Vector(maxs.x, mins.y, maxs.z),
        Vector(maxs.x, maxs.y, maxs.z),
    }

    local min_x, min_y = math.huge, math.huge
    local max_x, max_y = -math.huge, -math.huge
    local all_visible = true

    for _, c in ipairs(corners) do
        local world_pos = pos + c
        local dir = world_pos - eye_pos

        if dir:Dot(eye_forward) < 1 then
            all_visible = false
            break
        end

        local screen = world_pos:ToScreen()
        if not screen.visible then
            all_visible = false
            break
        end

        min_x = math.min(min_x, screen.x)
        min_y = math.min(min_y, screen.y)
        max_x = math.max(max_x, screen.x)
        max_y = math.max(max_y, screen.y)
    end

    if not all_visible then return nil end

    local bw = max_x - min_x
    local bh = max_y - min_y
    if bw < 2 then bw = 2 end
    if bh < 2 then bh = 2 end

    local sw, sh = ScrW(), ScrH()
    if bw > sw * 1.5 or bh > sh * 1.5 then return nil end

    return min_x, min_y, bw, bh
end

local function DrawBox2D(bx, by, bw, bh, col)
    surface.SetDrawColor(Color(0, 0, 0, 180))
    surface.DrawOutlinedRect(bx - 1, by - 1, bw + 2, bh + 2)
    surface.DrawOutlinedRect(bx + 1, by + 1, bw - 2, bh - 2)
    surface.SetDrawColor(col)
    surface.DrawOutlinedRect(bx, by, bw, bh)
end

local function DrawCorner2D(bx, by, bw, bh, col)
    local cw = bw * 0.25
    local ch = bh * 0.25
    local sh = Color(0, 0, 0, 180)
    local rx = bx + bw - 1
    local bot = by + bh - 1

    local function L(x1, y1, x2, y2, c)
        surface.SetDrawColor(c)
        surface.DrawLine(x1, y1, x2, y2)
    end

    L(bx-1,by,bx+cw,by,sh)   L(bx,by-1,bx,by+ch,sh)
    L(bx,by,bx+cw,by,col)    L(bx,by,bx,by+ch,col)
    L(rx-cw,by,rx+1,by,sh)   L(rx,by-1,rx,by+ch,sh)
    L(rx-cw,by,rx,by,col)    L(rx,by,rx,by+ch,col)
    L(bx-1,bot,bx+cw,bot,sh) L(bx,bot-ch,bx,bot+1,sh)
    L(bx,bot,bx+cw,bot,col)  L(bx,bot-ch,bx,bot,col)
    L(rx-cw,bot,rx+1,bot,sh) L(rx,bot-ch,rx,bot+1,sh)
    L(rx-cw,bot,rx,bot,col)  L(rx,bot-ch,rx,bot,col)
end

local function DrawDiamond2D(cx, cy, hw, hh, col)
    surface.SetDrawColor(Color(0, 0, 0, 180))
    surface.DrawLine(cx, cy-hh-1, cx+hw+1, cy)
    surface.DrawLine(cx+hw+1, cy, cx, cy+hh+1)
    surface.DrawLine(cx, cy+hh+1, cx-hw-1, cy)
    surface.DrawLine(cx-hw-1, cy, cx, cy-hh-1)
    surface.SetDrawColor(col)
    surface.DrawLine(cx, cy-hh, cx+hw, cy)
    surface.DrawLine(cx+hw, cy, cx, cy+hh)
    surface.DrawLine(cx, cy+hh, cx-hw, cy)
    surface.DrawLine(cx-hw, cy, cx, cy-hh)
end

local function ProjectEntityBox(ent)
    if not IsValid(ent) then return nil end
    local mins, maxs = ent:GetCollisionBounds()
    if not mins or not maxs then return nil end

    local pos = ent:GetPos()
    local ang = ent:GetAngles()
    local eye_pos = LocalPlayer():EyePos()
    local eye_fwd = LocalPlayer():EyeAngles():Forward()

    local corners_local = {
        Vector(mins.x, mins.y, mins.z), Vector(maxs.x, mins.y, mins.z),
        Vector(maxs.x, maxs.y, mins.z), Vector(mins.x, maxs.y, mins.z),
        Vector(mins.x, mins.y, maxs.z), Vector(maxs.x, mins.y, maxs.z),
        Vector(maxs.x, maxs.y, maxs.z), Vector(mins.x, maxs.y, maxs.z),
    }

    local pts = {}
    for i, c in ipairs(corners_local) do
        local world = LocalToWorld(c, Angle(0,0,0), pos, ang)
        if (world - eye_pos):Dot(eye_fwd) < 0 then return nil end
        local s = world:ToScreen()
        pts[i] = s
    end
    return pts
end

local box_edges = {
    {1,2},{2,3},{3,4},{4,1},
    {5,6},{6,7},{7,8},{8,5},
    {1,5},{2,6},{3,7},{4,8},
}

local function Draw3DBox(ent, col)
    local pts = ProjectEntityBox(ent)
    if not pts then return end
    surface.SetDrawColor(Color(0, 0, 0, 150))
    for _, e in ipairs(box_edges) do
        surface.DrawLine(pts[e[1]].x+1, pts[e[1]].y+1, pts[e[2]].x+1, pts[e[2]].y+1)
    end
    surface.SetDrawColor(col)
    for _, e in ipairs(box_edges) do
        surface.DrawLine(pts[e[1]].x, pts[e[1]].y, pts[e[2]].x, pts[e[2]].y)
    end
end

local function Draw3DCorner(ent, col)
    local pts = ProjectEntityBox(ent)
    if not pts then return end
    local frac = 0.25
    surface.SetDrawColor(col)
    for _, e in ipairs(box_edges) do
        local a, b = pts[e[1]], pts[e[2]]
        local dx = (b.x - a.x) * frac
        local dy = (b.y - a.y) * frac
        surface.DrawLine(a.x, a.y, a.x + dx, a.y + dy)
        surface.DrawLine(b.x, b.y, b.x - dx, b.y - dy)
    end
end

local function Draw3DDiamond(ent, col)
    if not IsValid(ent) then return end
    local mins, maxs = ent:GetCollisionBounds()
    if not mins or not maxs then return end

    local pos = ent:GetPos()
    local center_z = (mins.z + maxs.z) / 2
    local hw = (maxs.x - mins.x) / 2
    local hd = (maxs.y - mins.y) / 2

    local diamond_world = {
        pos + Vector(0, 0, maxs.z), pos + Vector(hw, 0, center_z),
        pos + Vector(0, 0, mins.z), pos + Vector(-hw, 0, center_z),
        pos + Vector(0, hd, center_z), pos + Vector(0, -hd, center_z),
    }

    local eye_pos = LocalPlayer():EyePos()
    local eye_fwd = LocalPlayer():EyeAngles():Forward()

    local pts = {}
    for i, p in ipairs(diamond_world) do
        if (p - eye_pos):Dot(eye_fwd) < 0 then return end
        pts[i] = p:ToScreen()
    end

    local diamond_edges = {
        {1,2},{1,4},{1,5},{1,6},{3,2},{3,4},{3,5},{3,6},{2,5},{5,4},{4,6},{6,2},
    }

    surface.SetDrawColor(Color(0, 0, 0, 150))
    for _, e in ipairs(diamond_edges) do
        surface.DrawLine(pts[e[1]].x+1, pts[e[1]].y+1, pts[e[2]].x+1, pts[e[2]].y+1)
    end
    surface.SetDrawColor(col)
    for _, e in ipairs(diamond_edges) do
        surface.DrawLine(pts[e[1]].x, pts[e[1]].y, pts[e[2]].x, pts[e[2]].y)
    end
end

local function DrawSkeletonWorld(ent, col)
    if not IsValid(ent) then return end

    local bone_count = ent:GetBoneCount()
    if not bone_count or bone_count <= 0 then return end

    local check_pos = ent:GetPos()

    local bone_pos = {}
    local bone_valid = {}
    local valid_bone_count = 0

    for i = 0, bone_count - 1 do
        local name = ent:GetBoneName(i)
        if not name or name == "" or name == "__INVALIDBONE__" then continue end
        local mat = ent:GetBoneMatrix(i)
        if not mat then continue end
        local pos = mat:GetTranslation()
        if not pos then continue end
        if pos:IsZero() then continue end
        if pos:DistToSqr(check_pos) > 250000 then continue end
        bone_pos[i] = pos
        bone_valid[i] = true
        valid_bone_count = valid_bone_count + 1
    end

    if valid_bone_count < 3 then return end

    local root_bone = nil
    for i = 0, bone_count - 1 do
        if not bone_valid[i] then continue end
        local parent = ent:GetBoneParent(i)
        if not parent or parent < 0 then root_bone = i break end
    end

    for i = 0, bone_count - 1 do
        if not bone_valid[i] then continue end
        local parent = ent:GetBoneParent(i)
        if not parent or parent < 0 then continue end
        if not bone_valid[parent] then continue end

        if parent == root_bone then
            local name = ent:GetBoneName(i)
            local name_lower = string.lower(name)
            local is_real_child = string.find(name_lower, "pelvis") or string.find(name_lower, "spine") or string.find(name_lower, "hip")
            if not is_real_child then continue end
        end

        local pos = bone_pos[i]
        local ppos = bone_pos[parent]
        if not pos or not ppos then continue end
        if pos:DistToSqr(ppos) < 0.01 then continue end

        local s1 = pos:ToScreen()
        local s2 = ppos:ToScreen()
        if not s1.visible or not s2.visible then continue end

        surface.SetDrawColor(Color(0, 0, 0, 150))
        surface.DrawLine(s1.x + 1, s1.y + 1, s2.x + 1, s2.y + 1)
        surface.SetDrawColor(col)
        surface.DrawLine(s1.x, s1.y, s2.x, s2.y)
    end
end

local hitbox_edges = {
    {1,2},{2,3},{3,4},{4,1},{5,6},{6,7},{7,8},{8,5},{1,5},{2,6},{3,7},{4,8},
}

local function DrawHitboxesWorld(ent, col)
    if not IsValid(ent) then return end

    local set = ent:GetHitboxSet() or 0
    local count = ent:GetHitBoxCount(set)
    if not count then return end

    local check_pos = ent:GetPos()

    for i = 0, count - 1 do
        local bone = ent:GetHitBoxBone(i, set)
        if not bone then continue end

        local mat = ent:GetBoneMatrix(bone)
        if not mat then continue end

        local bone_pos = mat:GetTranslation()
        local bone_ang = mat:GetAngles()

        if not bone_pos or not bone_ang then continue end
        if bone_pos:IsZero() then continue end
        if bone_pos:DistToSqr(check_pos) > 250000 then continue end

        local hb_mins, hb_maxs = ent:GetHitBoxBounds(i, set)
        if not hb_mins or not hb_maxs then continue end

        local corners = {
            Vector(hb_mins.x, hb_mins.y, hb_mins.z), Vector(hb_maxs.x, hb_mins.y, hb_mins.z),
            Vector(hb_maxs.x, hb_maxs.y, hb_mins.z), Vector(hb_mins.x, hb_maxs.y, hb_mins.z),
            Vector(hb_mins.x, hb_mins.y, hb_maxs.z), Vector(hb_maxs.x, hb_mins.y, hb_maxs.z),
            Vector(hb_maxs.x, hb_maxs.y, hb_maxs.z), Vector(hb_mins.x, hb_maxs.y, hb_maxs.z),
        }

        local pts = {}
        local all_ok = true
        for j, c in ipairs(corners) do
            local world = LocalToWorld(c, Angle(0, 0, 0), bone_pos, bone_ang)
            local s = world:ToScreen()
            if not s.visible then all_ok = false break end
            pts[j] = s
        end
        if not all_ok then continue end

        local min_sx, max_sx = math.huge, -math.huge
        local min_sy, max_sy = math.huge, -math.huge
        for _, p in ipairs(pts) do
            min_sx = math.min(min_sx, p.x) max_sx = math.max(max_sx, p.x)
            min_sy = math.min(min_sy, p.y) max_sy = math.max(max_sy, p.y)
        end
        local sw, sh = ScrW(), ScrH()
        if (max_sx - min_sx) > sw * 0.5 or (max_sy - min_sy) > sh * 0.5 then continue end

        surface.SetDrawColor(Color(0, 0, 0, 100))
        for _, e in ipairs(hitbox_edges) do
            surface.DrawLine(pts[e[1]].x + 1, pts[e[1]].y + 1, pts[e[2]].x + 1, pts[e[2]].y + 1)
        end
        surface.SetDrawColor(col)
        for _, e in ipairs(hitbox_edges) do
            surface.DrawLine(pts[e[1]].x, pts[e[1]].y, pts[e[2]].x, pts[e[2]].y)
        end
    end
end

local function DrawHealthBar(bx, by, bw, bh, ply)
    if not IsValid(ply) or not ply:IsPlayer() then return end
    local org = ply.organism
    if not org then return end

    local blood = org.blood or 5000
    local max_blood = 5000
    local frac = math.Clamp(blood / max_blood, 0, 1)

    local bar_w = 4
    local bar_x = bx - bar_w - 3
    local bar_h = bh

    surface.SetDrawColor(Color(0, 0, 0, 180))
    surface.DrawRect(bar_x - 1, by - 1, bar_w + 2, bar_h + 2)

    local fill_h = bar_h * frac
    local fill_y = by + bar_h - fill_h

    local col = Color(
        Lerp(frac, V.healthbar_color_empty.r, V.healthbar_color_full.r),
        Lerp(frac, V.healthbar_color_empty.g, V.healthbar_color_full.g),
        Lerp(frac, V.healthbar_color_empty.b, V.healthbar_color_full.b)
    )

    surface.SetDrawColor(col)
    surface.DrawRect(bar_x, fill_y, bar_w, fill_h)
    surface.SetDrawColor(gray2)
    surface.DrawOutlinedRect(bar_x - 1, by - 1, bar_w + 2, bar_h + 2)

    local pct = math.Round(frac * 100)
    if pct < 100 then
        draw.SimpleText(pct .. "%", "SW_ESP_Tiny", bar_x + bar_w / 2, by - 8, col, TEXT_ALIGN_CENTER, TEXT_ALIGN_BOTTOM)
    end
end

local function GetTeamDisplayInfo(ply)
    local mode = GetCurrentGameMode()

    if mode == "tdm" or mode == "cstrike" then
        local team_id = ply:Team()
        local tname = "Unknown"
        local tcol = Color(200, 200, 200)

        if team_id == 0 then
            tname = "Terrorist"
            tcol = Color(190, 0, 0)
        elseif team_id == 1 then
            tname = "Counter-Terrorist"
            tcol = Color(0, 120, 190)
        elseif team_id == TEAM_SPECTATOR then
            tname = "Spectator"
            tcol = Color(150, 150, 150)
        end

        return tname, tcol
    end

    if mode == "dm" then
        return "Fighter", Color(190, 15, 15)
    end

    if mode == "hmcd" then
        local role_name = nil
        local role_color = nil

        if ply.GetNWString then
            local nw_role = ply:GetNWString("ZBRole", "")
            if nw_role and nw_role ~= "" then
                role_name = nw_role
            end
        end

        if ply.GetNWVector then
            local nw_role_col = ply:GetNWVector("ZBRoleColor", Vector(-1, -1, -1))
            if nw_role_col.x >= 0 then
                role_color = Color(nw_role_col.x, nw_role_col.y, nw_role_col.z)
            end
        end

        local lp = LocalPlayer()
        if ply == lp then
            if lp.isTraitor then
                role_name = role_name or "Traitor"
                role_color = role_color or Color(190, 0, 0)
            elseif lp.isGunner then
                role_name = role_name or "Gunner"
                role_color = role_color or Color(158, 0, 190)
            else
                role_name = role_name or "Innocent"
                role_color = role_color or Color(0, 120, 190)
            end
        end

        if not role_name or role_name == "" then
            local raw_name = team.GetName(ply:Team())
            if raw_name and raw_name ~= "" and not string.find(raw_name, "^Team #") and not string.find(raw_name, "^Unassigned") then
                role_name = raw_name
            else
                role_name = "Unknown"
            end
        end

        if not role_color then
            local raw_col = team.GetColor(ply:Team())
            if raw_col and (raw_col.r ~= 255 or raw_col.g ~= 255 or raw_col.b ~= 255) then
                role_color = raw_col
            else
                role_color = Color(200, 200, 200)
            end
        end

        return role_name, role_color
    end

    local raw_name = team.GetName(ply:Team())
    local raw_col = team.GetColor(ply:Team())

    if not raw_name or raw_name == "" or string.find(raw_name, "^Team #") or raw_name == "Unassigned" or raw_name == "Player" then
        raw_name = "Team " .. tostring(ply:Team())
    end

    if not raw_col then
        raw_col = Color(200, 200, 200)
    end

    return raw_name, raw_col
end

local function DrawESPWorld()
    if not SilkWare then return end

    local any = V.box_enabled or V.name_enabled or V.usergroup_enabled
        or V.team_enabled or V.weapon_enabled or V.skeleton_enabled or V.hitboxes_enabled
        or V.healthbar_enabled or V.organism_info_enabled or V.show_inventory_enabled
    if not any then return end

    local lp = LocalPlayer()

    for _, ply in ipairs(player.GetAll()) do
        if ply == lp then continue end
        if not ply:Alive() then continue end

        local ent = GetESPTarget(ply)
        if not IsValid(ent) then continue end

        ent:SetupBones()

        local bx, by, bw, bh = Get2DBBox(ent)

        if V.skeleton_enabled then
            DrawSkeletonWorld(ent, V.skeleton_color)
        end

        if V.hitboxes_enabled then
            DrawHitboxesWorld(ent, V.hitboxes_color)
        end

        if not bx then continue end

        local box_cx = bx + bw / 2

        if V.healthbar_enabled then
            DrawHealthBar(bx, by, bw, bh, ply)
        end

        if V.box_enabled then
            local mode = V.box_mode
            local col = V.box_color
            if mode == 1 then DrawBox2D(bx, by, bw, bh, col)
            elseif mode == 2 then Draw3DBox(ent, col)
            elseif mode == 3 then DrawCorner2D(bx, by, bw, bh, col)
            elseif mode == 4 then Draw3DCorner(ent, col)
            elseif mode == 5 then DrawDiamond2D(box_cx, by + bh / 2, bw / 2, bh / 2, col)
            elseif mode == 6 then Draw3DDiamond(ent, col)
            end
        end

        local top_y = by - 2
        local top_items = {}

        if V.organism_info_enabled then
            local org_info = GetOrganismInfo(ply)
            for i = #org_info, 1, -1 do
                table.insert(top_items, {text = org_info[i].text, color = org_info[i].color, font = "SW_ESP_Tiny"})
            end
        end

        if V.team_enabled then
            local tname, tcol = GetTeamDisplayInfo(ply)
            table.insert(top_items, {text = tname, color = tcol, font = "SW_ESP_Info"})
        end

        if V.usergroup_enabled then
            table.insert(top_items, {text = ply:GetUserGroup(), color = V.usergroup_color, font = "SW_ESP_Info"})
        end

        if V.name_enabled then
            table.insert(top_items, {text = ply:Nick(), color = V.name_color, font = "SW_ESP_Name"})
        end

        for i = 1, #top_items do
            local item = top_items[i]
            top_y = top_y - 13
            draw.SimpleText(item.text, item.font, box_cx + 1, top_y + 1, Color(0,0,0,200), TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
            draw.SimpleText(item.text, item.font, box_cx, top_y, item.color, TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
        end

        local bot_y = by + bh + 2

        if V.weapon_enabled then
            local wep = ply:GetActiveWeapon()
            if IsValid(wep) then
                local wep_name = wep:GetPrintName() or wep:GetClass()
                local extra_parts = {}

                if V.weapon_show_ammo then
                    local clip = wep:Clip1()
                    if clip >= 0 then
                        local reserve = ply:GetAmmoCount(wep:GetPrimaryAmmoType())
                        table.insert(extra_parts, clip .. "/" .. reserve)
                    end
                end

                if V.weapon_show_reloading then
                    local seq = ply:GetSequence()
                    if seq then
                        local seq_name = ply:GetSequenceName(seq)
                        if seq_name and string.find(string.lower(seq_name), "reload") then
                            table.insert(extra_parts, "RELOADING")
                        end
                    end
                end

                local full = wep_name
                if #extra_parts > 0 then
                    full = full .. " (" .. table.concat(extra_parts, "/") .. ")"
                end

                draw.SimpleText(full, "SW_ESP_Info", box_cx + 1, bot_y + 1, Color(0,0,0,200), TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
                draw.SimpleText(full, "SW_ESP_Info", box_cx, bot_y, V.weapon_color, TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
                bot_y = bot_y + 12
            end
        end

        if V.show_inventory_enabled then
            local inv_items = GetInventoryInfo(ply)
            for i, item_name in ipairs(inv_items) do
                if i > 8 then
                    draw.SimpleText("... +" .. (#inv_items - 8) .. " more", "SW_ESP_Tiny", box_cx + 1, bot_y + 1, Color(0,0,0,180), TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
                    draw.SimpleText("... +" .. (#inv_items - 8) .. " more", "SW_ESP_Tiny", box_cx, bot_y, V.show_inventory_color, TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
                    break
                end
                draw.SimpleText(item_name, "SW_ESP_Tiny", box_cx + 1, bot_y + 1, Color(0,0,0,180), TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
                draw.SimpleText(item_name, "SW_ESP_Tiny", box_cx, bot_y, V.show_inventory_color, TEXT_ALIGN_CENTER, TEXT_ALIGN_TOP)
                bot_y = bot_y + 10
            end
        end
    end
end

local function NeedLocalBones()
    if not SilkWare then return false end
    return (V.skeleton_enabled and V.skeleton_show_local) or (V.hitboxes_enabled and V.hitboxes_show_local)
end

local function GetLocalPlayerEntity()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return nil end
    local ragdoll = lp:GetNWEntity("FakeRagdoll")
    if IsValid(ragdoll) then return ragdoll end
    if IsValid(lp.FakeRagdoll) then return lp.FakeRagdoll end
    return lp
end

SilkWare._local_skeleton_lines = nil
SilkWare._local_hitbox_boxes = nil

RegisterHook("ShouldDrawLocalPlayer", HID_SHOULDDRAW, function()
    if not SilkWare then return end
    if SilkWare._tp_active then return true end
end)

RegisterHook("PostRender", HID_POSTRENDER_BONES, function()
    if not SilkWare then return end

    SilkWare._local_skeleton_lines = nil
    SilkWare._local_hitbox_boxes = nil

    if not NeedLocalBones() then return end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end

    local ent = GetLocalPlayerEntity()
    if not IsValid(ent) then return end

    local saved_norender = ent.norender
    ent.norender = nil

    local bone_count = ent:GetBoneCount() or 0
    if bone_count < 1 then
        ent.norender = saved_norender
        return
    end

    local check_pos = ent:GetPos()

    if V.skeleton_enabled and V.skeleton_show_local then
        local bone_positions = {}
        local bone_valids = {}
        local valid_count = 0

        for i = 0, bone_count - 1 do
            local name = ent:GetBoneName(i)
            if not name or name == "" or name == "__INVALIDBONE__" then continue end
            local mat = ent:GetBoneMatrix(i)
            if not mat then continue end
            local pos = mat:GetTranslation()
            if not pos or pos:IsZero() then continue end
            if pos:DistToSqr(check_pos) > 250000 then continue end
            bone_positions[i] = Vector(pos)
            bone_valids[i] = true
            valid_count = valid_count + 1
        end

        if valid_count >= 3 then
            local root_bone = nil
            for i = 0, bone_count - 1 do
                if not bone_valids[i] then continue end
                local parent = ent:GetBoneParent(i)
                if not parent or parent < 0 then root_bone = i break end
            end

            local lines = {}
            for i = 0, bone_count - 1 do
                if not bone_valids[i] then continue end
                local parent = ent:GetBoneParent(i)
                if not parent or parent < 0 then continue end
                if not bone_valids[parent] then continue end

                if parent == root_bone then
                    local name = ent:GetBoneName(i)
                    local nl = string.lower(name)
                    if not (string.find(nl, "pelvis") or string.find(nl, "spine") or string.find(nl, "hip")) then continue end
                end

                local p1 = bone_positions[i]
                local p2 = bone_positions[parent]
                if p1:DistToSqr(p2) < 0.01 then continue end
                table.insert(lines, {Vector(p1), Vector(p2)})
            end

            if #lines > 0 then
                SilkWare._local_skeleton_lines = lines
            end
        end
    end

    if V.hitboxes_enabled and V.hitboxes_show_local then
        local hset = ent:GetHitboxSet() or 0
        local hcount = ent:GetHitBoxCount(hset) or 0
        local boxes = {}

        for i = 0, hcount - 1 do
            local bone = ent:GetHitBoxBone(i, hset)
            if not bone then continue end
            local mat = ent:GetBoneMatrix(bone)
            if not mat then continue end
            local bpos = mat:GetTranslation()
            local bang = mat:GetAngles()
            if not bpos or not bang then continue end
            if bpos:IsZero() then continue end
            if bpos:DistToSqr(check_pos) > 250000 then continue end

            local hmins, hmaxs = ent:GetHitBoxBounds(i, hset)
            if not hmins or not hmaxs then continue end

            table.insert(boxes, {
                pos = Vector(bpos),
                ang = Angle(bang),
                mins = Vector(hmins),
                maxs = Vector(hmaxs)
            })
        end

        if #boxes > 0 then
            SilkWare._local_hitbox_boxes = boxes
        end
    end

    ent.norender = saved_norender
end)

local function DrawLocalBones()
    if not SilkWare then return end

    local has_skeleton = SilkWare._local_skeleton_lines and #SilkWare._local_skeleton_lines > 0
    local has_hitboxes = SilkWare._local_hitbox_boxes and #SilkWare._local_hitbox_boxes > 0

    if not has_skeleton and not has_hitboxes then return end

    local view_setup = render.GetViewSetup()
    if not view_setup then return end

    cam.Start3D(view_setup.origin, view_setup.angles, view_setup.fov, nil, nil, nil, nil, 1, view_setup.zfar or 10000)

        render.SetColorMaterial()

        if has_skeleton and V.skeleton_enabled and V.skeleton_show_local then
            local col = V.skeleton_color
            local shadow = Color(0, 0, 0, 150)
            for _, line in ipairs(SilkWare._local_skeleton_lines) do
                render.DrawLine(line[1], line[2], shadow, false)
            end
            for _, line in ipairs(SilkWare._local_skeleton_lines) do
                render.DrawLine(line[1], line[2], col, false)
            end
        end

        if has_hitboxes and V.hitboxes_enabled and V.hitboxes_show_local then
            local col = V.hitboxes_color
            for _, box in ipairs(SilkWare._local_hitbox_boxes) do
                render.DrawWireframeBox(box.pos, box.ang, box.mins, box.maxs, col, false)
            end
        end

    cam.End3D()
end

RegisterHook("PostDraw2DSkyBox", HID_POSTSKYBOX, function()
    if not SilkWare then return end
    if not V.custom_skybox_enabled then return end
    cam.Start2D()
        surface.SetDrawColor(V.custom_skybox_color)
        surface.DrawRect(0, 0, ScrW(), ScrH())
    cam.End2D()
end)

RegisterHook("PreDrawSkyBox", HID_PRESKYBOX, function()
    if not SilkWare then return end
    if V.disable_3d_skybox then return true end
end)

RegisterHook("RenderScreenspaceEffects", HID_WORLDCOLOR, function()
    if not SilkWare then return end
    if not V.world_color_enabled then return end

    local col = V.world_color
    DrawColorModify({
        ["$pp_colour_addr"] = (col.r / 255 - 0.5) * 0.1,
        ["$pp_colour_addg"] = (col.g / 255 - 0.5) * 0.1,
        ["$pp_colour_addb"] = (col.b / 255 - 0.5) * 0.1,
        ["$pp_colour_brightness"] = 0,
        ["$pp_colour_contrast"] = 1,
        ["$pp_colour_colour"] = 1,
        ["$pp_colour_mulr"] = col.r / 255,
        ["$pp_colour_mulg"] = col.g / 255,
        ["$pp_colour_mulb"] = col.b / 255,
    })
end)

SilkWare._tp_active = false
SilkWare._tp_was_active = false
SilkWare._view_installed = SilkWare._view_installed or false

local function InstallViewOverride()
    if SilkWare._view_installed then return end

    _pC(_hR, "RenderScene", "jopa")

    if hg and hg.CalcView and not SilkWare._orig_hg_CalcView then
        SilkWare._orig_hg_CalcView = hg.CalcView
    end

    local cv_hooks = _hG()["CalcView"]
    if cv_hooks and cv_hooks["homigrad-view"] and not SilkWare._orig_homigrad_view then
        SilkWare._orig_homigrad_view = cv_hooks["homigrad-view"]
    end

    _hA("CalcView", "homigrad-view", function(ply, origin, angles, fov, znear, zfar)
        if SilkWare and SilkWare._aimbot_real_angles and V.aimbot_mode == 2 then
            angles = Angle(SilkWare._aimbot_real_angles.p, SilkWare._aimbot_real_angles.y, 0)
            ply:SetEyeAngles(angles)
        end

        local aa_on = SilkWare and V.aa_enabled and SilkWare._aa_initialized
        local aa_suppressed = SilkWare and SilkWare._aa_suppressed

        if aa_on and not aa_suppressed then
            local real = AA_GetRealAngles()
            ply:SetEyeAngles(real)
            angles = real
        end

        local view
        if SilkWare._orig_homigrad_view then
            view = SilkWare._orig_homigrad_view(ply, origin, angles, fov, znear, zfar)
        elseif SilkWare._orig_hg_CalcView then
            view = SilkWare._orig_hg_CalcView(ply, origin, angles, fov, znear, zfar)
        end

        if not SilkWare then return view end
        if not IsValid(ply) or ply ~= LocalPlayer() then return view end
        if not ply:Alive() then return view end

        if not view or not istable(view) then
            view = { origin = origin, angles = angles, fov = fov, drawviewer = false }
        end

        if aa_on and not aa_suppressed and not V.thirdperson_enabled then
            view.origin = AA_GetStableOrigin(ply)
            view.angles = AA_GetRealAngles()
        end

        if V.custom_fov_enabled and V.custom_fov > 0 then
            view.fov = V.custom_fov
        end

        if V.thirdperson_enabled then
            local dist = V.thirdperson_distance
            local right_off = V.thirdperson_right_offset
            local eye_pos = ply:EyePos()
            local base_angles
            if aa_on and not aa_suppressed then
                base_angles = AA_GetRealAngles()
            else
                base_angles = view.angles
            end
            local eye_ang = Angle(base_angles.p, base_angles.y, 0)
            local target = eye_pos - eye_ang:Forward() * dist + eye_ang:Right() * right_off
            local tr = util.TraceLine({ start = eye_pos, endpos = target, filter = ply, mask = MASK_SOLID })
            view.origin = tr.HitPos + tr.HitNormal * 5
            view.angles = eye_ang
            view.drawviewer = true
            ply.norender = nil
            local head = ply:LookupBone("ValveBiped.Bip01_Head1")
            if head then ply:ManipulateBoneScale(head, Vector(1, 1, 1)) end
            SilkWare._tp_active = true
        else
            if SilkWare._tp_was_active then
                CleanupPlayerState()
                view.drawviewer = false
            end
            SilkWare._tp_active = false
        end

        SilkWare._tp_was_active = SilkWare._tp_active

        return view
    end)

    RegisterHook("PreRender", HID_PRERENDER_TP, function()
        if not SilkWare then return end
        if not SilkWare._tp_active then return end
        local ply = LocalPlayer()
        if not IsValid(ply) then return end
        ply.norender = nil
        local head = ply:LookupBone("ValveBiped.Bip01_Head1")
        if head then ply:ManipulateBoneScale(head, Vector(1, 1, 1)) end
    end)

    SilkWare._view_installed = true
end

RegisterHook("CalcView", HID_CALCVIEW, function(ply, origin, angles, fov)
    if not SilkWare then return end
    if SilkWare._orig_homigrad_view or SilkWare._orig_hg_CalcView then return end
    if not IsValid(ply) or not ply:Alive() then return end
    if GetViewEntity() ~= ply then return end

    local new_origin = origin
    local new_angles = angles
    local new_fov = fov
    local draw_viewer = false
    local need = false

    if SilkWare._aimbot_real_angles and V.aimbot_mode == 2 then
        new_angles = Angle(SilkWare._aimbot_real_angles.p, SilkWare._aimbot_real_angles.y, 0)
        need = true
    end

    local aa_on = V.aa_enabled and SilkWare._aa_initialized
    local aa_suppressed = SilkWare._aa_suppressed

    if aa_on and not aa_suppressed and not V.thirdperson_enabled then
        new_origin = AA_GetStableOrigin(ply)
        new_angles = AA_GetRealAngles()
        need = true
    elseif aa_on and not aa_suppressed then
        new_angles = AA_GetRealAngles()
        need = true
    end

    if V.custom_fov_enabled and V.custom_fov > 0 then
        new_fov = V.custom_fov
        need = true
    end

    if V.thirdperson_enabled then
        local dist = V.thirdperson_distance
        local right_off = V.thirdperson_right_offset
        local eye_pos = ply:EyePos()
        local base_angles
        if aa_on and not aa_suppressed then
            base_angles = AA_GetRealAngles()
        else
            base_angles = new_angles
        end
        local eye_ang = Angle(base_angles.p, base_angles.y, 0)
        local target = eye_pos - eye_ang:Forward() * dist + eye_ang:Right() * right_off
        local tr = util.TraceLine({ start = eye_pos, endpos = target, filter = ply, mask = MASK_SOLID })
        new_origin = tr.HitPos + tr.HitNormal * 5
        new_angles = eye_ang
        draw_viewer = true
        need = true
        SilkWare._tp_active = true
    else
        if SilkWare._tp_was_active then
            CleanupPlayerState()
        end
        SilkWare._tp_active = false
    end

    SilkWare._tp_was_active = SilkWare._tp_active

    if not need then return end

    return {
        origin = new_origin, angles = new_angles, fov = new_fov,
        drawviewer = draw_viewer, znear = 1,
    }
end)

_tS(0.3 + _mR() * 0.4, function()
    if SilkWare then InstallViewOverride() end
end)

local function InstallArmorOverlayHook()
    local hooks = _hG()["Post Pre Post Processing"]
    if hooks and hooks["renderHelmetThingy"] and not SilkWare._orig_helmet_hook then
        SilkWare._orig_helmet_hook = hooks["renderHelmetThingy"]
        _hA("Post Pre Post Processing", "renderHelmetThingy", function(...)
            if SilkWare and V.hide_armor_overlay then return end
            if SilkWare._orig_helmet_hook then return SilkWare._orig_helmet_hook(...) end
        end)
    end
end

_tS(0.3 + _mR() * 0.5, function()
    if SilkWare then InstallArmorOverlayHook() end
end)

RegisterHook("Think", HID_THINK_NOFLASH, function()
    if not SilkWare then return end
    if not V.no_flash then return end
    if hg and hg.flashes then hg.flashes = {} end
    if amtflashed then amtflashed = 0 end
    if amtflashed2 then amtflashed2 = 0 end
end)

RegisterHook("Think", HID_THINK_NOTINN, function()
    if not SilkWare then return end
    if not V.no_tinnitus then return end
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    if lp.tinnitus then lp.tinnitus = 0 end
    lp:SetDSP(0)
end)

RegisterHook("Think", HID_THINK_NOSUPP, function()
    if not SilkWare then return end
    if not V.no_suppression then return end
    if SIB_suppress then SIB_suppress.Force = 0 end
end)

RegisterHook("Think", HID_THINK_TEXT, function()
    if not SilkWare or not menu.open or not config_typing then return end

    if input.IsKeyDown(KEY_BACKSPACE) then
        if not SilkWare._bs_held then
            config_name_buf = string.sub(config_name_buf, 1, -2)
            SilkWare._bs_held = true
            SilkWare._bs_time = _RT() + 0.4
        elseif _RT() > (SilkWare._bs_time or 0) then
            config_name_buf = string.sub(config_name_buf, 1, -2)
            SilkWare._bs_time = _RT() + 0.05
        end
    else
        SilkWare._bs_held = false
    end

    if input.IsKeyDown(KEY_ENTER) or input.IsKeyDown(KEY_ESCAPE) then
        config_typing = false
        return
    end

    local shift = input.IsKeyDown(KEY_LSHIFT) or input.IsKeyDown(KEY_RSHIFT)

    local key_map = {
        [KEY_A]="a",[KEY_B]="b",[KEY_C]="c",[KEY_D]="d",[KEY_E]="e",[KEY_F]="f",
        [KEY_G]="g",[KEY_H]="h",[KEY_I]="i",[KEY_J]="j",[KEY_K]="k",[KEY_L]="l",
        [KEY_M]="m",[KEY_N]="n",[KEY_O]="o",[KEY_P]="p",[KEY_Q]="q",[KEY_R]="r",
        [KEY_S]="s",[KEY_T]="t",[KEY_U]="u",[KEY_V]="v",[KEY_W]="w",[KEY_X]="x",
        [KEY_Y]="y",[KEY_Z]="z",
        [KEY_0]="0",[KEY_1]="1",[KEY_2]="2",[KEY_3]="3",[KEY_4]="4",
        [KEY_5]="5",[KEY_6]="6",[KEY_7]="7",[KEY_8]="8",[KEY_9]="9",
        [KEY_MINUS]="-",[KEY_EQUAL]="=",[KEY_SPACE]=" ",
    }

    SilkWare._keys = SilkWare._keys or {}
    for k, ch in pairs(key_map) do
        local down = input.IsKeyDown(k)
        if down and not SilkWare._keys[k] then
            if shift then ch = string.upper(ch) end
            if #config_name_buf < 32 then
                config_name_buf = config_name_buf .. ch
            end
        end
        SilkWare._keys[k] = down
    end
end)

local prev_home = false

RegisterHook("Think", HID_THINK_MAIN, function()
    if not SilkWare then return end
    local home = input.IsKeyDown(KEY_HOME)
    if home and not prev_home then
        menu.open = not menu.open
        gui.EnableScreenClicker(menu.open)
        if not menu.open then
            active_dropdown = nil
            active_color_picker = nil
            active_slider = nil
            config_typing = false
            SilkWare._keybind_waiting = false
        end
    end
    prev_home = home

    V.aimbot_enabled = true
end)

RegisterHook("PlayerBindPress", HID_BIND, function(ply, bind, pressed)
    if not SilkWare or not menu.open then return end
    if bind == "invnext" then scroll_acc = -1 return true end
    if bind == "invprev" then scroll_acc = 1 return true end
end)

RegisterHook("CreateMove", HID_CM_MAIN, function(cmd)
    if not SilkWare or not menu.open then return end
    cmd:RemoveKey(IN_ATTACK)
    cmd:RemoveKey(IN_ATTACK2)
end)

local function DrawAimbotFOVCircle()
    if not SilkWare then return end
    if not V.aimbot_fov_circle_enabled then return end
    if not V.aimbot_enabled then return end
    if V.aimbot_fov <= 0 or V.aimbot_fov >= 180 then return end

    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end

    local sw, sh = ScrW(), ScrH()
    local cx, cy = sw / 2, sh / 2

    local view_fov = V.custom_fov_enabled and V.custom_fov or 90
    local half_fov_rad = math.rad(view_fov / 2)
    local half_screen = sw / 2
    local pixels_per_degree = half_screen / math.deg(half_fov_rad)
    local radius = V.aimbot_fov * pixels_per_degree

    local col = V.aimbot_fov_circle_color
    local shape = V.aimbot_fov_circle_shape

    if shape == 1 then
        local segments = math.max(32, math.floor(radius / 2))
        surface.SetDrawColor(col)
        for i = 0, segments - 1 do
            local a1 = (i / segments) * math.pi * 2
            local a2 = ((i + 1) / segments) * math.pi * 2
            surface.DrawLine(
                cx + math.cos(a1) * radius, cy + math.sin(a1) * radius,
                cx + math.cos(a2) * radius, cy + math.sin(a2) * radius
            )
        end
    elseif shape == 2 then
        surface.SetDrawColor(col)
        surface.DrawOutlinedRect(cx - radius, cy - radius, radius * 2, radius * 2)
    end
end

RegisterHook("HUDPaint", HID_HUDPAINT, function()
    if not SilkWare then return end
    UpdateInput()
    DrawMenu()
    DrawESPWorld()
    DrawLocalBones()
    DrawTrajectory()
    DrawAimbotFOVCircle()
end)

menu.x = math.floor(ScrW() / 2 - menu.w / 2)
menu.y = math.floor(ScrH() / 2 - menu.h / 2)
menu.open = false
gui.EnableScreenClicker(false)

print("[SilkWare] Loaded. Press HOME to open.")