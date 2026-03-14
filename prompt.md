# WowUnreal — Ralph Loop Prompt

## Task Status Legend
- `[ ]` — Incomplete (not started or needs rework)
- `[?]` — Waiting verification (work done, needs a different agent to verify)
- `[x]` — Complete (verified by a second agent)

## Context Loading
1. Read `agents.md` — build commands, run/screenshot commands, project rules
2. Read `specs/*` — feature requirements
3. Read `implementation_plan.md` — current progress and task list

## Task Execution — VERIFICATION FIRST

### If you see a `[?]` task:
**Do this FIRST.** Review the code, confirm it exists and meets spec requirements.
- **Build it:** run the build command from `agents.md`
- **Run it:** launch the game and take a screenshot to `Saved/Screenshots/`
- **If VERIFIED:** mark `[x]`, commit
- **If INCOMPLETE:** mark `[ ]` with a note explaining what's missing

### If no `[?]` tasks exist:
1. Pick the first incomplete `[ ]` task (respect phase order, check dependencies)
2. Implement it completely — no placeholders, no TODOs
3. **Build → Run → Screenshot** to confirm it works visually
4. Mark as `[?]` (NOT `[x]` — you cannot verify your own work)
5. Commit immediately

## Build, Run & Screenshot — MANDATORY

Every task completion requires all three:
```bash
# 1. Build
UE="/Users/Shared/Epic Games/UE_5.7"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" WowUnrealEditor Mac Development \
  -project="/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" -waitmutex

# 2. Run + Screenshot (smoke test)
"$UE/Engine/Binaries/Mac/UnrealEditor" \
  "/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" \
  -game -windowed -resx=1920 -resy=1080 -nosplash -log

# 3. Save screenshot to Saved/Screenshots/ with descriptive name
```
If the build or run fails, fix it before moving on.

## Git Commits — MANDATORY
After EVERY successful change:
```bash
git add -A && git commit -m 'Add: description of change (verified: builds, screenshot confirms X)'
```
**DO NOT skip commits.** One logical change per commit.

## Error Handling
- File not found → use glob/list_directory to locate it
- Build fails → read error, fix it; if stuck 3 times → mark `[B]` blocked, move on
- Link errors → check `Build.cs` module dependencies
- UE macro errors → check `GENERATED_BODY()`, include order, UCLASS syntax

## Rules
- Search before implementing — don't duplicate existing code
- Read files before editing
- One task per iteration
- No placeholders or TODOs in committed code
- **NEVER mark your own work `[x]`** — only mark as `[?]`
- **Only mark `[x]` when verifying ANOTHER agent's `[?]` work**
- Do NOT reference `~/projects/WowGodot` — it has known issues

---RALPH_STATUS---
STATUS: IN_PROGRESS | COMPLETE | BLOCKED
TASKS_COMPLETED: <number>
FILES_MODIFIED: <number>
TESTS_PASSED: true | false
EXIT_SIGNAL: true | false
NEXT_STEP: <brief description>
---END_STATUS---
