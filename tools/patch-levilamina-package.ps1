$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$recipe = Get-ChildItem -LiteralPath (Join-Path $workspace '.xmake') -Recurse -Filter 'xmake.lua' -File |
    Where-Object { $_.FullName -match '[\\/]repositories[\\/]levimc-repo[\\/]packages[\\/]l[\\/]levilamina[\\/]xmake\.lua$' } |
    Select-Object -First 1

if (-not $recipe) {
    throw 'LeviLamina xmake package recipe was not found after repository update.'
}

$content = Get-Content -LiteralPath $recipe.FullName -Raw
$needle = 'on_install(function(package)'
$injection = @'
on_install(function(package)
    -- clang-cl instantiates MinecraftCommands' implicit destructor while
    -- CommandRegistry is only forward declared in this generated header.
    local source = package:sourcedir()
    local headers = {
        path.join(source, "src/mc/server/commands/MinecraftCommands.h"),
        path.join(source, "src/mc/server/commands/MinecraftCommandsArguments.h")
    }
    for _, header in ipairs(headers) do
        if os.isfile(header) then
            local text = io.readfile(header)
            local include = '#include "mc/server/commands/CommandRegistry.h"'
            if not text:find(include, 1, true) then
                text = text:gsub('#include "mc/_HeaderOutputPredefine.h"',
                    '#include "mc/_HeaderOutputPredefine.h"\n' .. include, 1)
                io.writefile(header, text)
            end
        end
    end
'@

if (-not $content.Contains($needle)) {
    throw "LeviLamina package recipe has no '$needle' hook."
}

$content = $content.Replace($needle, $injection.TrimEnd())
Set-Content -LiteralPath $recipe.FullName -Value $content -Encoding utf8
Write-Host "Patched LeviLamina package recipe: $($recipe.FullName)"
