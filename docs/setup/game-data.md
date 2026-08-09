# WoW 3.3.5a game data

WowUnreal reads original World of Warcraft assets at runtime. Supply a legally obtained, unmodified Wrath of the Lich King 3.3.5a build 12340 `Data/` directory.

The supported data is the original 2010-era WotLK MPQ layout. It is not:

- the original vanilla 1.12 client;
- Blizzard's modern WoW Classic or Wrath Classic CASC data; or
- AzerothCore's generated server-side map, vmap, mmap, or DBC bundle.

The current loader expects archives shaped like:

```text
Data/
├── common.MPQ
├── common-2.MPQ
├── expansion.MPQ
├── lichking.MPQ
├── patch.MPQ
└── enUS/
    ├── locale-enUS.MPQ
    ├── expansion-locale-enUS.MPQ
    └── lichking-locale-enUS.MPQ
```

Additional patch and speech archives are loaded when present. Locale discovery is not implemented: archive names are currently hardcoded for `enUS`.

Keep the data outside Git. The local shell setup exposes its absolute path as
`WOW_DATA`; for example, a zsh user can add this to `~/.zshrc`:

```bash
export WOW_DATA="${HOME}/Downloads/WoW/Data"
```

Verify the configured directory before launching:

```bash
test -f "${WOW_DATA}/common.MPQ"
```

Pass the environment variable's value through the required `-wowdata` argument:

```bash
"${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor" \
  "$(pwd)/WowUnreal.uproject" \
  -wowdata="${WOW_DATA}"
```

The shell expands `WOW_DATA`; WowUnreal itself currently reads the resulting
path from `-wowdata`, not directly from the process environment.

The executable from the original WoW installation is not used by WowUnreal.
