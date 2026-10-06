-- Sound effects shared across scenes, played on the Effects channel. Sfx.Play(Sfx.CLICK).
local Sfx = {
    CLICK = "Sounds/sfx_ui_click.wav",         -- a menu button released over itself
    HOVER = "Sounds/sfx_ui_hover.wav",         -- the pointer moving onto a menu button
    CANCEL = "Sounds/sfx_ui_cancel.wav",       -- backing out (Esc, right-click)
    SELECT = "Sounds/sfx_battle_unit_select.wav",  -- picking up a unit or party member
    ORDER = "Sounds/sfx_battle_unit_order.wav",    -- committing a unit's action
    MANA_GROW = "Sounds/sfx_mana_grow.wav",          -- new crystals slamming into place
    MANA_RECHARGE = "Sounds/sfx_mana_recharge.wav",  -- spent crystals relighting
    MANA_SPEND = "Sounds/sfx_mana_spend_1.wav",      -- a spent crystal popping
    CARD = {                                         -- a card drawn, played or hovered (any one of them)
        "Sounds/sfx_card_play_1.wav", "Sounds/sfx_card_play_2.wav", "Sounds/sfx_card_play_3.wav",
        "Sounds/sfx_card_play_4.wav", "Sounds/sfx_card_play_5.wav", "Sounds/sfx_card_play_6.wav",
    },
}

function Sfx.Play(sound, volume)
    return PlaySound(sound, volume or 1.0, false, CHANNEL_EFFECTS)
end

-- One of `sounds` at random, for variety.
function Sfx.PlayAny(sounds, volume)
    return Sfx.Play(sounds[math.random(#sounds)], volume)
end

return Sfx
