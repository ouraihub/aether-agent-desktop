# Vendored dependencies

These files are upstream release bytes, not locally patched implementations. CMake builds only the library translation units; SQLite's original `shell.c` is retained for archive completeness but is not built. No download occurs during configuration or compilation.

## SQLite 3.54.1

- Official release listing: https://sqlite.org/download.html
- Source archive: https://sqlite.org/2026/sqlite-amalgamation-3540100.zip
- Archive SHA3-256 (published by SQLite and independently computed on download): `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`
- Archive SHA256 (computed on download): `0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9`
- All four files in `sqlite/` are copied unchanged from `sqlite-amalgamation-3540100/` in that archive.
- License: public domain; upstream copyright disclaimer and blessing are preserved in the source headers. Official explanation: https://sqlite.org/copyright.html
- Upstream amalgamation identifies version 3.54.1 and Fossil check-in `555e07ef33f69a0957d24d8e555b702464a1`.

| File | SHA256 |
| --- | --- |
| `sqlite/sqlite3.c` | `0c657666d819f261cc4007526ad57e1669de9949df7a8ea847aa284a2bf0ee52` |
| `sqlite/sqlite3.h` | `d6c84557261893439b341d2e6c35187ae3ea24fbfd843ad94616f72d89242b8c` |
| `sqlite/sqlite3ext.h` | `48963abfefb41ac329ef9d706acb5eadba952a99a748285c9ded6b5e0942721b` |
| `sqlite/shell.c` | `00c5cba13701f8c02771f147190aca4faacdf6f6b0def48246fc6c2af656e9ee` |

`aether_sqlite3` is STATIC, exports the `sqlite/` system include directory, and privately defines `SQLITE_THREADSAFE=1`, `SQLITE_OMIT_LOAD_EXTENSION`, and `SQLITE_DEFAULT_MEMSTATUS=0`. Unix builds link `Threads::Threads`. Extension loading and optional SQL math functions are not enabled, so neither dl nor m is added. These options do not encrypt database, journal, WAL, or backup contents.

## yyjson 0.13.0

- Official release: https://github.com/ibireme/yyjson/releases/tag/0.13.0
- Official tagged source archive: https://codeload.github.com/ibireme/yyjson/zip/refs/tags/0.13.0
- Archive SHA256 (computed on download; not an upstream-published digest): `fe8e0136947cf0066794a602ffc25cb50fce0ec756b1458a8225ab12ae557f6b`
- `src/yyjson.c`, `src/yyjson.h`, and `LICENSE` are copied unchanged from `yyjson-0.13.0/`; the header reports `YYJSON_VERSION_STRING "0.13.0"`.
- License: MIT, copyright (c) 2020 YaoYuan. Full upstream notice is retained at `yyjson/LICENSE` and must accompany redistribution.

| File | SHA256 |
| --- | --- |
| `yyjson/yyjson.c` | `d2d58ef0a3b2267862dc363832c4f3185fd375aea933d5d3419d1a940d6ece2e` |
| `yyjson/yyjson.h` | `ef803cda5c06b8962face6dfa39c3284b3bcf3e73f4d7317664690cc779679ca` |
| `yyjson/LICENSE` | `45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947` |

`aether_yyjson` is STATIC, exports the `yyjson/` system include directory, and sets no compile definitions. Both third-party targets have private warning settings (`/W0` on MSVC, `-w` otherwise), separate from first-party strict warnings and without `/WX` or `-Werror`.

## Reproduction and upgrades

Download the exact archive URLs above, compute their stated archive digests, and reject mismatches before extracting. For SQLite, also compare SHA3-256 against the official release listing. Copy only the paths listed above without changing line endings; compare the individual SHA256 values to detect extraction or local modifications. Preserve license notices with every redistribution.

For upgrades, review official release notes and security advisories, select an explicit release rather than a floating branch, independently verify the available upstream checksums, update archive and source hashes together, and review changes to licensing and compiler options. Do not silently replace vendored bytes or use system-installed libraries as a fallback. Exercise credential CRUD, revision conflicts, malformed JSON handling, and persistence/reopen with synthetic data only after integration. Never use real credential databases for dependency validation.

Platform validation status: no configure, build, lint, formatter, or tests were run for this dependency import. Windows and Linux integration validation remains the integrating owner's responsibility; this document makes no platform-success claim.
