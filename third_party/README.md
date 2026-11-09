# Vendored dependencies

Vendored implementations are unchanged upstream release bytes, not locally patched code. SQLite and yyjson build only their library translation units; SQLite's original `shell.c` is retained for archive completeness but is not built. curl's complete release is retained separately from its project-owned integration CMake file. No download occurs during configuration or compilation.

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

## curl 8.23.0

- Official release listing: https://curl.se/download.html (8.23.0, released 2026-10-14).
- Official source archive: https://curl.se/download/curl-8.23.0.tar.gz
- Official SHA256 source: https://curl.se/info; its downloaded, unchanged snapshot is retained as `curl/release-info.txt`. This endpoint tracks the latest release, so use the retained version-specific snapshot when reproducing this import.
- Archive SHA256, published in that official snapshot and independently computed before extraction: `e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49`.
- The exact downloaded archive is retained as `curl/curl-8.23.0.tar.gz`; the entire `curl-8.23.0/` archive tree is extracted unchanged into `curl/curl-8.23.0/`. No upstream source, implementation detail, CMake module, test fixture, generated release file, or license notice has been pruned or patched.
- Upstream `include/curl/curlver.h` identifies `LIBCURL_VERSION "8.23.0"` and `LIBCURL_VERSION_NUM 0x081700`; the official metadata identifies git tag `curl-8_23_0`.
- License: the **curl license**, SPDX `curl`, not the standard MIT license text despite the shorthand `License: MIT` in the official metadata. The complete copyright and permission notice is retained at `curl/curl-8.23.0/COPYING`; upstream per-file notices are also retained. See https://curl.se/docs/copyright.html. Redistribution must preserve the notices.
- Verification performed for this import: HTTPS retrieval, official SHA256 comparison before extraction, and source version/license inspection. OpenPGP signature verification and independent reproducible-release verification were **not** performed; SHA256 agreement is not a claim of signer authentication. Official stronger verification guidance: https://curl.se/docs/verify.html.

Selected independently computed source identity digests (the archive digest covers the complete release):

| File under `curl/curl-8.23.0/` | SHA256 |
| --- | --- |
| `COPYING` | `82f2f4427d6545ee5aaac4f0b80428da6cc8ba41c2cf5da3a03680ec327b9681` |
| `include/curl/curlver.h` | `fed299993c517a6a1785f44b19893d4ae65061a7d7b5ca43036b35bf998065b4` |
| `CMakeLists.txt` | `90425e9ff3654938d1780703df0ecd9b6d48db8b01f206f5ce3aff66bbcffb38` |
| `lib/CMakeLists.txt` | `8fa2d3c63eea16e1bf7f759c200b2134f18e7cc89b264089ceebc9901ea0250b` |

`curl/CMakeLists.txt` is the project-owned configuration; everything below `curl/curl-8.23.0/`, including long files and upstream test data/release-generated artifacts, is third-party material, **not** hand-written project code. It is retained for provenance, not added to the application's source/test discovery. CMake output belongs in the selected binary directory, never this vendor tree.

On Windows, `aether_curl` aliases upstream's STATIC `libcurl_static` target. Link it with `target_link_libraries(... PRIVATE aether_curl)` (or PUBLIC when exposed by the consumer's public contract). Upstream exports the include directory for `<curl/curl.h>`, transitive `CURL_STATICLIB`, and required Windows system libraries (`bcrypt`, `advapi32`, `crypt32`, `secur32`, `ws2_32`, `iphlpapi`); consumers must not reconstruct these requirements manually. Its public headers are system includes, and only the curl target gets the existing private vendor warning suppression. No global C standard, compiler flags, warning level, or first-party warning-as-error policy is changed.

This configuration enables only HTTP/HTTPS with Windows Schannel and native certificate trust, plus curl's threaded DNS resolver using Windows native threads. All other transfer protocols, WebSockets, IPFS/IPNS, and DNS-over-HTTPS are disabled. It disables the curl executable, examples, upstream tests, manuals, installation/export targets, developer instrumentation, and optional dependency discovery. No OpenSSL/other external TLS backend, c-ares, zlib, brotli, zstd, nghttp2/ngtcp2/nghttp3, quiche, libpsl, libidn2, SSH, LDAP, GSSAPI, GSASL, libuv, or libbacktrace is used. There is no system-libcurl fallback. Generic upstream options are scoped below `third_party/curl` so they do not disable application tests or change other dependencies' linkage.

Only Windows creates `aether_curl` in this integration. Other platforms explicitly report its unavailability rather than introducing an unapproved TLS dependency or claiming HTTPS support without TLS. Neither Windows integration nor any non-Windows platform was configured, built, or tested as part of this import. The integrating owner must run the network HTTP/error/timeout/cancellation/resource-cycle tests and local TLS trust tests after linking the target; no platform-success claim is made here.

Integration acceptance for ENG-07-NW subsequently passed on Windows: the network
CTest group passed 3/3, full CTest passed 46/46, and a direct loopback executable
run reported resource handles before=100 / after=100. Run Debug executables in
the environment provided by `scripts/msvc-env.bat`. These results do not validate
non-Windows builds or a standalone redistributable package. Full evidence and
the SDK static-analysis limitation are in `docs/tasks/EXECUTION_LOG.md`.

For curl upgrades, first review https://curl.se/docs/security.html and the release changelog for fixes and backend-specific changes. Pin the new release and official archive URL; save fresh official checksum evidence, reject digest mismatches before extraction, and preserve the original archive and full unmodified tree. Verify the upstream signature against an independently authenticated release-manager key when available, or explicitly record that this was not done. Update the version path, archive/source hashes, license review, and this narrow option list together: inspect new protocol/dependency defaults so an upgrade cannot silently enable extra protocols or libraries. Re-run Windows integration and TLS verification with synthetic local endpoints before release. Do not weaken certificate verification or revert to the failed WinHTTP backend to make an upgrade pass.

## Reproduction and upgrades

Download the exact archive URLs above, compute their stated archive digests, and reject mismatches before extracting. For SQLite, also compare SHA3-256 against the official release listing. For SQLite and yyjson, copy only their listed paths without changing line endings; for curl, retain the archive, checksum snapshot, and complete extracted release tree as described above. Compare the individual SHA256 values to detect extraction or local modifications. Preserve license notices with every redistribution.

For upgrades, review official release notes and security advisories, select an explicit release rather than a floating branch, independently verify the available upstream checksums, update archive and source hashes together, and review changes to licensing and compiler options. Do not silently replace vendored bytes or use system-installed libraries as a fallback. Exercise credential CRUD, revision conflicts, malformed JSON handling, and persistence/reopen with synthetic data only after integration. Never use real credential databases for dependency validation.

Platform validation status: no configure, build, lint, formatter, or tests were run for this dependency import. Windows and Linux integration validation remains the integrating owner's responsibility; this document makes no platform-success claim.
