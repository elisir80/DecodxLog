# DecoDXLog performance baseline

This is a reproducible baseline for DXL-026. It measures the synthetic
dataset benchmark built into `tst_awards`; it is not a claim about every real
log or every platform.

## Environment

- Date: 2026-10-16
- Host: macOS, `Mac16,10`, arm64
- RAM: 16 GB
- CPUs: 10
- Build: Release CMake build, Qt 6.11.2
- Command: `DECODXLOG_BENCH_SYNTHETIC=N ./build/tst_awards benchSyntheticLog`
- RSS: `/usr/bin/time -l` around the same command; this memory measurement is
  host-specific and must be repeated with the native tool on other systems.

## Results

| QSO | Import | Date-range query | Compute | RSS |
|---:|---:|---:|---:|---:|
| 10,000 | 394 ms | 0 ms | 34 ms | included in prior RSS run |
| 100,000 | 4,079 ms | 6 ms | 398 ms | included in prior RSS run |
| 1,000,000 | 50,670 ms | 66 ms | 3,219 ms | 729,153,536 bytes (~695 MiB) in prior run |

The benchmark passed at all three sizes. The 1,000,000-row run was measured
again on 15 October 2026 with `/usr/bin/time -l`: 58.02 s wall time and
737,296,384 bytes maximum RSS. It reported 49,825 ms import, 61 ms
date-range query and 3,193 ms award computation. The 10,000 and 100,000 runs
reported respectively 389/1/35 ms and 4,011/6/391 ms for import/query/compute.
The benchmark now also times and validates a full date-range query. UI p50/p95
latency, cold startup, and cross-platform RSS remain unmeasured and require
additional instrumentation and runtime runs on each target platform. The
`tst_awards` case `editsAndDeletesMatchAFullRecompute` now verifies locally
that an edit plus soft-delete produces the same award summary as rebuilding
the database from the resulting live QSO set. Therefore DXL-026 remains
partially verified rather than complete.

## Latest verification — 16 October 2026

The same three sizes were rerun on the current checkout with:

```text
DECODXLOG_BENCH_SYNTHETIC=10000,100000,1000000 \
  /usr/bin/time -l ./build/tst_awards benchSyntheticLog
```

The test passed 3/3 cases. The current arm64 results were 388/1/34 ms,
4002/6/448 ms and 50114/62/3355 ms for import/query/compute respectively.
The one-million-row run took 58.52 s wall time and reached 728,449,024 bytes
maximum RSS. These are measurements of this macOS host, not cross-platform
acceptance thresholds; UI p50/p95 and cold-start latency remain unmeasured.
