# Fork audit issues

Audit date: 2026-10-12 (local time). Reviewed `dev` at `12722c89`; frozen
upstream reference: `master` at `4b9677db`. No application fixes were made during
the initial audit. All eight issues have since been completed.

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

## 2. Completed: choosing Save on close still discards edits when saving fails

- [x] **Fixed and verified on 2026-10-12; committed and pushed to `dev` as `0ef723bc`.**

**Resolution:** Saving now returns success only after the database transaction
has succeeded. Save-before-close and the other `saveIfNeeded` callbacks stop on
failure, retaining the live editor, unsaved status and story lock. Database
transaction-start and commit results are checked. A failed commit remains pending
on its original connection so Retry can preserve the updates already staged there;
after it succeeds, a new transaction saves any additional edits. Retry leaves the
story open until the writer requests closing or switching again. Cancel keeps
the story open; explicit Don't save still permits discarding edits.

GitHub replacement downloads now require the coordinator to confirm that the
opened target was saved and closed. A failed save aborts replacement before
acquiring its replacement lock, publishing a backup or writing downloaded bytes,
and reports that the live story was kept open.

**Validation:** `./build/dev.sh doctor`, `make qmake_all` and
`JOBS=4 ./build/dev.sh build debug` passed. The new
`./tools/check-save-failure.sh` passed 18 ordinary-exit cases on private Xvfb:
document-write rejection and deferred-constraint commit rejection; persistent
and recovered Retry, dismissing errors, Cancel and Don't save; the Open-menu
route; refused GitHub replacement and successful download with a byte-exact
backup; and a fresh-process reopen of every result. Failed saves retain edits,
Undo/Redo and the original committed screenplay. Reopens compare exact script,
emphasis/review XML and all document identities. A separate focused Retry/reopen
run also passed after typing additional edits while a commit was pending.

All seven existing checks passed on the final code: startup (35 ordinary-exit
cases), screenplay scope, independent Fountain/FDX interchange, project sync,
injected credentials, backup retention/restoration and writing-session storage.
The new save checks and startup checks used native XCB windows; the other suites
retain their documented headless/injected boundaries. Tests used disposable
profiles, an absent session bus and fake credentials against a loopback mock API.
The native runner isolated both networking and the X11 socket directory.

**Fix evidence:** [final build](.build/evidence/issue-2-build-2.log),
[native runner and related checks](.build/evidence/issue-2-native-9w5hrzct/),
[18 save/reopen cases and screenshots](.build/evidence/save-failure-aCTUS6/),
[edits after failed commit](.build/evidence/save-failure-4JT33s/),
[backup check](.build/evidence/issue-2-backups.log), and
[writing-session check](.build/evidence/issue-2-writing-session.log).
The new test reproduced the original loss before the fix in
[the retained baseline run](.build/evidence/save-failure-g5s3Mo/).
Earlier unsuccessful harness runs remain retained: missing test includes
(`save-failure-brQ2EV`), sandbox socket refusal (`save-failure-xAN35H`), an edited
running shell script's final-step error (`issue-2-native-qgu1yzww`), and a process
stalled in XOpenDisplay before its first assertion (`issue-2-native-rk4_es90`).
The latter was terminated after retaining
[its backtrace](.build/evidence/issue-2-runner-startup-stall.log); it is not passing
shutdown evidence. Initial screenshots included hidden-editor/isolated-keyring
and animation states; final fixtures wait for the editor to be visible.

This establishes Linux debug behavior with injected Qt actions and private Xvfb
windows. Full-disk/crash faults, every SQLite failure mode, physical desktop/GPU,
release packages and real GitHub/network/keyring integration were unexercised.

The original finding and reproduction below are retained for reference.

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

## 3. Completed: an OS file-open event switches stories without protecting unsaved edits

- [x] **Fixed and verified on 2026-10-12.**

**Resolution:** Public coordinator opening requests, including the application's
`QFileOpenEvent` route, now use the existing save guard. Save opens the requested
story only after saving succeeds. Cancel or a failed save keeps the current
editor, unsaved status, Undo/Redo and story lock. Explicit Don't save permits
switching. Clean requests still open immediately; same-story requests preserve
the live edits without a question. Repeated requests cannot bypass an outstanding
save question, including after a manual Save, or replace its callback.

Already-guarded story-list and cloud callbacks invoke the internal opening method
after approval, avoiding a second guard while their dialog disappears. Startup
file-event buffering and onboarding behavior are retained. The public boolean
result reports only an immediate successful opening; a save question can defer it.

**Validation:** `./build/dev.sh doctor`, `make qmake_all` and
`JOBS=4 ./build/dev.sh build debug` passed. The expanded
`./tools/check-save-failure.sh` passed all 36 ordinary-exit cases on private Xvfb:
the existing 18 save/reopen cases and 18 new file-event/reopen cases covering Save,
Cancel, Don't save, clean and same-story requests, repeated requests, an external
request during the Open-menu question, rejected document writes and rejected
commits with failed and recovered Retry. Every fresh-process reopen compares
exact screenplay/emphasis/review XML and document identities, including the
event targets. An independent read-only SQLite postcheck confirmed all 27 source
and target story comparisons and released story locks.

The 35-case startup/coordinator check also passed on private Xvfb. Screenplay
scope and independent Fountain/FDX interchange checks passed with their existing
offscreen boundaries. Bash syntax, ShellCheck and `git diff --check` passed.

**Fix evidence:** [build](.build/evidence/issue-3-build.log),
[qmake regeneration](.build/evidence/issue-3-qmake.log),
[native runner and related checks](.build/evidence/issue-3-native-0uu3g2ex/),
[36 native cases and screenshots](.build/evidence/save-failure-QRSkld/),
[independent postcheck](.build/evidence/save-failure-QRSkld/independent-postcheck.json),
[35 startup cases](.build/evidence/startup-last-story-MyLGhF/), and
[18 focused offscreen cases](.build/evidence/save-failure-jxhz6R/).
The new test reproduced the original bypass before the fix in
[the retained baseline](.build/evidence/save-failure-9xvnr2/).
[The initial post-fix harness run](.build/evidence/save-failure-fjniJH/) retained
the current story but checked dialog visibility before its opening animation
completed; the final harness waits for the observable save question. Screenshots
include animation transitions.

This establishes Linux debug behavior with actual `QFileOpenEvent` objects and
injected editor/dialog actions on private XCB windows. Physical desktop file
association delivery, other operating systems, release packages and exhaustive
storage/crash faults were unexercised. Existing sync controls used fake credentials,
an absent session bus and a loopback mock API in an isolated network.

The original finding and reproduction below are retained for reference.

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

## 4. Completed: saving a directly opened DOCX overwrites it with Fountain text

- [x] **Fixed and verified on 2026-10-12.**

**Resolution:** Directly opened import-only sources now choose a new `.starc`
destination before editing. This applies to DOCX, PDF, Markdown, RTF, ODT, Celtx,
Trelby and KIT Scenarist. The source is imported into the native copy and remains
byte-exact; its changes are saved in that copy. The misleading Keep .DOCX route
is no longer offered. Canceling or refusing an existing/locked destination keeps
the current story and its lock. Competing file-open events cannot switch stories
while the import chooser is active. Startup requests still wait for onboarding.

Ordinary source saves now accept only Fountain/TXT/FDX/FDXT, case insensitive.
The convenience export route refuses every other suffix before writing, and the
coordinator refuses unsupported shadow saves without clearing unsaved state.
Explicit DOCX/PDF exports remain available through the normal export dialog.
This preserves the source separately; it does not promise lossless import of all
Word or other source-format features.

**Validation:** `./build/dev.sh doctor`, `make qmake_all` and
`JOBS=4 ./build/dev.sh build debug` passed. The new
`./tools/check-import-only-sources.sh` passed all 12 scenarios on private Xvfb:
startup arguments, actual file-open events, fresh onboarding, Cancel, existing and
locked destinations, a read-only DOCX, an omitted destination suffix, competing
events, uppercase DOCX, PDF and Markdown. Including 11 profile-preparation exits
and 12 fresh-process reopens, all 35 processes exited ordinarily with status 0.
The copies support Unicode edits, Undo/Redo, emphasis and review annotations;
reopening compares exact screenplay XML and every document identity.

Independent Python checks confirmed all source hashes, valid DOCX ZIP archives
and their source-only XML parts, exact native script/identity oracles and released
locks. Export controls preserved existing bytes for 11 unsupported suffix cases,
including no suffix, and retained correct Fountain/TXT/FDX/FDXT output selection.
The 36-case save-failure/file-open and 35-case startup suites also passed on private
Xvfb. Screenplay scope and independent Fountain/FDX interchange checks passed
with their documented offscreen boundaries, including explicit DOCX/PDF exports.
Bash syntax, ShellCheck, Python syntax and `git diff --check` passed.

**Fix evidence:** [doctor](.build/evidence/issue-4-doctor.log),
[final build](.build/evidence/issue-4-build-2.log),
[qmake regeneration](.build/evidence/issue-4-qmake-2.log),
[native runner and related checks](.build/evidence/issue-4-native-de6k6ncv/),
[35 import lifecycle cases, source hashes and screenshots](.build/evidence/import-only-sources-p5StuC/),
[36 save cases](.build/evidence/save-failure-MkJ9R1/), and
[35 startup cases](.build/evidence/startup-last-story-XkOhk7/).
The new test reproduced the original overwrite before the fix in
[the retained baseline](.build/evidence/import-only-sources-Y9E295/).

Earlier unsuccessful harness runs are retained. Initial setup attempts had a
[missing include](.build/evidence/issue-4-baseline.log),
[unprepared configured-profile geometry](.build/evidence/issue-4-baseline-2.log),
[the same visibility setup failure](.build/evidence/issue-4-baseline-3.log), and
[a file event still buffered during startup](.build/evidence/issue-4-baseline-4.log).
The first fixed-code assertion expected literal rather than
[Fountain-escaped underscores](.build/evidence/import-only-sources-prwdao/).
[The unhandled close question](.build/evidence/import-only-sources-obPClu/) and
[its dirty-state diagnostic](.build/evidence/import-only-sources-CIsSB9/) were
traced to the first supported export initializing the existing empty title-page
document. The retained [model-change backtrace](.build/evidence/issue-4-export-diagnosis-ykfu3sc7/gdb.log)
establishes that cause; the final harness answers Save on ordinary close while
keeping the exact screenplay/identity reopen assertions. A
[sandbox-blocked debugger attempt](.build/evidence/issue-4-export-diagnosis-z3uuszvh/)
is retained separately. The
[initial existing-destination test](.build/evidence/import-only-sources-xw8VLq/)
dismissed Qt's confirmation without selecting its Yes button; the corrected
driver clicks that button. Finally,
[the first native run](.build/evidence/issue-4-native-i3qwjr91/) reset an already
completed profile to mimic first launch and hit the pending-versus-persisted
settings comparison. The final onboarding case uses genuinely fresh settings.
These failed harness runs are not passing shutdown evidence.

This establishes Linux debug behavior with injected coordinator arguments,
`QFileOpenEvent` objects and Qt chooser/editor/dialog actions on private XCB
windows. Screenshots include animation transitions. Physical desktop file
association delivery, other operating systems, release packages, exhaustive
storage/concurrency faults and third-party Word application compatibility were
unexercised. The sync controls used fake credentials, an absent session bus and
a loopback mock API inside an isolated network.

The original finding and reproduction below are retained for reference.

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

## 5. Completed: renaming a location can permanently lose or misplace annotations

- [x] **Fixed and verified on 2026-10-12.**

**Resolution:** Location renaming now uses the existing annotation-aware
`TextModelTextItem::replaceText` boundary. Emphasis, review comments/status and
resource links on the location name cover its replacement; annotations before
the name stay in place, and annotations on later text follow that text. The
existing model change transaction keeps each screenplay rename in one Undo step.

**Validation:** `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`
passed. `STARC_RENAME_CHECK=model ./tools/check-screenplay-scope.sh` passed
shorter, longer and equal-length names with all three annotation types before,
on, after, across and inside the name, including complete comment payloads,
completion status and resource UUIDs. The full `./tools/check-screenplay-scope.sh`
passed. Its real location-page checks rename HOUSE to HO and then LIVING ROOM,
compare the exact script XML through one Undo/Redo, and verify annotations and
the location identity after project save/reopen. Existing keyboard, pagination,
export, character-rename and project-preservation checks also passed.
`./tools/check-screenplay-interchange.sh` passed independent Fountain/FDX fixtures,
syntax/styles and serialization/save/reopen checks. `git diff --check` passed.

An independent read-only SQLite/XML postcheck confirms that the pre-fix page
fixture lost DAY's bold formatting, review comment and resource links, while
the fixed shorter and longer headings retain their correct ranges and payloads.

**Fix evidence:** [doctor](.build/evidence/issue-5-doctor.log),
[build](.build/evidence/issue-5-build.log),
[model checks](.build/evidence/screenplay-scope-cxXv0m/),
[full scope and page/save/reopen checks](.build/evidence/screenplay-scope-nMC2jA/),
[interchange](.build/evidence/screenplay-interchange-jM5y5h/), and
[independent postcheck](.build/evidence/issue-5-independent-postcheck.json).
The [pre-fix page failure](.build/evidence/screenplay-scope-nx1pJO/) is retained.
Earlier harness attempts remain retained: the model fixture initially passed the
wrong formatting input type ([compile failure](.build/evidence/screenplay-scope-tsANdH/))
and compared noncanonical font metadata ([initial baseline](.build/evidence/screenplay-scope-qh0Cqp/)
and [first fixed-code run](.build/evidence/screenplay-scope-YL6OYK/)). The corrected
fixture verifies its initial ranges and compares the stored canonical metadata.
An [intermediate full-scope run](.build/evidence/screenplay-scope-lEXcaw/) preserved
the annotations but failed an added nested-location assertion against the already
documented issue #7. The final annotation fixture is scoped to issue #5; issues
6–8 remain open.

This establishes Linux debug behavior with injected Qt editor/page actions and
in-process offscreen save/reopen. Visible desktop/GPU behavior, other operating
systems and release packages were unexercised. Existing compiler warnings in
unchanged code remain. Disposable projects and isolated profiles were used.

The original finding and reproduction below are retained for reference.

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

## 6. Completed: location renaming can overwrite the INT/EXT prefix

- [x] **Fixed and verified on 2026-10-13.**

**Resolution:** Location renaming now identifies the original location-name span
after the heading parser's first `. ` separator. It compares the same uppercase,
simplified name as the existing parser, while keeping replacement positions in
the original text. INT/EXT names cannot replace the prefix. Surrounding whitespace,
prefix spelling and time suffixes remain intact, including names with repeated
whitespace or Unicode uppercasing that changes length. The replacement retains
issue #5's annotation-aware boundary and the existing screenplay Undo transaction.

**Validation:** `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`
passed. The new model regression reproduced `INT. INT - DAY` prefix corruption
before the fix. `STARC_RENAME_CHECK=model ./tools/check-screenplay-scope.sh` passed
all 13 heading fixtures: INT/EXT names, a combined prefix, original surrounding
whitespace, mixed case, names with dots, repeated spaces/tabs, repeated location
and time names, literal punctuation, accented names and expanding Unicode case
conversion. The existing shorter/longer/equal-length annotation checks passed.

The full `./tools/check-screenplay-scope.sh` passed. Its real location page now
renames the annotated HOUSE heading to INT, then HO, then LIVING ROOM; each step
checks the heading, annotation payloads and location identity, exact script XML
through one Undo/Redo, and preservation after project save/reopen. Existing
keyboard, pagination, export and project-preservation checks passed too.
`./tools/check-screenplay-interchange.sh` passed its independent Fountain/FDX
fixtures and persisted round trips. An independent read-only SQLite/XML postcheck
confirmed the final saved INT prefix, location/time and annotation ranges/payloads.
`git diff --check` passed.

**Fix evidence:** [doctor](.build/evidence/issue-6-doctor.log),
[build](.build/evidence/issue-6-build.log),
[pre-fix model failure](.build/evidence/screenplay-scope-kTXQdp/),
[model fixtures](.build/evidence/screenplay-scope-M2Njur/),
[full scope and page/save/reopen checks](.build/evidence/screenplay-scope-mtfCQj/),
[interchange](.build/evidence/screenplay-interchange-fjP4og/), and
[independent postcheck](.build/evidence/issue-6-independent-postcheck.json).

This establishes Linux debug behavior with injected Qt editor/page actions and
in-process offscreen save/reopen. Visible desktop/GPU behavior, other operating
systems and release packages were unexercised. Existing compiler warnings in
unchanged code remain. All profiles and projects were disposable. Issues 7–8
remain open.

The original finding and reproduction below are retained for reference.

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

## 7. Completed: shortening a parent location can leave nested locations unchanged

- [x] **Fixed and verified on 2026-10-13.**

**Resolution:** The parent-rename callback now snapshots descendants' original
names and proposed names before applying changes. It skips the renamed parent
by identity and applies a descendant's change only while that model still has
its original name. Recursive child callbacks can rename deeper descendants
without the outer callback renaming them twice. A name beginning with the new
parent's letters no longer causes an untouched descendant to be skipped.
The existing separator rules and destination-collision guard remain in force;
taken destinations keep both location documents and their notes.

**Validation:** `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`
passed. The new page regression reproduced HOUSE → HO leaving HOUSE - KITCHEN
unchanged before the fix. The full `./tools/check-screenplay-scope.sh` passed:
HOUSE → HO and back, unrelated HOUSEBOAT retention, the existing collision
checks, and a five-location subtree using both dash and dot separators.
The subtree checks exercise HOUSE → HO, HO → HO - MAIN, HO - MAIN → HO and
HO → HO - FINAL, including recursive grandchildren and a taken garden name.
They compare every location UUID, document count and exact stored location
content with only the expected name change. All four tested subtree renames
restored exact screenplay XML with one Undo/Redo and retained deep-heading
emphasis, completed review comments and resource links through save/reopen.

Existing keyboard, pagination, export, character/location rename and project
preservation checks passed. `./tools/check-screenplay-interchange.sh` passed
independent Fountain/FDX fixtures and persisted round trips. An independent
read-only SQLite/XML postcheck confirmed the baseline's stale child and the
fixed saved nested names, collision records/notes, deep-heading annotations and
unrelated location. `git diff --check` passed.

**Fix evidence:** [doctor](.build/evidence/issue-7-doctor.log),
[build](.build/evidence/issue-7-build.log),
[pre-fix page failure](.build/evidence/screenplay-scope-Xz9LwZ/),
[full scope, nested pages and save/reopen](.build/evidence/screenplay-scope-vreVm7/),
[interchange](.build/evidence/screenplay-interchange-x4HQbS/), and
[independent postcheck](.build/evidence/issue-7-independent-postcheck.json).

This establishes Linux debug behavior with injected Qt editor/page actions and
in-process offscreen save/reopen. Visible desktop/GPU behavior, other operating
systems and release packages were unexercised. Existing compiler warnings in
unchanged code remain. All profiles and projects were disposable. Issue 8
remains open.

The original finding and reproduction below are retained for reference.

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

## 8. Completed: successful GitHub transfers leave stale sync status and actions

- [x] **Fixed and verified on 2026-10-13.**

**Resolution:** Successful uploads and replacement/new-story downloads now use
one `recordSync` helper to update the persisted link and cached entry together.
The transferred document fingerprint and remote SHA become both the current
snapshot and its comparison baseline, and the entry becomes `InSync`. A
remote-only download also records its new local path. Later saved local edits
therefore become `LocalChanged`, and the context menu offers the matching action.
Separate-copy downloads leave the original baseline intact; failed transfers
cannot record a successful sync.

**Validation:** `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`
passed. The new manager regression reproduced both original failures before the
fix in separate upload/download runs. `./tools/check-project-sync.sh` now runs
the real `GitHubSyncManager` in addition to the existing fingerprint/state/client
checks. It passed unchanged uploads/downloads after repeated own/other-story
status updates, local edits and actual context-action uploads, remote-only
downloads and edit/revert, real conflicts and separate-copy downloads, rejected
uploads, refused replacement downloads, chosen conflict replacement, exact
remote/local/backup bytes, persisted baselines and a fresh manager reload.

`./tools/check-github-credential-storage.sh` passed its injected asynchronous
credential and UI/settings checks. `STARC_SAVE_FAILURE_CASES='sync-failure
sync-success' ./tools/check-save-failure.sh` passed the real application/editor
replacement refusal and success cases, ordinary shutdown and fresh-process
reopening with script/annotation/document-identity preservation. Bash syntax,
ShellCheck, changed-code formatting and `git diff --check` passed.

**Fix evidence:** [doctor](.build/evidence/issue-8-doctor.log),
[build](.build/evidence/issue-8-build.log),
[pre-fix upload failure](.build/evidence/project-sync-Cdhdig/),
[pre-fix download failure](.build/evidence/project-sync-ixvIE3/),
[sync manager and client regression](.build/evidence/project-sync-EJkaaE/),
[injected credential regression](.build/evidence/github-credentials-AmmpPk/), and
[application replacement/save/reopen](.build/evidence/save-failure-NShzm5/).
The initial test-compilation failure and sandbox-blocked mock-server attempt
remain in [project-sync-W1qWHb](.build/evidence/project-sync-W1qWHb/) and
[project-sync-43Th3b](.build/evidence/project-sync-43Th3b/).

This establishes Linux debug behavior with offscreen Qt actions, disposable
SQLite stories/profiles and a loopback mock API using fake credentials. Real
GitHub authentication/network behavior, native keyrings, visible desktop/GPU,
other operating systems and release packages were unexercised. Existing warnings
in unchanged code remain. No user story or real GitHub repository was used.

The original finding and reproduction below are retained for reference.

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
fixes and their additional evidence are recorded above. Issue 2's fix was
committed and pushed to `dev`; its completion record was updated afterward.
