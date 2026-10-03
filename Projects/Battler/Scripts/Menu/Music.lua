-- The background music, shared by every scene: a required module is loaded once, so the
-- track keeps playing across scene changes. A scene says which track it wants; asking for
-- the one already playing changes nothing, so it doesn't restart between menu screens. It plays
-- on the Music channel, so the Music volume setting scales it.

local Music = {
    MENU = "Sounds/Music/Track1.mp3",
    BATTLE = "Sounds/Music/Track2.mp3",
}

local current, currentId = nil, nil

function Music.Play(track)
    if track == current then return end
    Music.Stop()
    current, currentId = track, PlaySound(track, 1.0, true, CHANNEL_MUSIC)
end

function Music.Stop()
    if currentId then StopSound(currentId) end
    current, currentId = nil, nil
end

return Music
