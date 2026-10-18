# Positions Roadmap

Positions is a set of Apache httpd modules written in C that serve a geographical hierarchy
(country → region → province → municipality → postal code) as a lightweight HTTP API.
The hierarchy is stored as a directory tree on disk, and postal codes are symbolic links
into that tree.

Today it ships three endpoints and a stub dataset for Italy:

| Endpoint | Module | Returns |
|---|---|---|
| `GET /positions/{path}` | `mod_positions` | JSON array of the direct children of a node |
| `GET /tree/{path}` | `mod_tree` | JSON object with the whole subtree below a node |
| `GET /italiancaps/{cap}` | `mod_italiancaps` | Plain-text path of the municipality with that CAP |

This roadmap has two goals: **cover more countries than Italy** and make the service
**as fast as possible**. Both point to the same architectural change, described in Phase 3:
compile the dataset once at build time into a compact, read-only index, and let each request
be a lookup plus a write of pre-serialized bytes.

Sizes are rough effort estimates: **S** ≈ 1–3 days, **M** ≈ 1–2 weeks, **L** ≈ 3+ weeks.

---

## Phase 0 — Fixes and housekeeping

| Item | Size | Notes |
|---|---|---|
| Fix `test.sh` checks | S | `[[ trento == "null" ]]` compares the literal string `trento`, not the variable, so the check never runs; the condition is also inverted (it should fail when Trento *is* found). |
| Fix the `raravenna` path | S | The sample data and tests use `raravenna` instead of `ravenna`. |
| Escape JSON strings in `mod_positions` | S | Names are written with `"%s"` and no escaping: a name containing `"` or `\` produces invalid JSON. Non-ASCII names (e.g. `Forlì`) must be valid UTF-8. |
| Request `APR_FINFO_TYPE` in `mod_positions` | S | `apr_dir_read` is called with `APR_FINFO_NAME` only. On filesystems that return `DT_UNKNOWN`, `filetype` is not set and every entry is silently skipped. |
| Single source for paths | S | `BASE_PATH` is used both as URL prefix and filesystem root, while `mod_tree` and `mod_italiancaps` hardcode `"/tree"`. Replace `config.h` with Apache directives (`PositionsDataRoot`, `PositionsPostalRoot`) read at startup. |
| Consistent response format | S | `mod_italiancaps` returns `text/plain`; all endpoints should return JSON, including errors (`{"error": "not_found"}`). |
| Validate the postal code format | S | Reject anything that does not match the country's pattern before touching the filesystem. |
| Dockerfile cleanup | S | `EXPOSE` takes only the container port (mapping happens at `docker run`); the comment mentions the official Apache image but the base is `debian:bookworm`; `vim` and build tools end up in the runtime image. Use a multi-stage build. |
| Remove `.DS_Store` from the repository | S | Add it to `.gitignore`. |
| Expand the README | S | Endpoints, examples, data sources, licensing of the data. |

## Phase 1 — Data model and API that work beyond Italy

The current model assumes Italian administrative levels and a 1:1 mapping between CAP and
municipality. Neither holds internationally, and the second does not fully hold in Italy
either: many CAPs cover several municipalities or hamlets (*frazioni*), and large cities
have many CAPs.

| Item | Size | Notes |
|---|---|---|
| ISO country codes in URLs | S | `/v1/IT/...` instead of `/italy/...` (ISO 3166-1 alpha-2). |
| Stable identifiers per node | M | ISO 3166-2 for first-level subdivisions, national codes below (e.g. ISTAT codes for Italian municipalities), GeoNames IDs as a cross-country fallback. Slugs stay for readability, IDs for integration. |
| Variable depth per country | M | Each country declares its levels (e.g. IT: region/province/municipality; DE: Land/Kreis/Gemeinde; FR: région/département/commune; US: state/county/place). Clients read the level names from a `/v1/{country}` metadata endpoint. |
| Many-to-many postal codes | M | A postal code returns a **list** of places, and a place exposes its postal codes. Symlinks cannot represent this, which is one more reason for the index in Phase 3. |
| Localized names | M | Multiple official names (Bolzano/Bozen, Aosta/Aoste, Brussels/Bruxelles/Brussel) and `Accept-Language` support. |
| Generic postal-code endpoint | S | `/v1/{country}/postalcodes/{code}` replaces `/italiancaps`. Keep `/italiancaps` as a deprecated alias for one release. |
| Coordinates | M | A centroid (lat/lon) per node, so the project lives up to its name. |
| Prefix search / autocomplete | M | `/v1/{country}/search?q=rav` for address forms. Implemented as a sorted array or trie in the index. |
| OpenAPI specification, CORS, versioning | S | Publish an `openapi.yaml`; add `Access-Control-Allow-Origin` for browser clients. |

## Phase 2 — Data pipeline and new countries

A reproducible import pipeline that turns open datasets into the project's own format.
**Check each source's license before redistributing**: the code is GPL v3, but data has
its own terms, and some licenses (e.g. ODbL for OpenStreetMap) impose share-alike
obligations on derived databases.

| Item | Size | Notes |
|---|---|---|
| Import pipeline | M | A small tool (Python or C) that downloads sources, normalizes them into a single intermediate format (CSV or JSON Lines: id, parent_id, level, names, postal codes, centroid) and validates it. Versioned output, checksums. |
| Italy, complete | M | Municipalities and hierarchy from ISTAT; CAPs from an openly licensed source; handling of municipality mergers that happen every year. |
| Global baseline from GeoNames | M | Admin divisions and the postal-code dump give a first, uniform coverage of many countries. Quality varies by country. |
| Official national sources for priority countries | L | For example France (La Poste / data.gouv.fr), Germany, Spain, United Kingdom (Code-Point Open), United States (Census ZCTA). One adapter per country, added incrementally. |
| EU cross-reference with Eurostat NUTS/LAU | M | Useful for European statistical regions and to cross-check municipality lists. |
| Scheduled refresh | S | A GitHub Action that rebuilds the dataset monthly and opens a PR with a diff summary (added, removed, renamed places). |
| Data quality report | S | Counts per level, orphans, duplicate names, postal codes without a place. |

## Phase 3 — Performance

The hot path today: for every request the module logs at `NOTICE`, opens directories,
reads entries through system calls, builds JSON (for `/tree`, recursively and with
`json-c` pretty-printing), and writes it. For `/tree` on a whole country this means
thousands of `opendir`/`readdir` calls per request.

Steps are in order of value: each one is measurable with the benchmark of step 1.

| # | Item | Size | Expected effect |
|---|---|---|---|
| 1 | **Benchmark harness** | S | `wrk` or `h2load` scripts with a fixed request mix, run in CI on every PR; report requests/s and p50/p99 latency. Set a target budget and fail the build on regressions. |
| 2 | **Remove per-request logging** | S | Every request (and every directory in `/tree`) writes a `NOTICE` line to the error log. Move these to `APLOG_TRACE1` and guard them with `APLOG_R_IS_LEVEL`. Probably the biggest single gain for the least effort. |
| 3 | **Compact JSON** | S | Drop `JSON_C_TO_STRING_PRETTY`; it inflates `/tree` responses considerably. |
| 4 | **HTTP caching** | S | Data changes at most monthly: strong `ETag` (dataset version), `Cache-Control: public, max-age=…, immutable` on versioned URLs, `304` responses. |
| 5 | **Depth limit on `/tree`** | S | `?depth=N` with a default and a maximum, so one request cannot walk an entire continent. This is also a denial-of-service fix. |
| 6 | **Precompiled read-only index** | L | The pipeline writes one binary file per dataset version: a node table, a hash table or perfect hash from path/ID/postal code to node, and **pre-serialized JSON** for each node's children (optionally pre-compressed with gzip and Brotli). The module `mmap`s it in `post_config`. The page cache shares it across all Apache child processes, and a request becomes one lookup and one write, with no system calls besides the socket. Dataset updates are atomic: write the new file, swap a symlink, graceful restart. |
| 7 | **Zero-copy responses** | M | With the index in place, send the pre-serialized byte range directly (`apr_bucket_mmap`) and skip building a brigade with `printf`, as well as the second pass `mod_positions` makes today to compute `Content-Length`. |
| 8 | **Pre-compressed variants** | S | Serve the stored Brotli/gzip bytes according to `Accept-Encoding` instead of compressing on every request with `mod_deflate`. |
| 9 | **Server tuning** | S | MPM `event`, keep-alive, `mod_http2`; disable unused modules and `AllowOverride` (no `.htaccess` lookups). Ship a tuned `httpd.conf`. |
| 10 | **Static export and CDN** | M | Because responses are pure functions of the dataset, the same pipeline can export every response as a static file and publish it to S3 + CloudFront: near-zero server latency at the edge, with the Apache modules kept for search and for self-hosting. |

Optional, only if measurements show the need:
- **Profile-guided optimization and LTO** (S) for the module build.
- **Remove `json-c`** (S): with pre-serialized data, the runtime no longer needs a JSON library.

## Phase 4 — Quality and security

The modules run inside the web server process, so a memory error in them takes down a
worker or worse.

| Item | Size | Notes |
|---|---|---|
| Unit tests in C | M | Test path parsing, validation and JSON escaping outside Apache (e.g. with `cmocka`), by moving the logic into plain functions. |
| Integration tests | S | Turn `test.sh` into a proper suite (e.g. `bats` or Python + `pytest`) that runs against the container in CI. |
| Sanitizer builds | S | Run the integration suite with an AddressSanitizer/UBSan build of the modules. |
| Fuzzing | M | `libFuzzer` targets for URL parsing and index lookup. |
| Static analysis | S | `clang-tidy`, `cppcheck`, compiler warnings as errors (`-Wall -Wextra -Werror`). |
| CI with GitHub Actions | S | Build, tests, sanitizers, benchmark, container image published to GHCR. |
| Hardened container | S | Non-root user, read-only filesystem, minimal runtime image. |
| Rate limiting | S | Document `mod_ratelimit` / `mod_evasive` or a reverse proxy in front for public deployments. |

## Phase 5 — Developer experience

| Item | Size |
|---|---|
| Example front end: cascading country/region/province/municipality selectors and postal-code autofill | M |
| Client snippets (JavaScript, Java) and a Postman/Bruno collection generated from OpenAPI | S |
| Deployment guides: Docker Compose, Kubernetes, AWS (EC2 or ECS behind CloudFront) | M |
| `CONTRIBUTING.md` with a guide on adding a new country adapter | S |

---

## Proposed milestones

| Release | Content |
|---|---|
| **0.2.0** | Phase 0 fixes, consistent JSON, configurable paths, CI with existing tests |
| **0.3.0** | Benchmark harness, logging fix, compact JSON, caching headers, `/tree` depth limit |
| **0.4.0** | `/v1` API with ISO codes, IDs, many-to-many postal codes; complete Italian dataset |
| **0.5.0** | Precompiled `mmap` index with pre-serialized and pre-compressed responses |
| **0.6.0** | GeoNames baseline for all countries, first official national adapters, search endpoint |
| **1.0.0** | API freeze, OpenAPI, security hardening, fuzzing, static export/CDN option |
