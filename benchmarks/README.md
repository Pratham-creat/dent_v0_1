# Benchmark harness

Use local Netlib LP, MIPLIB MPS, or Maros–Mészáros QP instances.

highs_compare.py runs the same LP/MPS instance through DENT and a locally installed HiGHS executable and records wall time, exit status, stdout and stderr.

Benchmark results are generated on the user's machine; the repository does not hard-code performance claims.
