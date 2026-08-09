# Runtime configuration

## Known hardcoded assumptions

These are cleanup targets in the current source, not setup instructions:

| Area | Current assumption | Intended replacement |
|---|---|---|
| Root and `Scripts/` launchers | Creator-specific absolute macOS project, UE 5.7, and WoW data paths | One Linux launcher driven by `UE_ROOT`, the repository location, and `-wowdata="${WOW_DATA}"` |
| [`AWowWorldManager`](../../Source/WowWorld/Private/WowWorldManager.cpp) | Falls back to the creator's WoW `Data/` directory | Require an explicit value or a documented project/user setting |
| [`WowParserTests`](../../Source/WowTests/Private/WowParserTests.cpp) | Probes common home-directory locations and the creator's path | Read one optional test-data setting and skip data-dependent tests clearly when absent |
| [`FMpqManager`](../../Source/WowData/Private/Mpq/MpqManager.cpp) and cinematic lookup | Archive and cinematic paths explicitly use `enUS` | Either keep `enUS` as an explicit supported constraint or add locale discovery/configuration |
| Lua API stubs | Several locale-returning functions report `enUS` | Source the locale from the same runtime configuration as the MPQ loader |
| [`FWowSavedVariables`](../../Source/WowUI/Private/WowSavedVariables.cpp) | Uses a fixed test-account string as a fallback namespace | Use the authenticated account or an explicit anonymous/local namespace |

The default authentication endpoint `127.0.0.1:3724` and conventional world port `8085` are development defaults rather than creator-specific paths, but they must remain overridable.

## WoW data

The workstation's shell configuration defines `WOW_DATA` as the absolute path
to the supported WoW `Data/` directory. Pass its value explicitly with
`-wowdata="${WOW_DATA}"`. The shell performs this expansion; the application
does not currently discover `WOW_DATA` directly.

`Config/DefaultEngine.ini` currently exposes `WowDataPath`, but `AWowWorldManager` does not consume that setting. When the command-line argument is absent, the source falls back to a creator-specific macOS path. Removing that fallback and defining a single configuration precedence order is pending cleanup work.

## Server and credentials

The login screen accepts an authentication endpoint in `host:port` form. Port 3724 is the default.

Saved credentials live in `Saved/WowCredentials.json`, which is ignored by Git. The parser expects an array, not the object shown by the original README. For temporary local autologin, it accepts this legacy plaintext form:

```json
[
  {
    "alias": "local",
    "username": "YOUR_ACCOUNT",
    "server": "127.0.0.1",
    "port": 3724,
    "default": true,
    "password": "YOUR_PASSWORD"
  }
]
```

The credential store may rewrite passwords as XOR-plus-Base64 obfuscation. This is not encryption. Protect the file as plaintext-equivalent data; OS credential-store integration remains future work.

Use `-autologin` to select the default credential or `-autologin -account=local` to select an alias. Autologin selects the first returned realm and first character.
