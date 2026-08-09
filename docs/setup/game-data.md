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

Keep the data outside Git and pass it explicitly when launching:

```bash
"${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor" \
  "$(pwd)/WowUnreal.uproject" \
  -wowdata="/absolute/path/to/Data"
```

The executable from the original WoW installation is not used by WowUnreal.
