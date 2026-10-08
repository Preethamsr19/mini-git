# MiniGit — A Lightweight Version Control System

A version control system written from scratch in C++17, implementing the core
data model that Git itself uses: a content-addressed object store and a
hash-linked chain of commits.

No external libraries. Standard library and `std::filesystem` only.

## Build and Run

```bash
g++ -std=c++17 -O2 -o minigit main.cpp
```

On older GCC toolchains that do not link `std::filesystem` by default, append
`-lstdc++fs`.

## Commands

| Command | Description |
|---|---|
| `minigit init` | Create a repository in the current directory |
| `minigit add <file>` | Hash a file, store it as a blob, and stage it |
| `minigit status` | Show staged, modified and missing files |
| `minigit commit <message>` | Record a snapshot of all tracked files |
| `minigit log` | Walk the commit history backwards from HEAD |
| `minigit checkout <commit>` | Restore the working directory to a past snapshot |

Commit ids may be abbreviated, the way `git checkout a1b2c3` works.

## Example

```
$ minigit init
Initialized empty MiniGit repository.

$ minigit add a.txt
Staged: a.txt (8e4f893)

$ minigit add b.txt
Staged: b.txt (49448e7)

$ minigit commit "Initial commit"
Committed e4c8a11: Initial commit
2 file(s) in this snapshot.

$ echo "changed" > a.txt
$ minigit status
Modified (not staged):
  a.txt

$ minigit add a.txt
Staged: a.txt (b407a42)

$ minigit commit "Update a.txt"
Committed 148dcd7: Update a.txt
2 file(s) in this snapshot.

$ minigit checkout e4c8a11
Restored 2 file(s) from commit e4c8a11
```

Note that the second commit contains two files even though only `a.txt` was
staged. A commit is a complete snapshot, not a record of what changed.

## How It Works

### The object store

Every piece of data is stored in `.minigit/objects/`, named by the SHA-1 hash of
its own contents. SHA-1 is implemented by hand in `main.cpp` — padding, the
80-word message schedule, and the four rounds — rather than pulled from a
library.

Objects are stored at `.minigit/objects/<first 2 chars>/<remaining 38 chars>`.
The two-character fan-out keeps any single directory from holding tens of
thousands of entries, which is the same layout real Git uses.

Because an object's name is derived from its contents, identical content
produces an identical name and is written once. Staging the same file under ten
different names, or committing an unchanged file across a hundred commits,
stores exactly one copy of those bytes.

### The staging area

`.minigit/index` holds one line per staged file:

```
<blob hash> <path>
```

`add` reads the file, writes its blob to the object store, and records the
hash against the path. Re-staging an edited file updates its hash.

### Commits

A commit is itself an object in the store. Its contents are plain text:

```
parent <commit id, or "none" for the first commit>
timestamp <...>
message <...>
file <blob hash> <path>
file <blob hash> <path>
```

The `parent` line is what turns a set of commits into a history. `.minigit/HEAD`
records the id of the current commit; `log` reads HEAD and follows parent
pointers back to the root.

Committing starts from the parent's file list and overwrites the entries that
were staged, so untouched files remain in the snapshot pointing at the same
blob they always did. This is what makes `checkout` able to reconstruct a
complete working directory from any commit.

### Status

`status` hashes each file in the working directory and compares it against the
hash recorded in the index, or in the current commit if the file is not staged.
A file whose hash differs is modified; a tracked file that no longer exists is
reported as missing. Nothing is tracked by modification time.

## What This Does Not Do

Deliberately out of scope, to keep the core model clear:

- **Branching and merging.** The commit format has a single parent field. Branches
  would need named refs; merging would need a second parent and conflict resolution.
- **Delta compression.** Each version of a file is stored whole. Real Git packs
  similar objects into deltas to save space.
- **Compression.** Blobs are stored as raw bytes; Git zlib-compresses them.
- **Diffs.** There is no `diff` command. Status reports *that* a file changed,
  not *what* changed inside it.
- **Directory tracking.** Only explicitly added files are tracked. There is no
  recursive `add .`.
- **Removing files from a commit.** Once a path enters a snapshot it carries
  forward; there is no `rm` command.

## Repository Layout

```
.minigit/
├── HEAD             current commit id
├── index            staging area: "<hash> <path>" per line
└── objects/
    └── ab/
        └── cdef...  blobs and commit objects, named by SHA-1
```
