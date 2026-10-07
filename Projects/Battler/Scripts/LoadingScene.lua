---@type SceneScript
-- Loading: shown by the engine while the next scene's <Preload> assets load (Project.xml
-- <LoadingScene>, "Loading" by default; the engine falls back to its own plain one). Fills the
-- Bar as the assets come in and names the one loading under it; the engine opens the scene
-- once they're all in. The layout is Scenes/Loading.xml.
local Widgets = require("Scripts/Components/Widgets")

local Loading = {}

local EASE = 12   -- how fast the bar catches up with the progress

function Loading:Initialize()
    self.shown = 0
end

function Loading:Update(dt)
    Widgets.CenterLayout("Background")
    local loading = SceneLoading()
    if not loading then return end
    self.shown = Widgets.Ease(self.shown, loading.progress, EASE, dt)
    Widgets.SetText("Asset", loading.asset)

    local bar = GetEntityByName("Bar")
    local mat = bar and GetComponent(bar, "Material")
    local meter = mat and mat:Layer("Meter")
    if meter then meter:Set("uProgress", self.shown) end
end

return Loading
