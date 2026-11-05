# Battle.net Steam Overlay Launcher

Launch Battle.net games through Steam with overlay support.

## Usage

1. Extract the launcher and add `Battlenet-Launcher.exe` to Steam as a non-Steam game.
2. Set the shortcut's launch options to a game code below, such as `wowf`.
3. Close the game and Battle.net, then launch the Steam shortcut.

The launcher closes Battle.net when the game exits if it started the client.
An already-running client is left open, but the Steam overlay may not work.
Use `--keep-client` to leave Battle.net open after playing.

## Game codes

| Game | Launch code |
| --- | --- |
| Blizzard Arcade Collection | rtro |
| Call of Duty HQ | codhq |
| Call of Duty: Black Ops 4 | codbo4 |
| Call of Duty: Black Ops Cold War | codbocw |
| Call of Duty: Modern Warfare | codmw2019 |
| Call of Duty: Modern Warfare 2 Campaign Remastered | codmw2cr |
| Call of Duty: MWII / WZ2.0 | codmw2 |
| Call of Duty: Vanguard | codvg |
| Crash Bandicoot 4: It's About Time | cb4 |
| Diablo | d1 |
| Diablo 2: Resurrected | d2r |
| Diablo 3 | d3 |
| Diablo 3 Public Test Realm | d3ptr |
| Diablo 4 | d4 |
| Diablo Immortal | di |
| Hearthstone | hs |
| Heroes of the Storm | hots |
| Heroes of the Storm Public Test Realm | hotsptr |
| Overwatch | ow |
| Overwatch Public Test Realm | owptr |
| Starcraft 2 | sc2 |
| Starcraft Remastered | scr |
| Warcraft 1: Orcs & Humans | w1 |
| Warcraft 1: Remastered | w1r |
| Warcraft 2: Battle.net Edition | w2 |
| Warcraft 2: Remastered | w2r |
| Warcraft 3: Reforged | w3 |
| World of Warcraft | wow |
| World of Warcraft Forever | wowf |
| World of Warcraft Classic | wowclassic |
| World of Warcraft Public Test Realm | wowptr |

## Options

```text
Battlenet-Launcher <game> [Battle.net.exe path] [options]

--timeout SECONDS  Time allowed for the game to start (default: 180; maximum: 3600)
--exe NAME_OR_PATH Game executable to monitor
--product CODE     Battle.net product code to launch
--keep-client      Leave Battle.net open after the game exits
--dry-run          Show settings without launching anything
--help / --list    Show usage and game codes
```

Battle.net is detected automatically. If it isn't found, specify its path:

```text
"G:\Steam\Battlenet-Launcher.exe" ow "D:\Battle.net\Battle.net.exe"
```

## Troubleshooting

- If login or updates take longer than three minutes, add `--timeout 600` to the shortcut's launch options.
- For multiple installations of the same game, use `--exe` with the full path to the intended game executable.
- Only one launcher session can run at a time. Close an already-running game before launching it through Steam.
- Legacy WoW Classic Era / Season of Discovery is not currently supported.

Based on [dafzor's bnetlauncher](https://github.com/dafzor/bnetlauncher) and the rumbleverse-launcher / fortnite-launcher framework.
