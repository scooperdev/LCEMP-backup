LCEMP is my Minecraft Legacy Console Edition source fork that enables LAN multiplayer hosting.

notes:
  - This is NOT the full source code.
  - You need to provide the required asset files yourself.
  - Code quality is not perfect. I am still learning C++.
  - If you use this in other LCE-based projects, credit me.

features:
  - Fully working multiplayer
  - Breaking and placing blocks synced
  - Kick system
  - Up to 8 players (modifiable in source)
  - Keyboard and mouse support
  - Gamma fixed
  - Fullscreen support

launch_arguments:
  - name: -name
    usage: "-name <username>"
    description: Sets your in-game username.

  - name: -ip
    usage: "-ip <targetip>"
    description: >
      Manually connect to an IP if LAN advertising does not work
      or if the server cannot be discovered automatically.

  - name: -port
    usage: "-port <targetport>"
    description: >
      Override the default port if it was changed in the source.

example:
  command: "Minecraft.Client.exe -name Steve -ip 192.168.0.25 -port 25565"

required_assets:
  - path: Minecraft.Client/music/
    content: Music files (.binka)

  - path: Minecraft.Client/Common/Media/
    content: UI (.swf), graphics (.png), sounds (.wav), fonts, localization, XUI scenes, .arc archives

  - path: Minecraft.Client/Common/res/
    content: Game textures (terrain, gui, mobs, items, fonts, particles, etc.)

  - path: Minecraft.Client/Common/DummyTexturePack/
    content: Default texture pack resources

  - path: Minecraft.Client/DurangoMedia/
    content: Xbox One platform media

  - path: Minecraft.Client/OrbisMedia/
    content: PS4 platform media

  - path: Minecraft.Client/PS3Media/
    content: PS3 platform media

  - path: Minecraft.Client/PSVitaMedia/
    content: PS Vita platform media

  - path: Minecraft.Client/Windows64Media/
    content: Windows 64 platform media

  - path: Minecraft.Client/redist64/
    content: Miles Sound System redistributables

  - path: Minecraft.Client/PS3_GAME/
    content: PS3 game package files

  - path: Minecraft.Client/PS4_GAME/
    content: PS4 game package files

  - path: Minecraft.Client/sce_sys/
    content: PS Vita system files

  - path: Minecraft.Client/TROPDIR/
    content: Trophy data

  - path: "**/4JLibs/"
    content: 4J Studios proprietary libraries

  - path: "**/Miles/"
    content: Miles Sound System middleware

  - path: "**/Iggy/"
    content: Iggy / Scaleform UI middleware

  - path: "**/Sentient/"
    content: Sentient middleware

  - path: Minecraft.Client/PS3/PS3Extras/boost_*/
    content: Boost C++ 1.53.0

  - path: Minecraft.Client/PS3/PS3Extras/DirectX/
    content: DirectX math headers

  - path: Minecraft.Client/PS3/PS3Extras/HeapInspector/
    content: Heap inspector static libraries

  - path: Minecraft.Client/Common/Network/Sony/
    content: Sony remote storage libraries

install:
  - Get required assets.
  - Replace your Minecraft.Client and Minecraft.World source folder with this one.
  - Build.
  - Run with optional launch arguments if needed.

contributing:
  - If you find issues, open a PR.
  - I will review and merge if valid.

author: notpies
