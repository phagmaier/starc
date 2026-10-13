# Fork audit issues

Audit date: 2026-10-12 (local time). Reviewed `dev` at `12722c89`; frozen
upstream reference: `master` at `4b9677db`. No application fixes were made during
the initial audit. Issue 1 has since been completed; issues 2–8 remain open.

Eight defects were confirmed outside the passing regression suite. Existing
upstream defects are included because they still affect this fork. The source
locations below refer to the audited revision. Reproductions used disposable
projects and profiles, never the user's stories.

## 1. Completed: converting to .STARC destroys an existing destination before checking its lock

- [x] **Fixed and verified on 2026-10-12.**

**Resolution:** New-story creation refuses existing files and symlinks, acquires
the destination lock before writing, and stages the new file beside its final
destination. Publication refuses an existing target, including one created
after the initial check. Failed creation/copy/conversion keeps the current story
and its lock, releases the attempted destination's lock, and removes only its own
staging file. The conversion hides its source from the story list only after
success. The creation dialog's permission probe now uses its own temporary file
instead of opening and removing the proposed destination.

**Validation:** Debug build passed. The expanded startup/coordinator check passed
35 ordinary-shutdown cases on private Xvfb, including existing/locked destinations,
a lock with no destination yet, live/dangling symlinks, an unwritable directory,
successful conversion/save/reopen, story copying and failed copying. A test-only
Linux interposer creates another writer's destination immediately before
publication; its exact bytes survive and the source stays open. The same 35
offscreen cases passed their assertions and corrected postcheck. Screenplay scope
and independent Fountain/FDX interchange regressions passed.

**Fix evidence:** [build and regression logs](.build/evidence/issue-1-fix-20261012/),
[successful native runner](.build/evidence/issue-1-native-na3vytx6/check.log),
[native case logs](.build/evidence/startup-last-story-428Qd6/), and
[late-publication collision](.build/evidence/startup-last-story-428Qd6/conversion-race.log).
Earlier unsuccessful harness runs are retained: the close-question responder
initially omitted conversion cases; the runner's postcheck initially mistook the
new compiler log for a case log; and namespace root initially bypassed the
unwritable-folder fixture. The final native run removed that permission bypass.
Native actions are injected through Qt signals/dialog responses; its screenshots
include animation transitions and expected isolated-keyring warnings. This is
Linux debug/private Xvfb evidence, not physical desktop/GPU or release validation.

The original finding and reproduction below are retained for reference.

**Where:** [application_manager.cpp](src/core/management_layer/application_manager.cpp),
lines 1658–1661 (`Switch to .STARC` destination), 1292 (opening it for writing),
and 1322 (checking its lock).

**Reproduce:** Open `existing.fountain` when `existing.fountain.starc` already
contains a saved project. Choose **Switch to .STARC**. The conversion uses that
fixed destination without first protecting the existing file. This also occurs
when another application instance holds the destination's lock.

**Observed:** A valid 53,248-byte destination became zero bytes before the
application rejected its lock. The other instance still owned the lock.

**Why this is a problem:** Conversion can destroy an already saved story, even
one actively protected by another instance. Opening with `WriteOnly` truncates
the destination before the lock check. Inherited from upstream.

**Evidence:** [conversion reproduction](.build/evidence/audit-convert-locked-dialog-cugluu1k/run.log).
The adjacent harness, run script, and destroyed disposable destination are retained.
Real coordinator and dialog path, in-process offscreen Qt; ordinary exit 0.

## 2. High: choosing Save on close still discards edits when saving fails

**Where:** [application_manager.cpp](src/core/management_layer/application_manager.cpp),
lines 1153–1156 (`saveIfNeeded`), 973–1022 (save failure handling), and
2374–2376 (closing through that callback).

**Reproduce:** Edit a `.starc` story, make its database reject document writes,
close the window, and choose **Save** in the normal confirmation.

**Observed:** All three save attempts failed, but the unconditional callback
still closed the story and the application exited 0. Reopening the story lost
the unsaved marker. The reproduction used an SQLite trigger to reject updates;
it did not simulate a full disk or every possible storage failure.

**Why this is a problem:** The writer explicitly requests preservation, but
save failure does not prevent the close operation from clearing the live models.
An error message does not preserve those edits. Inherited from upstream.

**Additional affected route:** GitHub replacement download emits save and close
unconditionally in [github_sync_manager.cpp](src/core/management_layer/content/github_sync/github_sync_manager.cpp),
lines 782–783. The application closes at `application_manager.cpp:2504–2510`
without a save-success response. If the failed save leaves disk bytes unchanged,
the download's fingerprint check can still pass and the replacement can proceed;
its backup contains only previously saved bytes. This sync-specific failure
chain was verified in source, but was not separately fault-injected.

**Evidence:** [failed-save close reproduction](.build/evidence/audit-failed-save-close-vwm10cqu/run.log),
with the isolated harness and write-rejection fixture alongside it.
Real editor/coordinator and ordinary close, offscreen Qt; exit 0 despite lost edits.

## 3. High: an OS file-open event switches stories without protecting unsaved edits

**Where:** [application.cpp](src/app/application.cpp), line 209;
[application_manager.cpp](src/core/management_layer/application_manager.cpp),
lines 2236 and 1409. The event reaches `openProject` directly, which closes the
current story without using `saveIfNeeded`.

**Reproduce:** Leave unsaved text in an open `.starc` story, then deliver an
operating-system file-open event for another story.

**Observed:** The second story opened immediately, no Save/Discard/Cancel
question appeared, and reopening the first story lacked its unsaved marker.

**Why this is a problem:** An external open request silently discards the
current editing session. The issue is in the OS event/public coordinator route;
this reproduction does not establish that the ordinary Open menu behaves the
same way. Inherited from upstream.

**Evidence:** [file-open reproduction](.build/evidence/audit-file-open-onboarding-utjzdwvj/run.log).
Actual `QFileOpenEvent`, application coordinator and editor with injected input,
offscreen Qt; ordinary exit 0. Physical desktop file association delivery was
not exercised.

## 4. High: saving a directly opened DOCX overwrites it with Fountain text

**Where:** [application_manager.cpp](src/core/management_layer/application_manager.cpp),
lines 1028–1031 (saving a local shadow project back to its source);
[export_manager.cpp](src/core/management_layer/content/export/export_manager.cpp),
lines 890–894 (FDX for `.fdx`/`.fdxt`, Fountain for every other suffix).

**Reproduce:** Open a valid DOCX directly through the application's file
argument or OS file-open route, choose **Keep .DOCX**, then save it.

**Observed:** The original 4,141-byte DOCX ZIP archive was replaced by 7,904
bytes of plain Fountain text under the same `.docx` filename.

**Why this is a problem:** Saving destroys the original file format and its
format-specific content. The normal Open chooser advertises `.starc`,
`.fountain` and `.txt`; this issue is reachable through direct file arguments
and file-open events. Importing DOCX into an existing `.starc` is a separate
workflow. The generic fallback also applies to other directly accepted source
formats, but DOCX is the format deliberately reproduced here. The fallback is
inherited from upstream; the fork's FDX exception does not cover DOCX.

**Evidence:** [DOCX save reproduction](.build/evidence/audit-docx-save-9mfu6800/run.log).
The intact original and overwritten output are retained in its `stories/`
directory. Real import/editor/save path, offscreen Qt; ordinary exit 0.

## 5. High: renaming a location can permanently lose or misplace annotations

**Where:** [screenplay_text_model.cpp](src/corelib/business_layer/model/screenplay/text/screenplay_text_model.cpp),
lines 630–635, especially `setText` at 634;
[text_model_text_item.cpp](src/corelib/business_layer/model/text/text_model_text_item.cpp),
lines 881–895. This changes the heading without moving its formatting, review,
or resource-link ranges. The page reaches it through
`location_information_manager.cpp:169` and `project_manager.cpp:3159–3163`.

**Reproduce:** Make `DAY` bold and attach a review comment to it in
`INT. HOUSE - DAY`. Rename location `HOUSE` to `HO` on its location page,
then save and reopen.

**Observed:** The heading correctly became `INT. HO - DAY`, but both the bold
formatting and review comment disappeared, including after reopening. A
separate model probe growing `ROOM` to `LIVING ROOM` left formatting, review,
and resource ranges attached to `ROO` instead of `DAY`.

**Why this is a problem:** An ordinary name change can silently delete review
work or attach it to unrelated text. Character renaming has annotation handling,
but location renaming still uses the unsafe replacement path. Inherited from
upstream and exposed through the fork's restored location page.

**Evidence:** [page/save/reopen reproduction](.build/evidence/audit-location-rename/annotation-page-probe2.log)
and [growing-name model probe](.build/evidence/audit-location-rename/probe.log).
Actual page under offscreen Qt and a separate core-library model probe; both
successful runs exited 0. An earlier diagnostic harness aborted on its own
unchecked indexing after the comment disappeared; its retained core is not an
application-crash finding.

## 6. Medium: location renaming can overwrite the INT/EXT prefix

**Where:** [screenplay_text_model.cpp](src/corelib/business_layer/model/screenplay/text/screenplay_text_model.cpp),
lines 631–633. The replacement searches the entire heading and uses its first
matching occurrence rather than the parsed location span.

**Reproduce:** Enter `INT. INT - DAY`, then rename location `INT` to `ROOM`.

**Observed:** The result was `ROOM. INT - DAY`, rather than
`INT. ROOM - DAY`. It survived saving and reopening. The separate model probe
still parsed the location as `INT` after the malformed replacement.

**Why this is a problem:** A permitted location name can corrupt the scene
prefix and leave the script inconsistent with the renamed location record.
Inherited from upstream.

**Evidence:** [actual-page/save/reopen reproduction](.build/evidence/audit-location-rename/page-probe.log)
and [model probe](.build/evidence/audit-location-rename/probe.log); offscreen,
disposable projects, successful exit 0.

## 7. Medium: shortening a parent location can leave nested locations unchanged

**Where:** [project_manager.cpp](src/core/management_layer/content/project/project_manager.cpp),
lines 3201–3202. The nested-location loop skips names beginning with the new
parent name, including old names that happen to share that prefix.

**Reproduce:** Create `HOUSE` and `HOUSE - KITCHEN`, then rename `HOUSE` to `HO`.

**Observed:** The parent became `HO`, but its nested location and corresponding
script heading stayed `HOUSE - KITCHEN`, including after save/reopen.

**Why this is a problem:** The location hierarchy and script no longer agree
about the parent's name. `HOUSE - KITCHEN` already starts with `HO`, so the
guard incorrectly treats it as updated. The upstream condition remains in the
fork's revised nested-location logic.

**Evidence:** [actual-page/save/reopen reproduction](.build/evidence/audit-location-rename/page-probe.log);
offscreen Qt, disposable project, exit 0.

## 8. Medium: successful GitHub transfers leave stale sync status and actions

**Where:** [github_sync_manager.cpp](src/core/management_layer/content/github_sync/github_sync_manager.cpp),
lines 717–720 (upload), 834–839 (download), and 1377–1385 (`updateProjectState`).
Transfers update the persisted links and badge but leave `entries[].snapshot`
unchanged; later calculations prefer that old snapshot.

**Reproduce:** Upload a changed story successfully, then update its status.
Alternatively, download a newer story successfully, make only a local edit,
save it, and update its status.

**Observed:** The unchanged uploaded story changed from `InSync` to
`LocalChanged`. The downloaded story with a local-only change became `Diverged`
instead of `LocalChanged`.

**Why this is a problem:** The UI reports changes or conflicts that do not
exist and offers context actions for the wrong state. Selecting an action can
refresh the snapshot and then reject that action as outdated. This is in the
fork's added GitHub synchronization code.

**Evidence:** [sync status reproduction](.build/evidence/audit-sync-status-20261012/run.log),
with `status_repro.cpp` alongside it. Actual `GitHubSyncManager` upload/download
against a loopback mock API with fake credentials; exit 0. No real GitHub
repository was contacted.

## Validation and limits

The debug build and all seven existing focused check scripts passed:

- `JOBS=4 ./build/dev.sh build debug`
- `./tools/check-screenplay-scope.sh`
- `./tools/check-screenplay-interchange.sh`
- `./tools/check-backup-retention.sh`
- `./tools/check-github-credential-storage.sh`
- `./tools/check-project-sync.sh`
- `./tools/check-writing-session-storage.sh`
- `./tools/check-startup-last-story.sh`

`./build/dev.sh doctor` passed required dependency checks. Qt 6.12.0 and the
prepared local QtKeychain dependency were used. Optional `lrelease` is absent.
The isolated native keyring and unavailable-provider checks also exited 0.
The passing suite covers keyboard editing, Undo/Redo, pagination, export/import,
backups, startup and retained legacy content; it did not cover the eight
reproductions above.

The production application was also inspected on a private Xvfb display:
story list, screenplay creation choices, project information, typed screenplay,
Statistics, menu, settings in dark/light themes, and character/location pages.
No additional visual defect was established in those inspected screens. Native
keyboard input created scene/action/character/dialogue paragraphs; Save, ordinary
quit, reopening and a second ordinary quit preserved the exact 449-byte script
document. All three production-application Ctrl+Q exits were 0. This is a sampled
UI check at a 1200 × 740 window, not every view, scaling level or screen size.

Build/check logs and native UI screenshots are retained under
[the audit evidence directory](.build/evidence/final-audit-20261012/).
The initial mock-server and Xvfb attempts were blocked by sandbox socket
restrictions and are retained separately. Successful runs used local socket
access, disposable settings/private keyrings, and a network-disabled private
Xvfb session for the application UI.

This audit is Linux debug evidence, including offscreen integration checks and
private Xvfb window inspection. It is not a claim that the whole application is
bug free. Release packaging, other operating systems, the owner's physical
desktop/GPU, real GitHub authentication/network behavior, and exhaustive
storage/crash/concurrency faults were not exercised. No fixes, commits or pushes
were performed during the initial audit; `master` was only read. The subsequent
issue 1 fix and its additional evidence are recorded above; no commit or push
was made for that fix.
