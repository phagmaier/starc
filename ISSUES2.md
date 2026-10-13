# Second fork audit: remaining issues

Reviewed on 2026-10-13 against clean `dev` at
`1580693c4123650a50aa79c5f4d503a81ea51cfe`. Frozen `master` remains
`4b9677dbfb5c1faaa91c55c415fdf12715f8521d` and was only read.
No application fixes, commits or pushes were made during this review.

The build and all nine existing focused regression scripts passed. Additional
fault, concurrency and editor probes nevertheless confirmed the fourteen issues below.
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

- [ ] **P2 — confirmed through the real screenplay editor/model.**

**Location:**
[text_document.cpp](src/corelib/business_layer/document/text/text_document.cpp),
`addReviewMark` at 2053–2078; UI route in
[screenplay_text_view.cpp](src/core/management_layer/plugins/screenplay_text/screenplay_text_view.cpp),
1300–1303, through `ScreenplayTextEdit::addReviewMark`.

**Trigger:** In `Alpha Beta Gamma`, comment on `Alpha `, then add another comment
on `Beta`, using the same author and color.

**Result:** Adjacent ranges are considered overlapping. The first mark expands
from `[0,6)` to `[0,10)`, but its only comment remains “first comment”; “second
comment” is never stored. The merge branch expands the old range and returns
without consuming the incoming `_comment`.

**Debugging direction:** Preserve meaningful incoming comment/thread payloads
when ranges merge. Distinguish merging highlight or revision ranges from adding
a comment. Check adjacent and overlapping comments, including different authors,
colors and existing threads.

**Evidence:** [editor log](.build/evidence/audit2-editor/run2.log),
`adjacent-comments.xml` and the independent `postcheck.txt` under
`.build/evidence/audit2-editor/`.

## 9. Deleting between visible paragraphs destroys a hidden beat heading

- [ ] **P2 — confirmed with an invisible beat block before deletion.**

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

## 10. Window restoration can abort when the splitter has zero total size

- [ ] **P2 — confirmed production startup abort on private Xvfb/XCB.**

**Locations:**
[splitter.cpp](src/corelib/ui/widgets/splitter/splitter.cpp), `setSizes` at
305–337 and 363; child Show handling at 558–563; state validation at 460–482;
[application_view.cpp](src/core/ui/application_view.cpp), `restoreState` at
197–207.

**Trigger and result:** During window restoration a child Show event can call
`setSizes` with a zero total size. The method divides both the available extent
and each requested size by that total, then converts non-finite results to integer
widget geometry. Startup aborts before the story opens.

The production debug executable reproduced this with an ordinary saved profile
copied from the offscreen startup fixture onto private XCB. GDB showed
`sizesMax=0` and `sizeDelta=NaN` in `Splitter::setSizes`. The stack reaches
`QRect::setWidth` with `-2147483648`, then Qt's checked-integer overflow assertion.
No application code was modified to produce the abort.

Removing the saved window-layout section from a separate disposable profile
allowed two production launches and normal Ctrl+Q exits. Their exact 376-byte
screenplay content survived, and SQLite integrity checks passed. That control
does not repair the failing state or establish physical-desktop behavior.
The observed trigger includes an offscreen-generated profile; an exclusively
native user-session sequence has not been established.

**Debugging direction:** Reject or safely defer zero-total/non-finite splitter
sizes before geometry arithmetic. Validate restored state and zero-extent Show
events, retain valid proportions through layout initialization, and test native
launch with saved layout state. Avoid accepting `{0,0}` as usable proportions.

**Evidence:**
[production failure log](.build/evidence/audit2-20261013/native/application-1.log),
[backtrace](.build/evidence/audit2-20261013/native/startup-backtrace.log),
[verified locals](.build/evidence/audit2-20261013/native/startup-locals.log),
[fresh-layout control](.build/evidence/audit2-20261013/native/fresh-run2/summary.json).
The original profile is retained under `native/profile/`. The first control's
window lookup timed out and required forced cleanup; that run remains separate
and is not counted as ordinary shutdown.

## 11. Fountain emphasis plus a review note crashes import

- [ ] **P1 — confirmed by real application startup and a retained backtrace.**

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

## 12. Fountain export/reimport changes literal brackets and backslashes into syntax

- [ ] **P1 — confirmed through the actual export manager and fresh import manager.**

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

## 13. Fountain escaping shifts a review note onto the wrong text

- [ ] **P2 — confirmed with independent before/after annotation ranges.**

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

## 14. Fountain export loses forced Action types and changes their content

- [ ] **P2 — confirmed for six independently authored forced Action paragraphs.**

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
