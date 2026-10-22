
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

## Requirements
- Windows 10 21H2 (Build 19044) or later.
- MSVC (`cl.exe`) via Developer Command Prompt.
- UAC enabled (below "Always Notify" level).

## Usage

**1. Build the binary:**
```cmd
cl /nologo /W4 /DUNICODE /D_UNICODE method34.c
2. Stage the payload:
Create the directory structure and copy your payload as cleanmgr.exe.

cmd
mkdir C:\Users\Public\p\system32
copy /Y C:\Windows\System32\cmd.exe C:\Users\Public\p\system32\cleanmgr.exe
3. Execute:

cmd
method34.exe C:\Users\Public\p
(If no argument is provided, it defaults to C:\Users\Public\p)

Upstream Bugs Fixed
This repository is not just a port; it fixes four critical defects found in the upstream reference code (UACME/Akagi) that caused intermittent failures and broken execution:

Broken quoteFix on 21H2+:
The upstream code appends a stray trailing quote (C:\Users\Public\p"), causing ERROR_FILE_NOT_FOUND. This version writes the value with no quotes (meaning the payload path must not contain spaces).

Race Condition (Async Trigger):
The upstream code clears WINDIR immediately after calling RunEx, causing the task to frequently resolve the original path. This version introduces a Sleep(3000) to ensure the task captures the hijacked variable.

Payload Validation:
The upstream default expects a file, but %windir% acts as a prefix. This version validates that the provided payload directory actually contains system32\cleanmgr.exe.

Task Termination:
The upstream code calls IRunningTask_Stop() immediately, which kills long-running payloads. This version releases the IRunningTask interface without stopping the process.

Limitations
Standard Users: HighestAvailable means "highest the user can get." If the account is not in the Administrators group, there is no elevated token to reuse. It will prompt for credentials.

No Spaces: The payload path must not contain spaces (due to the unquoted WINDIR fix).

15-Minute Timeout: The SilentCleanup task is hardcoded to terminate after 15 minutes. Long-running payloads will be killed.

Always Notify: If UAC is set to "Always Notify" (ConsentPromptBehaviorAdmin = 2), this bypass fails.

Detection Notes
Uses a legitimate, Microsoft-signed scheduled task.

Native C, no .NET metadata (avoids Wacatac.B!ml heuristics).

Different code path than standard fodhelper/cmstp bypasses, effectively evading specific AV signatures (e.g., McAfee's Trojan:Win/UACBypass.CMSTP).

Credits
Original concept: UACME by hfiref0x (Method 34).

Original standalone port: Akagi.

Bug analysis, hardening, and verification: Luscious.
