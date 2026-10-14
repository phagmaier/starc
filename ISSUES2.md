# Second fork audit: remaining issues

Reviewed on 2026-10-13 against clean `dev` at
`1580693c4123650a50aa79c5f4d503a81ea51cfe`. Frozen `master` remains
`4b9677dbfb5c1faaa91c55c415fdf12715f8521d` and was only read.
No application fixes, commits or pushes were made during this review.

The build and all nine existing focused regression scripts passed. Additional
fault, concurrency and editor probes nevertheless confirmed the fourteen issues below.
Issue 15 was additionally confirmed while fixing issue 13; it is recorded separately.
Issues 16 and 17 were confirmed while fixing issue 14 and are recorded separately.
Issues 18, 19 and 20 were confirmed while fixing issue 12 and are recorded separately.
The eight completed entries in `ISSUES.md` were not reopened; these findings
identify remaining paths or failure classes. Inherited upstream defects are
included because they still affect writing in this fork.

All experiments used disposable stories, profiles, fake credentials and local
mock repositories. No personal screenplay, real GitHub account or desktop keyring
was used. Locations refer to the reviewed revision.

**Priorities:** P1 means a data-safety, recovery or crash defect to address before
depending on the application as the only working copy of a script. P2 means a
narrower correctness or usability defect. These are review priorities, not an
estimate of how often a fault occurs. Keep an independent copy of important
scripts while the P1 items remain open, particularly because issue 3 affects the
application's own recovery copies.

| Issue | Priority | Problem |
| --- | --- | --- |
| 1 | P1 | Failed Fountain/FDX saves appear successful; export can destroy existing output |
| 2 | P1 | Save As overwrites another instance's locked destination |
| 3 | P1 | Background backups can mix two committed story generations |
| 4 | P1 | An automatic SQLite rollback leaves Save/Retry permanently stuck |
| 5 | P1 | A new GitHub download can overwrite a concurrently created, locked story |
| 6 | P1 | An old lock dialog can crash a different, modified story |
| 7 | P1 | Track Changes loses original text depending on selection direction |
| 8 | P2 | Adding an adjacent comment silently drops the new comment |
| 9 | P2 | Deleting between visible paragraphs also deletes a hidden beat |
| 10 | P2 | Restoring a window can divide by zero in its splitter and abort startup |
| 11 | P1 | Importing Fountain with both emphasis and a review note crashes the debug app |
| 12 | P1 | Fountain export/reimport removes literal brackets and backslashes from script text |
| 13 | P2 | Fountain escaping moves review-note anchors onto the wrong text |
| 14 | P2 | Fountain export loses forced Action types and changes their content |
| 15 | P1 | Undo of the first edit in an imported styled/reviewed screenplay aborts the debug app |
| 16 | P1 | Fountain export leaves out Shot paragraphs |
| 17 | P1 | Fountain export/reimport turns dialogue with a reserved line start into headings and cues |
| 18 | P2 | A review comment on an inline note comes back as part of the note's text |
| 19 | P1 | Fountain import removes asterisks and underscores that are text |
| 20 | P2 | Imported character and location names keep Fountain markup |

## 1. Failed Fountain/FDX saves appear successful; output is not protected

- [x] **P1 — fixed 2026-10-13; verified through exporters and the application coordinator.**

**Locations:**
[application_manager.cpp](src/core/management_layer/application_manager.cpp),
lines 1051–1060 and the final successful return from `saveChanges`;
[export_manager.cpp](src/core/management_layer/content/export/export_manager.cpp),
`canWriteToFile` at 160–166 and the void convenience export route at 886–923;
[abstract_markdown_exporter.cpp](src/corelib/business_layer/export/abstract_markdown_exporter.cpp),
333–342 and 531–535;
[screenplay_fdx_exporter.cpp](src/corelib/business_layer/export/screenplay/screenplay_fdx_exporter.cpp),
310–341.

**Trigger and result:** Open a Fountain/FDX file directly, edit it, then lose write
permission to that source before Save. The temporary `.starc` database commits,
but the source export fails. `exportDocument` returns no success status, and the
coordinator unconditionally clears the window's modified flag. Closing therefore
needs no save question, even after permission is restored. Reopening the source
loses the edit.

Actual Fountain and FDX application probes recorded `after_save_dirty=0`,
`database_error=0`, `source_unchanged=1`, `marker_in_source=0`, and
`marker_in_shadow=1`. Ordinary close exited 0 without a save question, and a fresh
process reopened the source without the marker. This is source-output failure,
distinct from the database failures already fixed in the earlier audit.

There is a second failure mode in the same output boundary: `canWriteToFile`
opens an existing file with `WriteOnly`, truncating it just to test access. Actual
exporters then write directly and ignore failed writes. With a file-size limit of
zero and `SIGXFSZ` ignored, the real export manager returned normally for both
Fountain and FDX while each existing 27-byte output became **zero bytes**. This
models an open-success/write-failure boundary; it does not prove physical-disk
exhaustion or crash durability.

**Debugging direction:** Return an explicit result through every export layer,
including final writes/flush/publication. Stage complete output and replace the
destination only after successful completion; remove the destructive access
probe. Source saving must retain dirty state and live edits when export fails.
Verify permission failure and partial-write failure through ordinary close and
fresh-process reopen, with exact original source bytes as the failure oracle.

**Evidence:**
[exporter log](.build/evidence/audit2-interchange/write-failure.log),
[Fountain save/close log](.build/evidence/audit2-interchange/shadow-fountain2/write-fail.log),
[fresh Fountain reopen log](.build/evidence/audit2-interchange/shadow-fountain2/reopen.log),
[FDX save/close log](.build/evidence/audit2-interchange/shadow-fdx/write-fail.log),
[fresh FDX reopen log](.build/evidence/audit2-interchange/shadow-fdx/reopen.log).
Probe sources are `write_failure.cpp` and `shadow_save_failure.cpp` in
`.build/evidence/audit2-interchange/`. Earlier harness failures are retained there.
[Supplemental interchange findings](.build/evidence/audit2-interchange/findings.md)
include compilation and execution commands for this and issues 11–14.

**Resolution (2026-10-13):** Exporters now return a checked result through the
manager to the Save coordinator. Fountain/Markdown, FDX, DOCX and PDF stage their
output with atomic publication and direct-write fallback disabled. The destructive
access probe is removed. Failed writes, flushes or publication preserve existing
output; failed source saves retain the live editor, modified flag and lock and
show a saving error with Retry. Retry is restricted to the same source/shadow
story. Export dialogs remain open on failure, and clipboard callers consume only
a successful selection export so they cannot reuse preserved stale output.

The new `./tools/check-source-export-failure.sh` passed **88 ordinary process
exits**: eleven cases and fresh-process reopen for each of Fountain, TXT, FDX and
FDXT. It covers read-only output, failed/recovered Retry with later edits,
Save on close, Cancel, explicit Don't save, short staged writes, failure at final
publication, and successful/failed clipboard exports. Independent file-byte
oracles verify retained originals and saved results. Direct exporter checks use
zero- and one-byte kernel file-size limits for all six output suffixes, check
existing/new destinations, then verify successful recovery. A Linux test-only
interposer applies a 64-byte limit during the selected source's final flush, or
removes destination-directory write permission only after staging; SQLite remains
writable. These are bounded write/publication faults, not physical-disk exhaustion
or power-loss durability evidence.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, the screenplay
interchange and scope checks, Bash syntax, ShellCheck and `git diff --check`
passed. The existing save-failure check passed its `close-retry`, `close-dismiss`,
`close-discard`, `commit-retry`, `commit-dismiss`, `commit-discard`,
`file-open-failure` and `file-open-commit` cases with fresh reopen. The scope check
also independently validated DOCX archive/content and seven-page PDF output.
All runtime checks used disposable profiles and an absent desktop session bus,
with offscreen Qt actions. Physical desktop/GPU behavior, release builds and
other operating systems remain unexercised.

Retained passing evidence: [source export regression](.build/evidence/source-export-failure-tgGr0t/),
[interchange](.build/evidence/screenplay-interchange-Z3eQz5/),
[database-save failures](.build/evidence/save-failure-HJOE7G/),
and [screenplay scope](.build/evidence/screenplay-scope-WRgBlk/).
Earlier harness failures remain in the other `source-export-failure-*` evidence
directories: the initial database lookup preceded import serialization, a raw
Fountain marker comparison ignored escaping, a close question was checked before
it appeared, and a 64-byte direct-export limit exceeded one complete minimal
fixture. The final direct-export limit guarantees an actual short write.
The scope run [Jrh1Iw](.build/evidence/screenplay-scope-Jrh1Iw/) started while a build
was replacing shared libraries and failed in the loader before any test output;
its core/backtrace remain, including the plugin build-ID mismatch. The completed
build's scope run passed. At that point, issues 2–14 remained open.

## 2. Save As overwrites a locked destination before checking its lock

- [x] **P1 — fixed 2026-10-13; verified through the real Save As coordinator.**

**Location:**
[application_manager.cpp](src/core/management_layer/application_manager.cpp),
`Implementation::saveAs`, lines 1269–1287; the later lock check is in
`Implementation::openProject`/`tryLockProjectOnOpen`.

**Trigger:** Story A is open. Another live instance holds B's `.starc.lock`.
Choose Save As to B and accept replacement.

**Result:** `saveAs` removes B and copies A over it before trying to open and lock
B. The subsequent opening refuses B's live lock, after the overwrite has already
happened and A's editor/lock has been closed. B's open SQLite connection may now
refer to the unlinked old file. If copying fails after removal, B also has no
protected original at its path.

The disposable probe used two distinct valid SQLite stories and a live
`QLockFile` holder. It drove the real file chooser, with the overwrite question
treated as accepted. Its byte oracle proved B changed while B's lock remained
held; A's lock was gone. The fixture then exited ordinarily with status 0.
The earlier conversion fix protects `createLocalProject`; Save As still uses
this separate destructive route.

**Debugging direction:** Acquire and validate the destination lock before any
destination mutation. Stage a complete copy, publish safely, and retain both
the old destination and the current editor on refusal or failure. Test locked
replacement, failed copy, symlinks and a concurrent destination publisher.

**Evidence:**
[verified log](.build/evidence/audit2-20261013/save-as/verified-run/audit-save-as-lock.log),
the destination byte oracle and both stories under that run's `stories/`, and
`save_as_probe.cpp`/`run_probe.py` in `.build/evidence/audit2-20261013/save-as/`.
The initial probe's incorrect source-open assertion is retained separately;
the verified run checks the source lock, not the database's retained filename.

**Resolution (2026-10-13):** Save As now acquires the destination lock before
replacement confirmation or any destination writes and transfers that same lock
to the copied story without releasing/reacquiring it. It stages a complete copy:
existing destinations use `QSaveFile` with direct-write fallback disabled, while
new destinations use a same-directory `QTemporaryFile` and a rename that refuses
an occupied path. Source reads, writes, flushes and publication are checked.
Symlinks and non-file destinations are refused, and the destination's kind is
checked again after the replacement question. The chooser applies the `.starc`
suffix before confirmation. A refusal or copy/publication failure leaves the
source editor and lock in place and preserves the old destination. Choosing
Don't save before a failed or canceled Save As also retains the modified flag,
live edits and Undo/Redo history. Incoming file-open requests cannot switch the
story while the Save As chooser or replacement question is active.

`./tools/check-save-as-safety.sh` passed **39 ordinary process exits**: a seed,
nineteen cases and a fresh-process reopen of each result. Cases include locked
existing/absent destinations, new and approved replacements, chooser and
replacement cancellation, omitted suffixes, same-story requests, valid/dangling
symlinks, a directory appearing after selection, an unwritable directory,
source-read failure, short staged writes, failed final publication, and a
competing publisher immediately before new-file publication. Dirty failure cases
verify live editor identity, pending text, Undo/Redo and subsequent successful
Save. Independent byte/SQLite oracles check unchanged destinations, exact script
and annotation bytes, document identities, integrity and released locks.

The Linux test-only interposer applies a genuine one-byte kernel file-size limit
only during writes to the staging descriptor, or removes directory write access
only for final publication. It also proves the destination lock remains held
during confirmation/publication and supplies an asynchronous in-memory credential
store. These are bounded write/publication faults with injected Qt actions, not
physical-disk exhaustion, power-loss durability or native-keyring evidence.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, Bash syntax,
ShellCheck, changed-code formatting and `git diff --check` passed. The existing
startup/last-story regression passed **35 ordinary exits**; Fountain/FDX
`permission-retry` and `permission-close` source-export controls and their fresh
reopens passed **8 ordinary exits**. Those controls used the same in-memory
credential-store injection. All projects/profiles were disposable, with offscreen
Qt and an absent desktop session bus. Desktop/GPU behavior, release builds and
other operating systems remain unexercised. Issues 3–14 remain open.

Passing evidence: [Save As safety](.build/evidence/save-as-safety-YBgEk6/),
[startup coordinator](.build/evidence/startup-last-story-Hv47s6/),
[source-export controls](.build/evidence/source-export-failure-RHT4I5/),
and [final build](.build/evidence/save-as-safety-build-final.log).
The new regression failed on the old build in
[Xs6MYi](.build/evidence/save-as-safety-Xs6MYi/): its independent hash proved the
locked destination changed, and the source lock was gone. Earlier harness/build
failures remain in the other `save-as-safety-*` directories/logs. They include
dialog handling, credential-isolation setup, unfired Qt virtual-write hooks,
reopening incorrectly tied to the startup preference, and a Python oracle that
initially distinguished SQLite TEXT from BLOB instead of comparing their exact
UTF-8 document bytes. Final runs assert that each requested fault actually fired.

## 3. Background backups can publish a mixture of two saved generations

- [x] **P1 — fixed 2026-10-13; verified with competing SQLite writers and exact recovery bytes.**

**Locations:**
[backup_builder.cpp](src/corelib/utils/tools/backup_builder.cpp), raw copy at
115–136, publication/pruning at 154–196;
[application_manager.cpp](src/core/management_layer/application_manager.cpp),
asynchronous backup launch at 1084–1107.

**Trigger:** A background backup is reading the live `.starc` SQLite file while a
later Save commits another generation. The per-source backup lock serializes
backups with one another; normal database saves do not take it.

**Result:** The backup can combine pages read before and after the commit, yet
report success and proceed with retention pruning. Its contents need not match
any committed version of the story.

The probe paused a worker immediately after its first 64 KiB source read, made
an ordinary SQLite UPDATE/COMMIT, and resumed the production copy. A 256 KiB
document changed from all A bytes to all B bytes. The published recovery copy
contained **53,671 old A bytes plus 208,473 new B bytes**. Its fingerprint matched
neither committed source. SQLite structural integrity checks passed, showing
that integrity checks alone do not detect this class of logical corruption.
The fixture uses a document-content blob to expose page mixing, rather than
parsed screenplay XML. The live source itself remained correct.

**Debugging direction:** Take a consistent SQLite snapshot or coordinate saves
with the entire source read. Retain the existing safe destination publication and
retention rules. Validate restored document bytes against one complete committed
generation while a second save deliberately overlaps the backup.

**Evidence:**
[backup race log](.build/evidence/audit2-persistence/backup-race.log), source and
published databases under `backup-race-fixture/`, and `backup-race.cpp` in
`.build/evidence/audit2-persistence/`. The pause establishes a concurrency defect,
not its incidence during normal typing.
[Supplemental findings](.build/evidence/audit2-persistence/findings.md) contain
fresh-directory replay commands.

**Resolution (2026-10-13):** SQLite backups now use a private, read-only connection
and hold a read transaction across the entire source-file copy. An actual schema
read acquires SQLite's shared lock before copying, so a later Save cannot change
database pages until the copy is complete. Source reads are unbuffered and restart
at byte zero under the lock, preventing pre-lock buffered bytes from contaminating
the snapshot. Connections and locks are released on success and failure. Plain
source files retain their exact byte-copy route. Existing snapshot names,
non-overwriting publication and retention rules remain in use.

The application uses SQLite rollback journaling. WAL-mode databases are explicitly
refused without publication or pruning: their committed content can reside outside
the main file and a read transaction does not freeze that file. This is a safe
backup refusal, not WAL backup support. No database is converted or rewritten.
See SQLite's [locking contract](https://sqlite.org/lockingv3.html).

`./tools/check-backup-consistency.sh` passed deliberate overlapping saves in
DELETE, TRUNCATE and PERSIST modes. The production worker pauses after its first
64 KiB read; another process cannot acquire an exclusive lock and a same-process
Save cannot commit until copying finishes. Save then succeeds. The first recovery
copy matches the entire old committed database byte for byte; the next copy
matches the new one. Restored documents, annotations, history patches, IDs and
opaque legacy data have exact authored bytes. An independent Python process
reopens all twelve source/recovery files and checks complete generations, schema
data and SQLite integrity. The existing retention check also restores a real
screenplay through the editor and verifies its serialized bytes after save/reopen.

Failure checks cover WAL refusal, an unreadable SQLite source and a genuine
32 KiB kernel file-size limit during native-database staging. Failed backups leave
existing recovery copies and the source unchanged, clean their temporary
files, and release the read lock. A subsequent Save and backup succeed; only that
successful publication permits pruning. The new concurrency regression fails
against the original builder: both writers proceed during copying and the
published file contains mixed A/B generations. The original audit reproduction
was also replayed before the fix and reproduced its reported byte counts.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, the new consistency
check, `./tools/check-backup-retention.sh`, `./tools/check-project-sync.sh`, Bash
syntax, ShellCheck, changed-code formatting and `git diff --check` passed. Sync
used only a disposable localhost mock repository and injected credentials. Its
first run could not create the sandboxed loopback socket; the permitted rerun
passed. Runtime fixtures were disposable; editor checks used offscreen Qt.
Desktop/GPU behavior, release builds, other operating systems and power-loss
durability remain unexercised. WAL backup support is outside this fix.
Issues 4–14 remain open.

Passing evidence: [consistency and failure checks](.build/evidence/backup-consistency-nIHOdx/),
[retention and real screenplay restoration](.build/evidence/backup-retention-qVk66L/),
[shared sync caller](.build/evidence/project-sync-j7e7Ud/),
and [debug build](.build/evidence/backup-consistency-build.log).
[Original-builder regression failure](.build/evidence/backup-consistency-baseline-KfkRmS/)
and [original audit replay](.build/evidence/backup-race-before-hldyKs/) remain.
Earlier harness failures remain in the other `backup-consistency-*` directories:
the first expected a native error code absent from Qt's COMMIT error, the Python
annotation oracle initially included an extra terminal NUL, and the short-write
test first lacked the C++ signal header. Their final checks passed without timing
delays or relaxed generation assertions.

## 4. An automatic SQLite rollback makes Save/Retry permanently fail

- [x] **P1 — fixed 2026-10-13; verified with genuine SQLITE_FULL and the real editor/coordinator.**

**Locations:**
[database.cpp](src/corelib/data_layer/database.cpp), `Database::commit` at 173–197,
especially 190–193;
[application_manager.cpp](src/core/management_layer/application_manager.cpp),
952–960, 983–987 and 1015–1019;
[abstract_mapper.cpp](src/corelib/data_layer/mapper/abstract_mapper.cpp),
75–85, 95–97 and 123–125;
[document_storage.cpp](src/corelib/data_layer/storage/document_storage.cpp),
110–120;
[document_change_storage.cpp](src/corelib/data_layer/storage/document_change_storage.cpp),
126–140.

**Trigger:** An earlier document write succeeds inside Save, but a later write
returns an error that rolls back the entire transaction, such as `SQLITE_FULL`.
The underlying storage problem is then repaired and the writer chooses Retry.

**Result:** `Database::commit` treats every failed COMMIT as a still-active pending
transaction. After SQLite already rolled back, Retry commits a transaction that
does not exist and refuses to reconnect while it considers that commit pending.
Earlier accepted writes have also been marked stored or removed from persistence
queues before the outer commit succeeded.

A page-count cap on only the disposable database produced genuine `SQLITE_FULL`
without filling the physical disk. After lifting the cap, retries still returned
`cannot commit - no transaction is active`; the live document held new bytes but
was marked stored, while disk held its old bytes. Separately, changing the
existing coordinator fixture's trigger from `RAISE(ABORT)` to `RAISE(ROLLBACK)`
made its recovery assertion fail after the trigger was removed. Live edits,
annotations, Undo/Redo and the dirty flag remained, but Save never recovered.

**Debugging direction:** Distinguish a failed active COMMIT from an automatic
rollback. Restore the dirty/insertion/history state of all statements lost by
rollback, preserving the live editor and document identities. Clearing only the
pending flag is insufficient. Cover recovery after capacity returns and additional
edits made while saving is blocked.

**Evidence:**
[SQLITE_FULL log](.build/evidence/audit2-persistence/sqlite-full.log),
[coordinator failure log](.build/evidence/audit2-persistence/rollback-coordinator.log),
`rollback-storage.cpp` and `rollback-coordinator.cpp` in
`.build/evidence/audit2-persistence/`.
The coordinator fixture intentionally exits 1 at the failed recovery assertion;
it is not successful ordinary-close evidence. Supplemental replay commands are
in [the persistence findings](.build/evidence/audit2-persistence/findings.md).

**Resolution (2026-10-13):** Failed saves now explicitly roll back the root
transaction, whether SQLite left it active or already aborted it. The original
statement/COMMIT error is retained. Transaction completion callbacks restore
provisional updates to dirty state, revert uncommitted insert IDs/cache ownership,
and restore new-document, ordered history, image and attachment queues. Deletions
retain their objects until the root commit succeeds and restore them on rollback.
Nested failure vetoes the root commit; subsequent mapper/UUID saves are refused
until it ends, and the project manager stops processing after a storage error.
Clearing an error cannot revive a failed outer transaction. Storage clearing and
connection closure unwind callbacks before freeing objects.

Retry starts a new transaction and resubmits the retained live state, including
later edits. It no longer repeatedly commits a nonexistent transaction.
Reconnection is restricted to SQLite's file-open error, preserving connection
settings such as deferred-constraint enforcement on other failures. Explicit
rollback follows SQLite's [documented error-recovery option](https://sqlite.org/lang_transaction.html).
No native SQLite dependency or schema migration was added.

`./tools/check-transaction-recovery.sh` passed genuine capped-database
`SQLITE_FULL`, late automatic rollback, statement-only abort and deferred COMMIT
failure. Every case accepts earlier updates, new documents, multiple history
inserts, images/attachments and deletions. Two successive failures preserve the
entire committed database byte for byte; capacity/fault recovery saves all pending
writes plus later edits. Existing IDs and live UUIDs survive, and ordered undo/redo
patches are exact. C++ reopen and an independent Python process check authored
content, PNG/attachment bytes, retained deletions and history ordering.
The original capacity reproduction and real-coordinator rollback case both failed
to recover before this fix; their baseline logs remain in
[rollback-fix-baseline](.build/evidence/rollback-fix-baseline/).

`./tools/check-save-failure.sh` passed **42 ordinary process exits**: all 21 cases
and a fresh-process reopen for each, including automatic rollback, failed COMMIT,
repeated/recovered Retry, later edits, Undo/Redo, Save on close, Cancel, discard,
file-open requests and local mock sync. Reopened script/emphasis/review XML and
document UUIDs are exact. Source-export checks with
`STARC_SOURCE_EXPORT_CASES='permission-retry partial-retry publish-retry'` passed
24 ordinary exits across Fountain/TXT/FDX/FDXT, plus the direct exporter checks.

`./build/dev.sh doctor`, `make qmake_all` in `.build/debug`,
`JOBS=4 ./build/dev.sh build debug`, screenplay scope/interchange, backup
consistency, writing-session storage, changed-code formatting, Bash syntax,
ShellCheck and `git diff --check` passed. Runtime fixtures used disposable stories
and profiles, offscreen Qt and an absent desktop session bus; sync used only a
localhost mock repository. Physical desktop/GPU behavior, release builds, other
operating systems, physical-disk exhaustion and power-loss durability remain
unexercised. Issues 5–14 remain open.

Passing evidence: [transaction recovery](.build/evidence/transaction-recovery-VCLH9E/),
[editor/coordinator](.build/evidence/save-failure-ZpNitN/),
[source export](.build/evidence/source-export-failure-YP3clD/),
[screenplay scope](.build/evidence/screenplay-scope-WDZxUT/),
[interchange](.build/evidence/screenplay-interchange-EZDZU9/),
[backup consistency](.build/evidence/backup-consistency-ChgTfi/),
[writing statistics](.build/evidence/writing-session-storage-AcMDd3/),
and [debug build](.build/evidence/rollback-build-final.log).

The image test initially aborted in the unchanged JPEG encoder, with bundled
PDF-library JPEG frames called by Qt's JPEG plugin. Its
[log and backtraces](.build/evidence/transaction-recovery-uXIVtg/) remain; that
separate codec problem is unresolved. Transaction checks exercise the production
PNG byte-input route. Earlier verification failures remain: the first coordinator
run exposed lost connection settings after unconditional reconnect, and the first
Python byte oracle compared SQLite TEXT directly with bytes. The final code
restricts reconnects and the independent reader requests exact BLOB bytes.

## 5. A new GitHub download can overwrite a concurrently created, locked story

- [x] **P1 — fixed 2026-10-13; verified through both new-story and separate-copy downloads.**

**Location:**
[github_sync_manager.cpp](src/core/management_layer/content/github_sync/github_sync_manager.cpp),
793–807 and 852–861. Only the replacement branch acquires the destination lock
at 822–825.

**Trigger:** A remote-only story is downloading to a path classified as new.
After the existence check, another writer acquires its `.lock` and publishes a
local story there. New and separate-copy downloads use replacing `QSaveFile`
publication without acquiring that lock.

**Result:** Download overwrites the competing writer's bytes, reports success,
and makes no pre-replacement backup because the destination was classified as
new. A lock held before the file exists is likewise ignored. A dangling symlink
passes the existence check and causes the download to create its target outside
the selected destination path.

The real manager probe intercepted only `QSaveFile::open` for its disposable
download path. At that boundary it acquired the lock and published
`OTHER_WRITER_SAVED_SCRIPT_BYTES`. Production download replaced those bytes with
the remote SQLite story while the competing lock remained owned. Separate controls
confirmed an absent-file live lock and dangling symlink. The separate-copy branch
shares this source path but was not independently raced.

**Debugging direction:** Lock every destination, reject existing paths and
symlinks, stage output, and publish new stories without replacing a concurrent
writer. Reuse the guarded new-project workflow. Refusal must preserve the remote
entry and sync baseline.

**Evidence:**
[collision log](.build/evidence/audit2-persistence/sync-new-destination-race-runner.log),
[lock/symlink controls](.build/evidence/audit2-persistence/sync-new-destination-runner.log),
`sync-new-destination.cpp`, `run-sync-race.sh` and `run-sync-destination.sh` in
`.build/evidence/audit2-persistence/`. Runners use fresh disposable directories,
fake credentials and loopback only; they require local socket access.

**Resolution (2026-10-13):** All download destinations now acquire the story lock,
including absent paths and separate copies. The path is inspected under that lock;
existing new-file destinations, valid/dangling symlinks and non-file destinations
are refused. New stories and copies follow the guarded new-project pattern:
stage in a same-directory `QTemporaryFile`, check the complete write and flush,
then publish with a rename that refuses an occupied destination. A file, symlink
or directory appearing immediately before publication survives unchanged.
Replacement downloads retain their fingerprint and pre-replacement backup checks
and use `QSaveFile` with direct-write fallback explicitly disabled. Locks remain
held through publication and project registration. Failed downloads do not record
a sync baseline or register a local project; separate copies retain the original
story's bytes and conflict baseline.

`./tools/check-github-download-safety.sh` passed **26 ordinary harness process
exits**, thirteen cases for each route, with a separate Python process checking
every result. Cases cover successful publication, live locks on absent/existing
paths, a story appearing after the repository check, valid/dangling symlinks,
directories, competing file/symlink/directory publication, an unwritable staging
folder, genuine short staged writes and failed final publication. Every refusal
is followed by successful recovery after removing the collision or fault.
The test checks lock ownership through publication, exact persisted baselines,
cached conflict/sync status, no phantom projects, temporary-file cleanup and
released owned locks. Independent read-only SQLite/byte oracles verify entire
downloaded and competing files, document/review IDs and bytes, opaque legacy
content, change-history patches, integrity and untouched symlink targets.

The Linux test-only interposer freezes wall time for deterministic copy names,
observes the real Qt publication boundary, inserts competing paths, limits only
the staging descriptor to one byte with `RLIMIT_FSIZE`, or removes directory
write permission only during publication. The requested faults must actually
fire. These are bounded concurrency/write/publication checks, not physical-disk
exhaustion or power-loss durability evidence. The same regression fails against
the retained pre-fix plugin for **both** routes: a locked, absent destination is
published and registered despite refusal being required.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, the existing
`./tools/check-project-sync.sh`, changed-code formatting, Bash syntax, ShellCheck
and `git diff --check` passed. The sync control covers uploads, replacement
downloads with exact previous-version backups, successful baseline advancement,
separate-copy conflicts and failed transfers. All fixtures/profiles were
disposable, with a localhost mock API, injected fake credentials, offscreen Qt
dialog actions and an absent desktop session bus. Editor/visible-desktop/GPU
behavior, native keyrings, real GitHub accounts, release builds and other
operating systems remain unexercised. Issues 6–14 remain open.

Final evidence: [download safety](.build/evidence/github-download-safety-89Miue/),
[sync controls](.build/evidence/project-sync-LAdqzT/),
and [debug build](.build/evidence/github-download-build-formatted.log).
Expected pre-fix failures remain in [new-story control](.build/evidence/github-download-safety-Wpy5T2/)
and [separate-copy control](.build/evidence/github-download-safety-49DyvE/).
The initial passing matrix remains in [30ggR8](.build/evidence/github-download-safety-30ggR8/).
Earlier runner attempts remain in `github-download-baseline*.log`: direct
execution lacked an executable bit, the sandbox denied localhost sockets, and
the original success-expecting audit harness timed out after loading the fixed
plugin. That audit timeout is not the pre-fix regression evidence above.

## 6. An old lock dialog can crash a different, modified story

- [x] **P1 — fixed 2026-10-13; verified with competing file-open events, late dialog responses and fresh reopening.**

**Location:**
[application_manager.cpp](src/core/management_layer/application_manager.cpp),
`tryLockProjectOnOpen` callback at 1535–1541; `closeCurrentProject` at 1801–1804;
the public guarded opening route at 2273–2299 is bypassed by this callback.

**Trigger:** Opening locked story B shows the “Ignore and open” question. Before
answering, an OS file-open request opens unlocked story C. Edit C, then answer the
still-actionable question about B.

**Result:** The callback uses the shared `lockFile`, which now belongs to C,
instead of the original attempt. It resets C's lock and calls the private opening
route directly. Closing C then aborts at `!lockFile.isNull()`; without assertions
the following null dereference also remains. C's unsaved edits were not offered
a save question before this crash path.

The probe confirmed C's real editor held unsaved edits, the old dialog remained
visible, and answering it aborted at `application_manager.cpp:1801`. The warning
immediately before the abort was `removeStaleLockFile can only be called when not
holding the lock`, corroborating that the callback acted on C's acquired lock.

**Debugging direction:** Bind lock questions to the original attempt/lock and
invalidate stale questions when the current project changes. Route accepted
reopening through the unsaved-edit guard. Do not reset a newer story's lock.
Cover competing OS requests and edits before answering either dialog button.

**Evidence:**
[confirmed abort log](.build/evidence/audit2-20261013/stale-lock/verified-run3/audit-stale-lock.log),
`stale_lock_probe.cpp` and `run_probe.py` under
`.build/evidence/audit2-20261013/stale-lock/`. Earlier harness attempts that failed
before reaching the lock callback remain in `verified-run/` and `verified-run2/`.
The confirmed run aborted; it is not ordinary-close evidence.

**Resolution (2026-10-13):** Lock questions now own the failed opening attempt's
lock separately from the current story's acquired lock. Closing or switching
stories invalidates, disconnects and disables the old question before its hide
animation, even when no story is currently open. A dialog-identity check also
rejects a response delivered after invalidation. A current **Ignore and open**
response operates only on its own attempted lock and uses public `openProject`,
which applies the unsaved-edit guard and reacquires the destination lock.

The regression first reproduced the original assertion with real
`QFileOpenEvent` objects and unsaved text in the real screenplay editor. Eight
new cases now cover both stale buttons after a competing unlocked request and
edits, a close/save question, both stale buttons after a newer locked request,
current acceptance/rejection, and refusal to remove a live lock. Invalidated
dialog widgets are retained in the test so late signals are deliberately
delivered. Cases verify dirty state, exact lock bytes, byte-exact unopened
databases, Undo/Redo, Save, ordinary shutdown and released locks. Fresh processes
compare exact screenplay/emphasis/review XML and every document identity.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, Bash syntax,
ShellCheck and `git diff --check` passed. The eight lock cases with
`STARC_SAVE_FAILURE_CASES` passed **16 ordinary process exits** offscreen.
The complete `./tools/check-save-failure.sh` passed **58 ordinary process exits**
on a private Xvfb/XCB display with
`STARC_SAVE_FAILURE_PLATFORM=xcb STARC_SAVE_FAILURE_SCREENSHOTS=1`, including
29 fresh reopen checks and the existing save, rollback, Retry, close, GitHub
replacement and external-request cases. Native editor screenshots were inspected.
All profiles and stories were disposable; lock holders were same-process
`QLockFile` objects, the GitHub server was loopback-only, and the desktop session
bus was absent. Actions used Qt signals/dialog responses; physical desktop/GPU,
production-executable keyboard interaction, release builds and other operating
systems remain unexercised.

Passing evidence: [offscreen lock cases](.build/evidence/save-failure-2Xh5sw/),
[complete native suite](.build/evidence/save-failure-MpswYS/), and
[build/native runner logs](.build/evidence/issue6-lock-dialog/).
The new [pre-fix abort](.build/evidence/save-failure-YZdGRS/) is retained.
The [initial fixed-code run](.build/evidence/save-failure-KYu33G/) exposed a
fixture assumption: successful opening runs the existing database-version
upgrade, including SQLite BLOB-to-TEXT storage changes and version records.
Successful opens therefore check exact screenplay bytes and identities;
unopened targets still require whole-file equality. The sandbox's refused Xvfb
socket launch is retained separately from the successful native run.
At that point, issues 7–14 remained open.

## 7. Track Changes physically deletes original text depending on selection direction

- [x] **P1 — fixed 2026-10-13; verified through the screenplay editor, coordinator saves and fresh-process reopening.**

**Location:**
[screenplay_text_edit.cpp](src/core/management_layer/plugins/screenplay_text/text/screenplay_text_edit.cpp),
798–831.

**Original trigger:** Original action text is followed by an action paragraph whose text
is marked as an addition. With Track Changes enabled, select both paragraphs
from their beginning toward the end and press Delete.

**Original result:** The entire selection is classified as added text and physically
deleted, including the original text that should remain as deletion marks.
Reversing the same selection preserves the text as tracked deletions.
The addition scan starts at `cursor.block()`, the selection's active endpoint,
so forward selection omits earlier original paragraphs.

**Resolution:** The addition scan now starts at
`document()->findBlock(selectionInterval.from)` and inspects the selected source
range in either direction. A selection containing original text remains in the
script with removal marks; selections containing only additions still delete
normally. Existing fragment boundaries, deletion marking and replacement-typing
behavior are retained.

The committed regression is
[check-track-changes.sh](tools/check-track-changes.sh), backed by
[track_changes_test.cpp](tests/track_changes_test.cpp). Its 54 cases cover both
directions, Delete/Backspace/replacement typing, original/added/mixed paragraphs,
an original middle paragraph, partial paragraphs and format fragments, an end
at the next block's start, and added-only controls. It checks exact character
ranges and emphasis, byte-exact one-step Undo/Redo, coordinator saves, retained
document identities and exact revision XML after fresh-process database/editor
reopening. Independent XML comparisons require equivalent results in both
directions, accounting for separately generated review dates and default-scene
UUIDs; within each case, Undo/Redo and reopening compare exact bytes.

**Validation:** `./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`,
`./tools/check-track-changes.sh` (54 cases and fresh-process reopening), and
`./tools/check-screenplay-scope.sh` passed. The original six-case reproduction
also passed with `STARC_TRACK_CHANGES_PLATFORM=xcb` on an isolated private Xvfb
display, including fresh-process reopening and independent XML comparison.
Bash syntax, ShellCheck, changed-code formatting and `git diff --check` passed.

The pre-fix loss is retained in
[track-changes-byHcvo](.build/evidence/track-changes-byHcvo/), with the final full
run in [track-changes-Rc4N9E](.build/evidence/track-changes-Rc4N9E/) and native run
in [track-changes-N2APYG](.build/evidence/track-changes-N2APYG/). Build, scope and
debugger logs are under [issue7-fix](.build/evidence/issue7-fix/).
Earlier fixture failures remain: an incorrect font alias, a read-only hosted
editor, lowercase input capitalized at a paragraph start, destroyed fixture
editors whose addresses were cached by handlers, and cross-fixture comparison
of newly generated default-scene UUIDs. None was substituted for the retained
pre-fix data-loss reproduction.

**Limits:** The editable production editor is explicitly bound to the
coordinator's loaded model, with injected Qt key events and disposable profiles;
the toolbar interaction itself was not exercised. Offscreen and private Xvfb
checks do not establish physical desktop/GPU, other-platform or release behavior.

**Original audit evidence:**
[editor probe log](.build/evidence/audit2-editor/run2.log),
`tracked-delete-reverse-0.xml` and `tracked-delete-reverse-1.xml` under
`.build/evidence/audit2-editor/`. The first snapshot loses original text; the
reverse-direction snapshot retains it with `rem="true"`. These are serialized
editor/model snapshots, not full application database-reopen evidence.
Issues 8–14 remain open.

## 8. Adding an adjacent comment silently discards the new comment

- [x] **P2 — fixed 2026-10-13; verified through the screenplay editor, live comments panel, coordinator saves and fresh-process reopening.**

**Location:**
[text_document.cpp](src/corelib/business_layer/document/text/text_document.cpp),
`addReviewMark` at 2053–2078; UI route in
[screenplay_text_view.cpp](src/core/management_layer/plugins/screenplay_text/screenplay_text_view.cpp),
1300–1303, through `ScreenplayTextEdit::addReviewMark`.

**Original trigger:** In `Alpha Beta Gamma`, comment on `Alpha `, then add another
comment on `Beta`, using the same author and color.

**Original result:** Adjacent ranges are considered overlapping. The first mark
expands from `[0,6)` to `[0,10)`, but its only comment remains “first comment”;
“second comment” is never stored. The merge branch expands the old range and
returns without consuming the incoming `_comment`. A comment added on text the
same author had highlighted in the same color was dropped the same way.

**Resolution:** `TextDocument::addReviewMark` stitches only marks without text
(highlights, revision and Track Changes marks); that path is unchanged. A comment
with text is always stored:

- A selection that only touches a comment or highlight becomes its own comment on
  exactly the selected text. The neighbouring mark keeps its range, messages,
  dates and resolved state.
- A character holds one review thread, so a selection that shares text with
  existing comment threads continues them: one thread covers the union of their
  text and the selection, with every earlier message unchanged in text order and
  the new comment last. It keeps the earliest thread's color and is reopened if
  it was resolved. Parts of those threads outside the selection (other emphasis
  fragments or paragraphs) are included.
- A highlight without text gives way to the comment on the selected characters
  only; the rest of the highlight remains.

The comments panel model listed a comment in the story's first paragraph once per
emphasis fragment after any edit, and compared fragments instead of whole threads
when a paragraph changed. It now groups fragments the way its loader does, so the
live panel and a reopened one show the same rows.

An earlier uncommitted attempt joined touching comments into one thread and sent
highlights through the same code. A probe showed that a same-color highlight over
a comment then deleted the comment, and that a comment on highlighted text was
filed under an empty first message, so it was replaced. Its patch and probe logs
are kept in [issue8-review](.build/evidence/issue8-review/).

**Regression:** [review_comments_test.cpp](tests/review_comments_test.cpp) and
[check-review-comments.sh](tools/check-review-comments.sh) drive the production
screenplay editor, a live comments-panel model and the project coordinator with
disposable stories. The 56 cases (28 in both selection directions) cover touching
selections before and after a comment, beside a resolved thread with a reply,
between two threads, across a paragraph boundary and beside a highlight, with
author and color differences; overlapping, contained, containing, identical and
bridging selections; a repeated message; threads and selections spanning two
paragraphs; comments inside and over highlights; and highlight, revision,
addition and removal controls, including a highlight over a comment. Each case
checks every panel row's text, messages, color and resolved state, exact metadata
of earlier messages, the editor's mark on every character, emphasis, byte-exact
one-step Undo/Redo, a shifted anchor, the coordinator save, and exact XML and
panel rows after fresh-process reopening. An independent script compares the
saved XML anchors with the expected ones and both selection directions.

**Validation:**

- `./build/dev.sh doctor` and `JOBS=6 ./build/dev.sh build debug`: passed.
- `./tools/check-review-comments.sh`: 56 cases, fresh-process reopening and the
  independent XML check passed offscreen
  (`.build/evidence/review-comments-t9GQlt/`) and on a private Xvfb/XCB display
  with editor screenshots (`.build/evidence/review-comments-SmI7A1/`).
- Against the committed pre-fix sources 44 of the 56 cases fail; the 12 highlight
  and revision controls pass before and after, so those paths behave as they did
  (`prefix-soft-summary.txt`).
- Removing each part of the fix in turn fails the suite at a different case: the
  stitch guard (`adjacent`), the thread join (`overlap`), both panel changes
  together (`adjacent`), the fragment grouping (`comment-in-highlight`) and the
  first-paragraph guard removal (`new-comment-control`). Logs are in
  `mutations/`; the sources were restored, rebuilt and the full suite rerun.
- `./tools/check-track-changes.sh` (54 cases), `./tools/check-hidden-beats.sh`
  (48 cases), `./tools/check-screenplay-scope.sh` and
  `./tools/check-screenplay-interchange.sh`: passed.
- Bash syntax, ShellCheck, changed-code formatting and `git diff --check`: passed.

Build and suite logs are under [issue8-review](.build/evidence/issue8-review/).

**Limits:** Comments are added with injected editor/model calls on disposable
profiles; the toolbar and the add-comment form were not operated. Offscreen and
private Xvfb runs do not establish physical desktop/GPU, other-platform or
release behavior.

Three related behaviors were present before this fix, are unchanged, and were
confirmed on the fixed build (`limit-*` under `issue8-review`):

- A highlight in a different color applied over a comment replaces the comment
  on the highlighted text; one Undo restores it.
- A comment applied over a Track Changes mark replaces that mark on the shared
  text.
- The panel identifies a thread by author, creation second and colors. Two
  adjacent notes by one author with the same timestamp are shown as one row with
  only the first note's text, although both are stored.

**Original audit evidence:** [editor log](.build/evidence/audit2-editor/run2.log),
`adjacent-comments.xml` and the independent `postcheck.txt` under
`.build/evidence/audit2-editor/`.

## 9. Deleting between visible paragraphs destroys a hidden beat heading

- [x] **P2 — fixed 2026-10-13; verified through the screenplay editor, exact Undo/Redo and fresh-process reopening.**

**Location:**
[standard_key_handler.cpp](src/core/management_layer/plugins/screenplay_text/text/handlers/standard_key_handler.cpp),
341–346 and 431; deletion through
[text_cursor.cpp](src/corelib/business_layer/document/text/text_cursor.cpp).
The selection-removal source retains a TODO about hidden beats.

**Trigger:** Two visible action paragraphs have a beat heading between them.
With beats hidden, select from the end of the first action to the beginning of
the next and press Delete, intending to join the visible text.

**Result:** The actions merge, but the invisible beat title is also removed from
the model and serialized XML. Beat content can therefore disappear without being
displayed in the selected text. Beats are hidden by default in the screenplay
document.

**Debugging direction:** Preserve hidden outline content during visible-text
editing or make destructive outline changes explicit. Cover selection deletion,
boundary Delete/Backspace and replacement typing, including Undo/Redo and reopen.

**Evidence:** [editor log](.build/evidence/audit2-editor/run2.log) verifies that
the heading was invisible before deletion. `before-hidden-delete.xml` contains
`HIDDEN BEAT TITLE`; `after-hidden-delete.xml` does not. Artifacts and the reusable
`editor_probe.cpp`/`run.sh` are under `.build/evidence/audit2-editor/`.

**Resolution:** `TextCursor::removeCharacters` splits deletion into visible
sections around hidden beat headings, preserving the heading text, formatting,
review comments, beat identities and both paragraph boundaries. Boundary
Delete/Backspace keeps the caret in place, including beside empty actions.
Selection Delete/Backspace and replacement typing remove only the selected
visible text; the remaining paragraphs stay in their original beats. Show beat
headings to explicitly delete or merge across them. Ordinary joins within one
beat and deletion across visible headings retain their existing behavior.

**Regression:** [hidden_beats_test.cpp](tests/hidden_beats_test.cpp) and
[check-hidden-beats.sh](tools/check-hidden-beats.sh) exercise the production
screenplay editor and project coordinator with disposable stories. The 48 cases
cover both selection directions, single/multiple/adjacent hidden headings,
select-all, empty actions, boundary keys, replacement typing and visible-heading
and ordinary-join controls. They check exact XML through one-step Undo/Redo,
coordinator save and fresh-process reopening. Independent XML checks compare
beat identities, heading emphasis/review comments and selection directions.

**Validation:**

- `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`: passed.
- `./tools/check-hidden-beats.sh`: 48 offscreen editor cases and fresh-process
  reopening passed. The initial XML postcheck incorrectly expected reverse
  fixtures for single-key cases; the runner now distinguishes those cases.
- The final 48-case suite passed under private Xvfb/XCB, including caret checks
  and the corrected independent XML postcheck. To verify this fix independently
  of pre-existing issue-8 worktree edits, a separate `libcorelib.so` used the
  committed `TextDocument`/comments implementations and this issue's cursor fix.
  The retained driver was
  `STARC_HIDDEN_BEATS_PLATFORM=xcb xvfb-run -a -e .build/evidence/issue9-build/xvfb.log -s '-screen 0 1280x900x24' bash .build/evidence/issue9-build/native-run.sh`.
  Final evidence: `.build/evidence/hidden-beats-FV5KkI/`; build and independent
  library logs: `.build/evidence/issue9-build/`.
- `./tools/check-track-changes.sh`: all 54 cases, fresh-process reopening and
  independent selection-direction XML comparison passed in
  `.build/evidence/track-changes-1ADz9E/`.
- Bash syntax, ShellCheck, changed-code formatting and `git diff --check`: passed.
- The original failing deletion control remains in
  `.build/evidence/hidden-beats-xSc5vG/`; the earlier offscreen run/postcheck failure
  and sandbox Xvfb abort remain in `.build/evidence/hidden-beats-slXUJu/` and
  `.build/evidence/hidden-beats-obnVGJ/`. The sandbox abort occurred before cases ran.
  Xvfb used programmatic Qt key events; physical desktop keyboard interaction,
  release builds and user-driven window close remain unexercised.

## 10. Window restoration can abort when the splitter has zero total size

- [x] **P2 — fixed 2026-10-13; verified through the production application on private Xvfb/XCB displays of three sizes, with ordinary exits.**

**Locations:**
[splitter.cpp](src/corelib/ui/widgets/splitter/splitter.cpp), `setSizes` at
305–337 and 363; child Show handling at 558–563; state validation at 460–482;
[application_view.cpp](src/core/ui/application_view.cpp), `restoreState` at
197–207.

**Original trigger and result:** During window restoration a child Show event can
call `setSizes` with a zero total size. The method divides both the available
extent and each requested size by that total, then converts non-finite results to
integer widget geometry. Startup aborts before the story opens.

The production debug executable reproduced this with an ordinary saved profile
copied from the offscreen startup fixture onto private XCB. GDB showed
`sizesMax=0` and `sizeDelta=NaN` in `Splitter::setSizes`. The stack reaches
`QRect::setWidth` with `-2147483648`, then Qt's checked-integer overflow assertion.

**Cause:** Qt does not restore a saved window geometry when the saved screen width
and the current one differ by more than about a fifth (a ratio below 0.8 or above
1.25). The application view is then shown before it has any size, so its splitter
is zero pixels wide. At that width the splitter's rule that hides a panel narrower
than 24 pixels set the navigator's proportion to zero, the navigator's Show event
asked for the sizes `{0, 0}`, and dividing by their total produced the NaN
geometry. Any saved layout opened on a sufficiently wider or narrower screen
(another monitor, resolution or scale) therefore aborted the debug build.

This is now reproduced with a native session only. Using the pre-fix build, a
layout was saved by an ordinary first-run setup and Ctrl+Q on a 1500x950 display.
Starting with a story on the same display worked. On 800x600 and 1920x1080
displays the same profile aborted with the same assertion before a window
appeared. The audit profile behaves the same way: saved on an 800 pixel wide
screen, it aborts on a 1280 pixel one and starts on an 800 pixel one.

**Resolution:** `Splitter::setSizes` ignores sizes whose total is not positive
and keeps the current proportions, and it no longer applies the narrow-panel
rule while the splitter has no width. `Splitter::restoreState` ignores saved
proportions that are both zero or not finite. A window whose geometry Qt refuses
now opens at the existing 1200x740 fallback size with its saved panel
proportions; unusable saved proportions leave the default 30/70 split. Ordinary
resizing, dragging and the narrow-panel rule at a real width are unchanged.

**Regression:** [window_restore_test.cpp](tests/window_restore_test.cpp) and
[check-window-restore.sh](tools/check-window-restore.sh) restore layouts through
the production `ApplicationView` and exercise the production `Splitter`. Seven
window cases use the audit profile's saved bytes as if saved on a screen 2.5
times wider or narrower, with usable, zero, non-finite and split proportions,
plus same-screen and first-run controls. Five splitter cases cover showing it
without a width, sizes with no total at zero and real widths, a collapsed panel
shown without a width, saved-state validation and ordinary resizing. They check
the window size, exact proportions, finite panel geometry inside the splitter and
the state saved next. Each case runs in its own process.

**Validation:**

- `./build/dev.sh doctor` and `JOBS=6 ./build/dev.sh build debug`: passed.
- `./tools/check-window-restore.sh`: 12 cases passed offscreen
  (`.build/evidence/window-restore-foQevD/`) and with XCB on private 1280x900,
  1920x1080 and 800x600 Xvfb displays.
- Against the committed pre-fix splitter 9 of the 12 cases abort with the audit's
  assertion and the 3 controls pass. Removing one guard at a time fails 2, 4 and
  3 cases respectively (`mutations/`); the source was restored and rebuilt.
- Production application with the fix, each started with a story and closed with
  Ctrl+Q: the natively saved layout on 1500x950, 800x600 and 1920x1080 displays,
  and the audit profile twice on 1280x900. All showed a 1200x740 window, exited
  0, left no story lock and passed the SQLite integrity check. Screenshots show
  the story open with the saved 30/70 split, and the audit profile's saved
  full-width navigator with the show-panel handle.
- `./tools/check-startup-last-story.sh`, `./tools/check-screenplay-scope.sh` and
  `./tools/check-review-comments.sh`: passed.
- Bash syntax, ShellCheck, changed-code formatting and `git diff --check`: passed.

Traces, mutation logs, per-launch `result.json` files and screenshots are under
[issue10-fix](.build/evidence/issue10-fix/). The retained pre-fix evidence is
`prefix-trace/gdb.log` (every `setSizes` call up to the abort),
`prefix-regression.log` and `native-3-prefix-narrow-screen/`.

**Limits:** Xvfb has no window manager, and the application ran without network,
session bus or keyring, so each launch showed the expected keyring notice.
Physical desktops, window managers, Wayland, moving between monitors while
running, and release builds were not exercised. In a release build the removed
arithmetic had no assertion and was undefined behavior rather than a reported
abort.

Unchanged behavior worth knowing:

- The fallback window is 1200x740 and centred even when the screen is smaller; on
  the 800x600 display it extended beyond the screen.
- A collapsed panel shown while the splitter has no width stays collapsed.
- The splitter's resize helper receives its own proportion list by reference, so
  its step that restores proportions after applying them does nothing for resize
  and design-system events; the pre-fix trace shows the collapsed proportions
  surviving it. By the code, a splitter that is briefly a few pixels wide can
  still hide a panel until the saved state is applied. That was not reproduced.

**Original audit evidence:**
[production failure log](.build/evidence/audit2-20261013/native/application-1.log),
[backtrace](.build/evidence/audit2-20261013/native/startup-backtrace.log),
[verified locals](.build/evidence/audit2-20261013/native/startup-locals.log),
[fresh-layout control](.build/evidence/audit2-20261013/native/fresh-run2/summary.json).
The original profile is retained under `native/profile/`. The first control's
window lookup timed out and required forced cleanup; that run remains separate
and is not counted as ordinary shutdown.

## 11. Fountain emphasis plus a review note crashes import

- [x] **P1 — fixed 2026-10-13; verified through import, native save/reopen and production application startup.**

**Locations:**
[abstract_fountain_importer.cpp](src/corelib/business_layer/import/abstract_fountain_importer.cpp),
paragraph XML construction at 419–424, `appendComments` at 440–471, and final
comment append at 1019–1026;
[text_model_text_item.cpp](src/corelib/business_layer/model/text/text_model_text_item.cpp),
`readXml` at 124–127, 163–219;
[project_manager.cpp](src/core/management_layer/content/project/project_manager.cpp),
`storeScreenplay` around 3891.

**Trigger:** Open a Fountain source containing an emphasized paragraph with an
ordinary review note, for example:

```fountain
INT. ROOM - DAY

!A **bold** clue.[[Check clue]]
```

**Result:** The importer generates an action with child tags in the order
`v`, `fms`, `rms`. The model reader expects review marks before formats and never
returns to consume review marks after formats. The unconsumed `rms` tag then
reaches paragraph parsing and aborts at `paragraphType != Undefined`.
An actual `ApplicationManager` opening this disposable source exited by abort
(134); the retained GDB stack reaches `ProjectManager::storeScreenplay` through
the import manager. Controls removing either the emphasis or the review note
opened and closed ordinarily with exit 0. The original source's SHA-256 remained
unchanged; the fixture itself remains available.

**Debugging direction:** Make the importer's XML order conform to the internal
document contract, or make reading robust to supported child order. Do not mask
the assertion while leaving review data unparsed. Check emphasis and review notes
individually and together, including multiple styled spans/notes, export/fresh
import, and serialized save/reopen. The debug abort is confirmed; release
behavior has not been exercised.

**Evidence:**
[application abort](.build/evidence/audit2-interchange/format-review-app/run.log),
[backtrace](.build/evidence/audit2-interchange/semantics-gdb2.log),
`semantics3-run/comment-after-bold-raw.xml`, the minimal source, probe sources and
retained core under `.build/evidence/audit2-interchange/`.
The passing authored interchange fixture did not combine these features in the
same paragraph; both features are within the documented Fountain support scope.

**Resolution (2026-10-13):** The paragraph reader consumes formatting and review
sections in either supported order before advancing to resource/custom content
and the next paragraph. Fountain's `v`, `fms`, `rms` order and the serializer's
`v`, `rms`, `fms` order both retain their data. The paragraph-type assertion remains
in place; unread review metadata is not discarded or treated as a paragraph.

`./tools/check-screenplay-interchange.sh` now includes an independent combined
fixture: the reported forced-Action reproducer, emphasis-only and review-only
controls, multiple styled spans and two notes, styled dialogue, combined bold and
italic, underline, and a final styled paragraph with a note. Exact text,
per-character styles, comments and review ranges are checked through the actual
import/project managers, explicit serialization and native SQLite save/reopen.
A separate process reopens the original combined screenplay and a filtered fresh
import, checking document UUIDs and exact saved XML before binding the editor.
The semantic oracle joins only contiguous fragments with identical complete
review metadata, because the editor splits a thread at formatting boundaries;
it retains exact coverage and distinct comments. Independent Python checks
verify both XML child orders, authored styles, anchors, comment metadata,
unchanged fixture hashes and exported syntax.

Four production-executable launches on a private Xvfb/XCB display passed: the
minimal reproducer, emphasis-only, review-only and multiple-span/note sources.
Each reached the project screen, exited ordinarily through Ctrl+Q and **Don't
save**, released its source lock and left the original Fountain bytes unchanged.
These are import/startup checks; the native screenshots show the project screen
and expected source-format/keyring dialogs, not a full desktop editor exercise.

Full annotated Fountain export/reimport remains affected by the separate issue 13
path. The combined regression exposed duplicated notes across emphasis fragments
as well as misplaced anchors; that failed run is retained and described below.
The passing combined fixture exports with reviews explicitly disabled and checks
that intended omission alongside unchanged text/styles. This is not credited as
an annotated Fountain round trip. The original combined annotations are checked
without filtering through native persistence. Issues 12–14 remain open.

**Subsequent update:** Issue 13 below now fixes that export path. The combined
fixture's regression exports reviews enabled and verifies the complete annotated
round trip; the earlier filtered result remains historical evidence.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, the expanded
interchange check, screenplay scope check, changed-source formatting, Bash
syntax, ShellCheck, Python compilation and `git diff --check` passed. Evidence:
[interchange](.build/evidence/screenplay-interchange-QfilAM/),
[scope](.build/evidence/screenplay-scope-6xIVc5/),
[production startup](.build/evidence/fountain-review-fix/native-mlyhursb/),
and [builds and earlier attempts](.build/evidence/fountain-review-fix/).
The pre-fix regression aborted in [JsHAz9](.build/evidence/screenplay-interchange-JsHAz9/).
Earlier attempts retain the exporter failure, a review-fragment comparison that
initially counted the same thread repeatedly, an independent XML oracle that
initially omitted the internal HTML-escaping layer, blocked Xvfb sockets, and an
initial production-close probe that did not answer the existing Save question.
Linux debug, offscreen and private Xvfb evidence only; release, physical
desktop/GPU and other operating systems remain unexercised. No user story,
external repository or desktop keyring was used.

## 12. Fountain export/reimport changes literal brackets and backslashes into syntax

- [x] **P1 — fixed 2026-10-13; verified through fixture and model round trips, source Save, clipboard export and fresh-process reopening.**

**Locations:**
[screenplay_fountain_exporter.cpp](src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp),
constructor's escaped-character list at 53;
[abstract_markdown_exporter.cpp](src/corelib/business_layer/export/abstract_markdown_exporter.cpp),
text escaping around 515;
[abstract_fountain_importer.cpp](src/corelib/business_layer/import/abstract_fountain_importer.cpp),
escape processing and `[[` note parsing, including 328–335.

**Trigger:** Script text contains literal Fountain metacharacters, such as
`Screen shows [[message]] then continues.` or folder text containing backslashes.
Save/export to Fountain and reopen/reimport it.

**Result:** The exporter escapes only `*` and `_`. Literal double brackets are
written as review-note syntax, and backslashes become importer escape markers.
The probe's first paragraph changed to `Screen shows  then continues.`, with
`message` moved into a review comment. Its folder paragraph lost its literal
backslashes. Exact before/after model text proves body-content loss, independently
of byte-level whitespace normalization.

The reproduction used the real manager's convenience source-export route and a
fresh import model. It is not a third-party Fountain compatibility test or a full
ordinary-close application probe. Because direct Fountain saving uses this same
route, canonical output can silently change screenplay content on the next open.

**Debugging direction:** Escape all literal syntax the importer recognizes,
including escape characters themselves, while keeping generated formatting and
review syntax distinct. Use model text as an independent oracle for literal
brackets, backslashes and other reserved sequences through export/fresh import
and source Save/close/reopen. This is not an intentional interchange loss in
`docs/INTERCHANGE.md`.

**Evidence:** Before/after JSON, raw/imported XML and exported Fountain in
[literal bracket case](.build/evidence/audit2-interchange/case-literal-comment-markers/checks/)
and [literal backslash case](.build/evidence/audit2-interchange/case-literal-backslash/checks/).
`semantics-status.json` records successful probe exits that confirmed these
mismatches; probe source and runner are in `.build/evidence/audit2-interchange/`.

**Resolution (2026-10-13):** The Fountain exporter now writes a backslash in front
of every character of the script that this application's importer would otherwise
read as syntax:

- every backslash, `*` and `_` (the last two as before);
- a square bracket with an equal bracket beside it, so `[[message]]` is written
  `\[\[message\]\]`. Emphasis markup between two brackets does not separate them,
  because the importer removes it before it looks for notes;
- a single `[` directly in front of a review note, which would otherwise open the
  note one character early;
- a `/` directly in front of emphasis markup that begins with `*`, opening or
  closing, because `/*` opens an inline note.

The text of a comment is escaped the same way on its way into a note, and a `]` at
its end as well. Single brackets, a slash between words and the delimiters the
exporter writes itself stay as they are. The decision is made for each character in
a new `needsEscaping` hook of the Markdown export layer; plain Markdown export
keeps its own character list. The position up to which that layer still has to
escape now only moves towards the start of a paragraph, so it never points into
text that already holds insertions.

The importer already read the character after a backslash as text, in script
text, notes and inline notes; that is unchanged, and it is why the audit's
hand-escaped sources imported correctly. Its search for emphasis did not count
backslashes, though: it skipped a marker after any backslash, including an escaped
one, and it consumed the character in front of each marker. The search now leaves
out every backslash together with the character it escapes and consumes nothing
else. `C:\\**bold**` and `and\/*or*` therefore keep their emphasis.

The defect was wider than the two reported forms. On the unfixed build, with text
typed into the model and written by the exporter:

- `C:\` followed by bold text was written `C:\**bold**` and came back as `C:*bold`
  without the bold.
- `and/` followed by italic `or` was written `and/*or*`. The importer read `/*` as
  the start of an inline note: the paragraph ended at `Either and`, and the rest of
  it, the paragraphs after it, the cue and its speech came back as one inline
  note, up to the next `*/` in the file. With a bold space after the slash
  (`A slash/**** then bold.`) there was no later `*/`, and the paragraph after it
  was not in the imported script at all. A bold slash on its own (`One **/** two.`)
  cut its paragraph the same way.
- Two brackets with only emphasis markup between them (`**ab[**[cd`) opened a note
  that took the following paragraph with it.
- A comment that was a single `]` came back empty, with `]` added to the script. A
  comment ending in a backslash escaped its own closing bracket: the note ran on
  and the next paragraph disappeared into it.
- A probe script of a parenthetical, speeches and a transition holding brackets and
  backslashes produced a review range outside its paragraph; the probe's range
  check stopped there.

With a single rule taken out of the fixed code (the mutation runs below), the other
forms show on their own:

- `A [` with a comment directly after the bracket came back as `A ` with the
  comment `[Bracket note`.
- The comment `Check [[tag]] in C:\dir, 2 * 3 and file_name]` came back as
  `Check [[tag`, with the rest of it in the script text; without only the
  end-bracket rule its last `]` moved into the script.
- The comment `2 * 3 * 4 and [[x]]` came back as `2  3  4 and [[x]]`, and `C:\dir`
  in a comment as `C:dir`.

Two reading faults in the same search are fixed with it and covered: emphasis
directly after an escaped backslash, and combined underline and italic at the very
start of a line. `_*Stressed*_ first word.` imported as underline only, with the
underline covering two characters too many, because a marker in the second
position of a line was never found.

`./tools/check-screenplay-interchange.sh` passed with a new independently authored
fixture and a model-authored script. `literal-syntax.fountain` holds 19 paragraphs
in the exporter's canonical spelling: doubled and tripled brackets, backslashes in
a path, at the start of a line and before a note, a literal `/*` and `*/`, bold
after a backslash, italic after a slash, bold ending in a slash, a bracket in front
of a note, a comment with brackets, a backslash, a star, an underscore and a
closing bracket at its end, a parenthetical, a speech, a forced Action, and
controls with single brackets and a slash between words that must stay unescaped.
It is compared with explicitly listed types, text, styles, comments and review
ranges after import, native save/reopen, export and fresh import, and the exported
bytes must equal the fixture apart from the scene number. Python reads the
importer's raw XML and both saved documents without the application. The
model-authored script holds the same kinds of text with no importer involved, plus
an inline note containing `*/`, a comment with paired asterisks, and a paragraph
ending in spaces of two styles. Its exact output is written out in Python and it
must come back with the listed text, the paragraph ending in spaces as one styled
space. The manager's source Save route must write the same bytes apart from the
scene number and come back the same.

`./tools/check-fountain-review-source.sh` passed **ten ordinary exits** offscreen
and the same **ten on private Xvfb/XCB**; two of each are the new fixture. There the
real editor types ` SAVEMARKER [[typed]] D:\x` at the end of the script, the
ApplicationManager Save coordinator writes it as
`SAVEMARKER \[\[typed\]\] D:\\x`, and after an ordinary close a fresh application
import holds the typed text again. Saved bytes equal the authored fixture plus the
typed text, repeated and fresh-process saves are byte-identical, the full clipboard
selection equals the saved source, and the saved model holds the listed types and
text with exactly three paragraphs carrying a comment. The native screenshots show
the application window and navigator; the editor area is not painted in them, so
they are not evidence of editor rendering.

Without the fix the interchange check fails twice over. Its fixture stops at
import with the three emphasis rows and the line-start row above, and a copy of
the test without that fixture stops at the model-authored script's fresh import,
with the changed text listed above. Twelve single-rule mutations of the fixed code
each failed the interchange check for their own reason: no backslash escape, no
doubled-bracket rule, no bracket-before-note rule, no slash rule, comment not
escaped, comment without the end-bracket, emphasis, doubled-bracket or backslash
rule, escapes not hidden from the emphasis search, the old search, and the
escaping position allowed to grow. The last one aborts on a Qt bounds assertion for
the paragraph that ends in spaces of two styles. The first eleven also failed the
source check. Each mutation was restored, the restored sources matched their
checksums, and the build and both checks passed again afterwards.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, the interchange,
source (offscreen and Xvfb), screenplay scope, source-export-failure,
import-only-sources and startup-last-story checks, changed-code formatting, Bash
syntax, ShellCheck, Python compilation and `git diff --check` passed. Retained
evidence: [interchange](.build/evidence/screenplay-interchange-VW4TVR/),
[source checks](.build/evidence/fountain-review-source-7WrS0g/),
[native source checks](.build/evidence/fountain-review-source-ETyZTf/),
[scope](.build/evidence/screenplay-scope-WH3MCd/), and
[builds, failing runs, mutations and probes](.build/evidence/literal-syntax-fix/).
The first failing runs are [the fixture](.build/evidence/screenplay-interchange-qxKnau/)
and [the model-authored script](.build/evidence/screenplay-interchange-1K5ORD/);
`probe-LriTxl` and `probe-eEe5x4` under the last link hold the further forms
before and after the fix.

The rules follow this application's importer. Fountain describes the backslash
for emphasis characters only, so another program may show the other backslashes
and is not covered. A source with an unescaped literal `[[text]]` or backslash,
written by hand or saved by a version before this fix, is still read as a note or
an escape: nothing in the file tells the two apart. Emphasis that covers only
spaces is not kept: a bold space between two plain words comes back plain, and
trailing spaces of two styles come back as one styled space. A scene heading
ending in `#` and a cue ending in `^` were not examined. A speech beginning with a
reserved character remains issue 17. Issues 18, 19 and 20 below were confirmed with
separate probes and are not fixed here. Release, physical desktop/GPU, other
operating systems and third-party Fountain applications remain unexercised.

## 13. Fountain escaping shifts a review note onto the wrong text

- [x] **P2 — fixed 2026-10-13; verified through annotated export/reimport, source Save, clipboard exports and fresh-process reopening.**

**Locations:**
[screenplay_fountain_exporter.cpp](src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp),
88–89;
[abstract_markdown_exporter.cpp](src/corelib/business_layer/export/abstract_markdown_exporter.cpp),
515 and 525.

**Trigger:** A review mark covers text containing characters that export must
escape, such as a literal pair of asterisks in `A *literal* clue.`.

**Result:** Review insertion uses original model offsets after Markdown escaping
has added characters to the output string. A mark covering `[0,17)` exported as
`A \*literal\* clu[[Check clue]]e.` and reimported with range `[0,15)`.
The script text and comment text survive, but the review range no longer covers
the same source text. A text-only or comment-text-only comparison misses this.

**Debugging direction:** Map source offsets to escaped output positions, or emit
text/formatting/review delimiters together from source intervals. Check exact
review spans alongside text, including multiple escapes and emphasis runs.

**Evidence:** Before/after XML and the exported Fountain in
[the escaped-star comment case](.build/evidence/audit2-interchange/case-comment-after-escaped-star/checks/).
The probe used actual export/fresh-import paths; no physical desktop or full
application database-reopen claim is made.

**Supplemental observation while fixing issue 11 (2026-10-13):** Emphasis also
changes output positions before review insertion. A single review range is split
into formatting fragments in the text document; the Fountain exporter inserts
the same comment once for each fragment at those original offsets. The combined
fixture exported `A [[Check clue]]**bo[[Check clue]]ld** c[[Check clue]]lue.`
from `A **bold** clue.[[Check clue]]`. Fresh import produced invalid style ranges.
The failed comparison and exact output remain in
[RwQFi1](.build/evidence/screenplay-interchange-RwQFi1/).
Issue 11's reader fix does not repair this exporter path. Resolution here must
also avoid duplicated comments and preserve styles alongside exact review spans.

**Resolution (2026-10-13):** The Markdown formatting layer now records a map from
each UTF-16 source boundary to its escaped/formatted output position. Review
notes follow closing emphasis at that boundary and precede opening emphasis or
an escape prefix belonging to the next character. Fountain uses this map when
inserting notes. Adjacent text-format fragments are joined only when all review
metadata matches, including authors, dates, reply lists, status/flags and colors;
font differences alone no longer emit another copy of the same note.

The importer projects collected style ranges when removing note syntax or escape
prefixes, including spans that cross a note. This keeps later styles at their
actual text positions. Adjacent `[[...]][[...]]` entries retain one review range
with the replies in order, instead of creating an invalid range past the paragraph.

`./tools/check-screenplay-interchange.sh` passed complete annotated round trips
for the previous combined fixture and an independent `review-offsets.fountain`
fixture. It checks repeated literal asterisks/underscores, mixed literal and styled
text, multiple distinct notes, replies, dialogue, combined bold/italic/underline,
a style spanning a note and escaped text, note boundaries immediately before an
escape/opening style, and an emoji with UTF-16 offsets. Every character, style
bit, comment and represented review range is compared before/after export and
native serialization/save/reopen. A separate process reopens all four original
and fresh-imported scripts, verifying exact XML and UUIDs before editor binding.
Python independently checks source hashes, internal XML ranges and exact output
syntax, requiring each note/reply once at the authored boundary.

The new `./tools/check-fountain-review-source.sh` passed **four ordinary exits**
offscreen and the same **four on private Xvfb/XCB**. Both fixtures cross the actual
ApplicationManager Save coordinator after a real pending editor edit, ordinary
close and a fresh application import. Repeated Save, fresh-process Save and final
close preserve the exact expected source bytes. Per-character styles and review
coverage remain exact; full and partial clipboard selections also match independent
annotated syntax. Tests choose **Keep .FOUNTAIN** before editing and answer an
observed asynchronous close Save question once when needed. Credentials are an
asynchronous in-memory backend; temporary clipboard files, projects and profiles
are disposable. Native screenshots are diagnostic captures, not an assertion of
completed editor rendering or physical desktop/GPU behavior.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, the expanded
interchange check, screenplay scope check, changed-code formatting, Bash syntax,
ShellCheck, Python compilation and `git diff --check` passed. Retained evidence:
[interchange](.build/evidence/screenplay-interchange-8j17du/),
[scope](.build/evidence/screenplay-scope-TIOcHj/),
[source/clipboard checks](.build/evidence/fountain-review-source-ridkiM/),
[native source checks](.build/evidence/fountain-review-source-blAFe3/),
and [builds, original failure and earlier probes](.build/evidence/fountain-review-offsets-fix/).
The original annotated regression fails on `d86a057d`; its invalid emphasis
ranges and duplicated output remain in the pre-fix evidence. Earlier native
harness attempts left the existing source-format or late Save question unanswered;
their logs/screenshots are retained. No retry, forced shutdown or relaxed data
assertion is counted as a passing close.

The separate first-edit Undo assertion was reproduced against the previous code
and remains unchecked as issue 15 below. The passing source/export cases do not
claim that workflow passes. Fountain's generated author/date/status/color policy
and inability to express every arbitrary native review start remain unchanged;
these checks cover the represented spans authored in the fixtures. Issues 12 and
14 remain open. Release, physical desktop/GPU, other operating systems and
third-party Fountain applications remain unexercised.

## 14. Fountain export loses forced Action types and changes their content

- [x] **P2 — fixed 2026-10-13; verified through fixture and model round trips, source Save, clipboard export and fresh-process reopening.**

**Location:**
[screenplay_fountain_exporter.cpp](src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp),
Action handling at 193–200.

**Trigger:** Import valid forced Action such as `!# A private instruction`,
`!@address`, `!>A direction`, `!= A hint`, `!~ A hummed sound`, or
`!INT. The camera opens - Day`. Save/export it and reimport.

**Result:** The original model classifies all six as Action. The exporter adds
the forcing `!` only when the whole paragraph is uppercase, so these mixed/lower
case Action paragraphs are written without it. Fresh import reclassifies them as
sequence heading, character, transition, beat heading, lyrics and scene heading,
respectively. Several also lose their literal leading symbols or have their
capitalization changed. The false scene heading acquires a scene number.

**Debugging direction:** Preserve Action for every paragraph whose emitted text
would trigger another Fountain paragraph rule; the uppercase heuristic alone is
insufficient. Test all reserved initial markers and heading heuristics against
independent text/type expectations through source Save and fresh reopen.

**Evidence:**
[before snapshot](.build/evidence/audit2-interchange/semantics-run/action-prefixes-before.json),
[after snapshot](.build/evidence/audit2-interchange/semantics-run/action-prefixes-after.json),
independent input/export fixtures in the same directory, and the separate
`case-action-prefixes/checks/` repeat under `.build/evidence/audit2-interchange/`.
These changes exceed documented whitespace/forced-syntax normalization, which
does not permit unrelated text or paragraph-type differences.

**Resolution (2026-10-13):** The exporter now decides the forcing `!` for each
physical line of an Action paragraph from that line's own text, rather than from
the case of the whole paragraph. A line is forced when this application's importer
would otherwise read it as another paragraph, strip its first characters or skip it:

- a first character among `.`, `!`, `@`, `>`, `=`, `~` and `#`;
- a scene-heading start from the importer's own list (`INT`, `EXT`, `EST`, `I/E`
  and their combinations, with any translated spellings);
- text wrapped in parentheses, which is a parenthetical after a cue or a speech;
- a title page key followed by a colon, which begins a title page when it is the
  first line of a file;
- an uppercase line after an empty line, as before, now judged from the first
  written line of each physical line instead of the whole paragraph.

The first four tests sit beside the importer rules they mirror, in
`AbstractFountainImporter::startsAnotherBlock`. Line-start positions are kept
through review-note insertion, because a note can carry line breaks of its own.
Ordinary Action lines are written exactly as before, and a `!` that a source does
not need is still dropped on export.

The same defect was confirmed in further forms, all now covered. Importing the
unfixed exporter's retained output again (without its bold line, which stops the
comparison harness earlier) showed `.A dotted remark` and
`...and then silence.` as scene headings (the second as `..AND THEN SILENCE.`),
`!Twice forced` without its `!`, `(Later that night)` after a speech as a
parenthetical without its parentheses, `>Centered words<` without both markers,
and `ESTHER walks in.`, `EXT is short for exterior.` and `I/E reads as both.` as
scene headings. A first line `Source: a live broadcast.` was taken for a title
page and left out of the script entirely. An Action paragraph with embedded line
breaks is written as one line per physical line: with the old whole-paragraph test
`BOB` + break + `walks in.` came back as a cue and its dialogue, and `BANG` with a
note beginning `SEE` and continuing on a second line came back as a cue; with
only the first line forced, inner lines `# 1 in sales`, `INT. means interior.`
and `~ humming` came back as a sequence heading, a scene heading and lyrics.

`./tools/check-screenplay-interchange.sh` passed with two new independently
authored fixtures and one model-authored script. `forced-action.fountain` holds 19
forced Action paragraphs (each reserved first character, each heading start, the
parenthesized line after a speech, centered text, a page-break line, a bold and a
reviewed variant, an uppercase line) beside ordinary Action controls that must
stay unforced. `forced-action-first.fountain` holds title-key lines, including
the first line of the file, and two controls. Each fixture is compared with
explicit types and text after import, native save/reopen, export and fresh import,
and the exported bytes must equal the fixture apart from the scene number. The
model-authored script covers per-line forcing, a note with a line break before a
forced line and the uppercase note case, with exact output text.

`./tools/check-fountain-review-source.sh` passed **eight ordinary exits** offscreen
and the same **eight on private Xvfb/XCB**; four of each are the two new fixtures.
They cross the actual ApplicationManager Save coordinator after a real pending
editor edit in a forced paragraph, an ordinary close and a fresh application
import. Saved bytes equal the authored fixture plus the typed marker, repeated
and fresh-process saves are byte-identical, the full clipboard selection equals
the saved source, and the saved model holds the explicitly listed types and text.
The native screenshots show the application window and navigator; the editor area
is not painted in them, so they are not evidence of editor rendering.

Without the fix both checks fail: the interchange check at the fresh import of
`forced-action`, the source check at its fresh application import, after the
application's own Save had written the unforced source. Eight
single-rule mutations (old uppercase-only test, no marker rule, no parenthetical
rule, no heading rule, no title-key rule, no note offset, whole-paragraph
uppercase, first line only) each failed the interchange check; the first five
also failed the source check. The other three need a paragraph with an embedded
line break, which a Fountain source cannot hold. Each mutation was restored,
rebuilt and followed by the passing runs below.

`./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug`, the interchange,
source (offscreen and Xvfb), screenplay scope, source-export-failure,
import-only-sources and startup-last-story checks, changed-code formatting, Bash
syntax, ShellCheck, Python compilation and `git diff --check` passed. Retained
evidence: [interchange](.build/evidence/screenplay-interchange-lf4aOw/),
[source checks](.build/evidence/fountain-review-source-bUmYlR/),
[native source checks](.build/evidence/fountain-review-source-1vC0ad/),
[scope](.build/evidence/screenplay-scope-IFjP51/), and
[builds, failing runs, mutations and probes](.build/evidence/forced-action-fix/).
The first failing runs are [unfixed](.build/evidence/screenplay-interchange-Bs2AHU/)
and [before the title-key rule](.build/evidence/screenplay-interchange-0qaB8s/).

Forcing does not look at the surrounding paragraphs. A parenthesized or key-like
Action line therefore gains `!` even where the importer would have read it as
Action; this is harmless and stable on the next save. The rules follow this
application's importer: heading starts are case-sensitive and only its eight title
keys count, so a third-party reader with wider rules (lowercase `int.`, any
`key: value` first line) is not covered. Empty lines inside an Action paragraph
and a review range that crosses an embedded line break were not exercised.
Literal `[[`, `/*` and backslashes in Action text remain issue 12. Issues 16 and 17 below were confirmed with a separate probe and
are not fixed here. Release, physical desktop/GPU, other operating systems and
third-party Fountain applications remain unexercised.

## 15. Undo of the first edit after importing styled review notes aborts

- [ ] **P1 — confirmed in the real editor/coordinator and reproduced against the previous core library.**

**Locations:**
[text_model.cpp](src/corelib/business_layer/model/text/text_model.cpp),
`TextModel::applyPatch`, final `newContent == toXml()` assertion around 2887;
[abstract_model.cpp](src/corelib/business_layer/model/abstract_model.cpp),
`saveChanges`, `undo` and `undoChange`;
[text_document.cpp](src/corelib/business_layer/document/text/text_document.cpp),
editor/model review-format projection.

**Trigger:** Open `emphasis-review.fountain` in a disposable application profile,
choose **Keep .FOUNTAIN**, select the screenplay editor, type ` SAVEMARKER` at its
end, then Undo. The controlled editor probe also fails after an initial Save.

**Result:** The debug application aborts at `TextModel::applyPatch`'s exact XML
comparison. The Undo patch includes initial review normalization (foreground
color and formatting fragments) as well as the authored edit. Reconstructed
model XML differs from the patch target. This is an Undo/history defect, separate
from Fountain output offsets; no assertion was disabled for the issue 13 fix.
Release behavior and physical keyboard input remain unexercised.

**Reproduction:** `STARC_REVIEW_SOURCE_MODES=cold-undo ./tools/check-fountain-review-source.sh`.
This explicit case fails rather than being included among the passing Save/reopen
cases. It uses disposable sources/profiles, Qt editor actions and a fake
credential backend. No user script or real keyring is involved.

**Evidence:**
[normal source-format flow](.build/evidence/fountain-review-source-Q7zyOJ/),
[earlier first-edit failure](.build/evidence/fountain-review-source-SYDizx/),
and [previous-code control](.build/evidence/fountain-review-offsets-fix/baseline-flwio6x9/normal-flow-undo/).
The control rebuilds the four changed importer/exporter objects from `d86a057d`
and links them with the unchanged debug objects in a separate library, leaving
the working application libraries intact. The same normal-flow editor probe
aborts there too. A separate previous-code probe also aborts before any export.
Build/loader setup failures remain under the other `baseline-*` evidence paths.

**Debugging direction:** Trace editor initialization, review projection and
history ownership through the first edit and Undo. Preserve complete annotations,
source text and exact model/XML equality through Undo/Redo and save/reopen;
do not suppress the assertion, clear history, or add delays to conceal it.

## 16. Fountain export leaves out Shot paragraphs

- [ ] **P1 — confirmed through the actual exporter and a fresh import manager while fixing issue 14.**

**Location:**
[screenplay_fountain_exporter.cpp](src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp),
`processBlock`: there is no case for `TextParagraphType::Shot`, so it reaches the
default branch that skips unknown blocks. `addIndentationAtBegin` in the same file
already lists Shot. Frozen `master` has the same code.

**Trigger:** A screenplay contains a Shot paragraph, typed in the editor or
imported from FDX. Export it to Fountain.

**Result:** The paragraph is not written. A model-authored script with the Shot
`CLOSE ON THE LETTER` between a scene heading and an Action exported without that
line, and the fresh import has no such paragraph. The neighbouring paragraphs
were unaffected.

The probe used the exporter and the import manager directly. An application
source Save was not run for this case; direct Fountain saving uses the same
exporter, so a Shot is expected to disappear from the source on Save.

**Debugging direction:** Fountain has no Shot element. Decide how the text is kept
(an uppercase forced Action keeps the words and loses the type) and record the
type change in `docs/INTERCHANGE.md` if it is accepted. Check the other
paragraph types that reach the default branch.

**Evidence:** Probe source, run log and exported Fountain in
[adjacent-probe](.build/evidence/forced-action-fix/adjacent-probe/).

## 17. Fountain export/reimport turns dialogue with a reserved line start into headings and cues

- [ ] **P1 — confirmed for four dialogue lines through the actual exporter and a fresh import manager while fixing issue 14.**

**Locations:**
[abstract_fountain_importer.cpp](src/corelib/business_layer/import/abstract_fountain_importer.cpp),
`blockType` and the scene-heading test in `documentText`, which run before the
dialogue rule;
[screenplay_fountain_exporter.cpp](src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp),
Dialogue handling, which writes the text unchanged.

**Trigger:** A speech begins with `#`, `@`, `.` or a scene-heading start, such as
`#1 is the best.`, `@home, he says.`, `...and then nothing.` or
`INT. is short for interior.`. Export to Fountain and import again.

**Result:** The four speeches came back as a sequence heading `1 is the best.`, a
cue `home, he says.`, a scene heading `..and then nothing.` and a scene heading
`INT. is short for interior.`. Types change, leading characters are lost and the
false headings add a sequence and scenes. A speech that trails in with an
ellipsis is ordinary screenplay text, so the `.` case does not need unusual
input. A hand-written Fountain file with such a speech imports the same way
without any export.

Fountain's `!` forces Action only, so the issue 14 fix cannot be applied to a
speech. Starts with `!`, `>`, `=` and `~` take the same importer path and were
not run. A parenthetical beginning `~` round-tripped correctly.

**Debugging direction:** Fountain forces a scene heading only for a single period
followed by a letter or digit; reading two or more leading periods as text would
remove the ellipsis case for every paragraph type, including hand-written
sources. The remaining starts need an escape that the importer honours before
classification, which belongs with issue 12. Check type, complete text and the
absence of new headings through export/fresh import and source Save/reopen.

**Evidence:** Probe source, run log and exported Fountain in
[adjacent-probe](.build/evidence/forced-action-fix/adjacent-probe/).

**Subsequent update (2026-10-13):** The issue 12 fix escapes characters inside a
line but leaves the start of a speech as it was, so this issue is unchanged. The
escape the direction above asks for is already read: hand-written speeches
`\#1 is the best.`, `\@home, he says.`, `\...and then nothing.`,
`\INT. is short for interior.`, `\!Bang.`, `\>Onward.`, `\= Equal.` and `\~ Hum.`
after a cue all imported as dialogue with their complete text, because the
importer decides the paragraph type from the backslash and then reads the
character after it as text. The exporter does not write that backslash for a
speech. Source and log are `escaped-starts.fountain` and `probe.log` in
[import-probe](.build/evidence/literal-syntax-fix/import-probe/).

## 18. A review comment on an inline note comes back as part of the note's text

- [ ] **P2 — confirmed through the actual exporter and a fresh import manager while fixing issue 12.**

**Locations:**
[screenplay_fountain_exporter.cpp](src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp),
`processBlock`: review notes are inserted before the InlineNote case wraps the
paragraph in `/* */`;
[abstract_fountain_importer.cpp](src/corelib/business_layer/import/abstract_fountain_importer.cpp),
`Implementation::processBlock`, the `[` case, which opens no note inside an inline
note.

**Trigger:** Add a review comment to an inline note. Export to Fountain, or save a
Fountain source, and import it again.

**Result:** A model-authored inline note `private` with the comment `about it` was
written `/* private[[about it]] */`. The fresh import holds an inline note with the
text `private[[about it]]` and no review comment. A hand-written source with the
same line imports the same way. Since the issue 12 fix the next save writes those
brackets escaped, so they stay script text from then on.

The probe used the exporter and the import manager directly; an application
source Save was not run for this case.

**Debugging direction:** Either read a note inside an inline note, with its range
counted in the note's own text, or do not write one there and record the loss in
`docs/INTERCHANGE.md`. Check the comment, its range and the note text through
export/fresh import and source Save/reopen.

**Evidence:** Script 8 in
[probe-eEe5x4](.build/evidence/literal-syntax-fix/probe-eEe5x4/) and
`note-in-comment.fountain` with `probe.log` in
[import-probe](.build/evidence/literal-syntax-fix/import-probe/).

## 19. Fountain import removes asterisks and underscores that are text

- [ ] **P1 — confirmed with hand-written sources through the importer while fixing issue 12.**

**Locations:**
[abstract_fountain_importer.cpp](src/corelib/business_layer/import/abstract_fountain_importer.cpp),
`Implementation::processBlock`, the `*` case: an asterisk outside a review note is
never added to the text;
[abstract_markdown_importer.cpp](src/corelib/business_layer/import/abstract_markdown_importer.cpp),
`collectSelectionTypes`, which pairs any two equal markers of a line, inside a
review note as well;
[screenplay_text_edit.cpp](src/core/management_layer/plugins/screenplay_text/text/screenplay_text_edit.cpp),
`insertFromMimeData`, which passes pasted plain text to the same importer.

**Trigger:** Open a Fountain source, written by hand or by another program, that
holds an asterisk without a partner or a note with such characters, for example
the speech `It costs 2 * 4.` or the note
`[[see file_name and other_name, 2 * 3 * 4]]`.

**Result:** The speech came back as `It costs 2  4.`. In
`He writes 5 * 3 = 15 and a footnote* here.` the two asterisks were taken for a
pair: both were removed and ` 3 = 15 and a footnote` became italic. The note came
back as `see filename and othername, 2  3  4`. Fountain reads an asterisk without
a partner as text. A source opened in place is rewritten on Save, so these
characters are gone from the file after the next Save.

Text typed in the application is not affected: the exporter escapes these
characters, in comments too since the issue 12 fix. Plain text pasted into the
screenplay editor goes through the same importer and by the code loses the same
characters, and a backslash in front of an ordinary character with them
(`C:\temp` in a source file came back as `C:temp`). The paste itself was not run.

**Debugging direction:** Keep an asterisk or underscore without a partner as text,
and decide whether emphasis is looked for inside a review note at all. For paste,
decide whether plain text that is not Fountain should pass through the Fountain
reader. Check that the escapes of issue 12 still round-trip, and compare complete
text and styles for hand-written sources.

**Evidence:** `lone-star.fountain`, `note-emphasis.fountain` and `probe.log` in
[import-probe](.build/evidence/literal-syntax-fix/import-probe/).

## 20. Imported character and location names keep Fountain markup

- [ ] **P2 — confirmed with a hand-written source through the importer while fixing issue 12.**

**Location:**
[abstract_fountain_importer.cpp](src/corelib/business_layer/import/abstract_fountain_importer.cpp),
`importDocuments`, which reads the source lines on its own, without the emphasis,
escape and note handling of the script import.

**Trigger:** Import or open a Fountain source whose cue or scene heading holds
emphasis or an escaped character, for example `**MARA**`, `JO\_ANN` or
`INT. LAB \[\[A\]\] - DAY`. The last two are what the exporter writes for the cue
`JO_ANN` and the heading `INT. LAB [[A]] - DAY`.

**Result:** The script text is right: `MARA` in bold, `JO_ANN` and
`INT. LAB [[A]] - DAY`. The documents returned for the character and location
lists are named `**MARA**`, `JO\_ANN` and `LAB \[\[A\]\]`.

The probe called `importDocuments` directly. The documents a story ends up with
were not inspected in the application.

**Debugging direction:** Take the names from the same cleaned text as the script
paragraphs. Compare the imported names with the script's cues and headings,
including a cue that carries a review note.

**Evidence:** `names.fountain` and `probe.log` in
[import-probe](.build/evidence/literal-syntax-fix/import-probe/).

## Validation, coverage and remaining limits

The following current commands passed; logs and the scripts' own evidence paths
are retained under [.build/evidence/audit2-20261013/](.build/evidence/audit2-20261013/):

```bash
./build/dev.sh doctor
JOBS=4 ./build/dev.sh build debug
./tools/check-screenplay-scope.sh
./tools/check-screenplay-interchange.sh
./tools/check-backup-retention.sh
./tools/check-github-credential-storage.sh
./tools/check-project-sync.sh
./tools/check-writing-session-storage.sh
./tools/check-startup-last-story.sh
./tools/check-save-failure.sh
./tools/check-import-only-sources.sh
```

Qt 6.12.0 and the prepared local QtKeychain dependency were used. Optional
`lrelease` remains absent. The isolated native Secret Service test and a separate
unavailable-provider test both exited 0. They used a new private D-Bus and a
disposable keyring; the owner's desktop keyring was never accessed. The native
test's first attempt lacked the test daemon on PATH; its log remains separate
from the successful run using the already prepared local daemon/libraries.
The save-failure suite's first attempt was blocked by sandbox restrictions on
localhost sockets; the full permitted loopback rerun passed. Neither setup
failure is classified as an application defect.

The passing suite covers ordinary keyboard editing, paragraph transitions,
Undo/Redo, pagination, selected exports/imports, existing backup-retention rules,
credential ordering/failures, sync baselines, save-failure handling, startup,
import-only source preservation and legacy-document retention. It does not cover
the new reproductions above; a passing suite is not a clean data-safety finding.
Fault probes deliberately exercise additional boundaries. Several emit exit 0
because their assertions confirm a defect; logs explain the observed invariant.
Failed assertions, aborted processes and harness failures have been retained.

Three focused review agents covered persistence/sync, editing/models and
interchange/retirement. The parent reviewed application lifecycle, coordinated
the baseline checks and reproduced Save As, stale lock callbacks and native
startup. Structural discovery used the codebase-memory graph generation
`2026-10-13T07:52:59Z` (refreshed to `2026-10-13T08:23:22Z` during report creation)
with explicit path coverage checks and direct source
fallback for flagged ranges and Qt lambdas. The screenplay-text manager and FDX
importer have unusable graph coverage and were inspected directly. Graph absence
was not used as proof that a path is unreachable.

This was a risk-focused code review and Linux debug validation, with offscreen
editor/coordinator probes and a private Xvfb production startup/close sample.
Release packaging, other operating systems, physical desktop/GPU behavior,
external GitHub authentication/network failures, every editor interaction,
every legacy schema and exhaustive crash/power-loss/storage faults remain
unexercised. Reported bugs are concrete; no claim is made that this inventory
contains every defect or that unexercised paths are broken.

Only this report is a new worktree file. Probe code, disposable data and logs
live in ignored `.build/evidence/` directories and are local supporting artifacts,
not committed regression tests. Preserve them when debugging these findings;
promote the relevant reproductions into focused tests alongside future fixes.
