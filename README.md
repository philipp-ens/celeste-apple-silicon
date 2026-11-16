# Celeste native Apple Silicon converter

Soooooo, since macOS will kill Rosetta with macOS 28, I thought I try myself on a solution and made a script to convert the game to a native app.

This will run with a shell script, so If you are not sure about it you can check it out yourself.

## Requirements

- An Apple Silicon Mac with macOS 11 (Big Sur) or newer
- Celeste 1.4.0.0 installed through Steam, unmodded
- An internet connection during conversion (about 1 MB is downloaded, see below)

## Usage

1. Quit Celeste.
2. Open Terminal in this folder and run:

   ```bash
   ./convert.sh
   ```

   The script finds Celeste in your Steam library. To point it elsewhere:

   ```bash
   ./convert.sh --game-dir "/path/to/steamapps/common/Celeste"
   ```

3. Start Celeste from Steam as usual.

If macOS refuses to run the script because it was downloaded, run it as
`bash convert.sh`.

### After a Steam update or "Verify integrity of game files"

Steam puts the Intel build back. Run `./convert.sh` again.

### Going back

```bash
./convert.sh --undo
```

restores the Intel app saved during conversion. Steam's "Verify integrity of
game files" does the same.

## Changes

Inside your Celeste folder, `Celeste.app` is replaced by a converted copy. The
game itself (`Celeste.exe`) and all its content are left as they are; only the
Intel-only parts around it are replaced:

| Original (Intel) | Replacement (arm64) |
|---|---|
| Launcher with embedded Mono runtime | New launcher + Mono 6.14.1 runtime and class libraries |
| FNA 21.x with SDL2, FNA3D, FAudio | FNA 26.11 with SDL3, FNA3D (Metal), FAudio |
| FMOD Studio 1.10 | FMOD Engine 2.03.15 + a small compatibility shim |
| Steamworks.NET 10 + CSteamworks | A minimal wrapper over the current Steam API library |

A folder `arm64-converter-files` is created next to `Celeste.app`. It holds:

- the original Intel `Celeste.app` (for `--undo`)
- a one-time backup of your save folder
- the Steam library, cached for re-conversion
