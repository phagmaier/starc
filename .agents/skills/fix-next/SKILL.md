---
name: fix-next
description: Work the first unchecked WORKLIST.md issue top to bottom (reproduce, fix, verify, commit/push dev or report blocked). Use when asked to fix the next issue.
---

# fix-next

Work the first unchecked item in `WORKLIST.md` top to bottom, following
`AGENTS.md` (issue work protocol) and `docs/WORKFLOW.md` (start/fix/verify/finish).

Rules: branch must be `dev`, `master` is frozen read-only. One item only.
If checks pass: check the box with a ≤5-line resolution, commit and push to `dev`.
If blocked: leave unchecked, report blocker + attempts, add an `Untriaged`
`WORKLIST.md` entry only for genuinely new defects.
