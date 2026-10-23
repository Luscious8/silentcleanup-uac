# SilentCleanup UAC Bypass (Method 34)

A standalone, native C implementation of UACME's Method 34 (DiskCleanup Environment Variable). This tool hijacks the user's `%windir%` environment variable and triggers a built-in Windows scheduled task to execute an arbitrary payload at **High integrity without a UAC consent prompt**.

No DLLs, no service installation, and no admin rights required to launch. A single, lightweight native executable.

## How It Works

The bypass relies on the combination of three legitimate Windows behaviors:

1. **Writable User Environment:** `HKCU\Environment` is writable by any user without elevation.
2. **Auto-Elevating Scheduled Task:** Windows ships `\Microsoft\Windows\DiskCleanup\SilentCleanup` with `RunLevel = HighestAvailable`. It elevates silently because scheduled tasks bypass consent prompts.
3. **Environment Variable Resolution:** The task's action is `%windir%\system32\cleanmgr.exe`. Because it runs in the user's context, `%windir%` is resolved from the user environment — which we control.

### Execution Chain
1. Set `HKCU\Environment\WINDIR` to the payload directory.
2. Broadcast `WM_SETTINGCHANGE` to propagate the update.
3. Trigger the `SilentCleanup` task via COM (`ITaskService`).
4. The task scheduler runs it with `HighestAvailable`, granting silent elevation.
5. The task resolves `%windir%` to the hijacked path and executes `system32\cleanmgr.exe`.
6. The `WINDIR` environment variable is restored.

> **Note:** Because `%windir%` is a prefix, the payload must be placed in a directory structured exactly like: `<payload_dir>\system32\cleanmgr.exe`.

<img width="800" height="450" alt="2026-10-23 14-01-41" src="https://github.com/user-attachments/assets/d08e3067-df4a-462b-b2f8-7c98b6dfdc98" />

scious.
