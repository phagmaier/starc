# Issue fix workflow

Single agent, directly on `dev`. `master` is frozen. Queue is `/WORKLIST.md`.
History in `docs/archive/*` is read-only.

## Start (every task)

1. `git status --short --branch`. Confirm branch is `dev`. Do not stash, reset,
   or touch unrelated changes.
2. Open `WORKLIST.md`, take the first unchecked item top to bottom. Read its
   archive detail link. Do not skip ahead.
3. Read the locations it names before editing. Trace ownership; keep the fix minimal.

## Fix

1. Reproduce with disposable projects/profiles, fake credentials, loopback mocks only.
2. Fix the named path. Match existing code and `src/.clang-format`; format only changed code.
3. No rewrites outside the issue. Retain source data and recovery paths
   (keyboard, Undo/Redo, pagination, save/reopen, interchange).

## Verify (finish-fixed requires all that apply)

```bash
./build/dev.sh doctor
JOBS=4 ./build/dev.sh build debug
# plus the item's Oracle, e.g.:
./tools/check-screenplay-interchange.sh
./tools/check-first-edit-undo.sh
git diff --check
```

Record exact commands, outcomes, evidence dirs, and anything not exercised
(offscreen vs Xvfb, no physical desktop/GPU, debug only unless stated).

## Finish-fixed

1. In `WORKLIST.md`: check the box, append ≤5 lines under the item:
   what changed, files, oracle + evidence dir.
2. Commit on `dev` and push to `dev`. Never commit/push `master`.
3. Reply with: item, change summary, checks run + result, commit hash, Git state.

## Finish-blocked

1. Leave the box unchecked. Do not mark partial work done.
2. Reply with: item, blocker, what was tried, logs/evidence, what is needed.
3. Only for genuinely new defects: append one short `Untriaged` entry in
   `WORKLIST.md` using its template (trigger, observed bytes, location if known,
   oracle). Never log new issues into `docs/archive/*`.

## Shortcut

Project skill `fix-next` (`.agents/skills/fix-next/SKILL.md`) runs exactly
this file against the first unchecked `WORKLIST.md` item. Invoke via
`/skills` or `$fix-next` in Codex, `/fix-next` in Claude
(`.claude/commands/fix-next.md`). Codex custom prompts under `~/.codex/prompts/`
are global-only and deprecated, so no repo `prompts` file is used.
