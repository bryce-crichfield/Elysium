-- Sound effects shared across scenes, played on the Effects channel. Sfx.Play(Sfx.CLICK).
local Sfx = {
    CLICK = "Sounds/sfx_ui_click.wav",         -- a menu button released over itself
    HOVER = "Sounds/sfx_ui_hover.wav",         -- the pointer moving onto a menu button
    CANCEL = "Sounds/sfx_ui_cancel.wav",       -- backing out (Esc, right-click)
    SELECT = "Sounds/sfx_battle_unit_select.wav",  -- picking up a unit or party member
    ORDER = "Sounds/sfx_battle_unit_order.wav",    -- committing a unit's action
}

function Sfx.Play(sound, volume)
    return PlaySound(sound, volume or 1.0, false, CHANNEL_EFFECTS)
end

return Sfx
