# BBIME Local Modifications

Upstream: libgooglepinyin 0.1.2. License and copyright are unchanged.
Modification date: 2026-10-01.

- `src/matrixsearch.cpp`: a dictionary extension without a valid system or user
  handle returns no branch. It cannot assert on bounded pool exhaustion or use
  cached candidates without an associated dictionary state.
- `src/dicttrie.cpp`: extensions check milestone and parsing-mark capacity before
  allocation. A partially allocated extension rolls back its parsing marks and
  returns no handle/candidates instead of publishing an invalid count.
- `src/userdict.cpp`: the previously started sync-export fix measures the UTF-16
  temporary buffer end in elements, not bytes, and checks conversion results.
  This entry point remains unexposed and is not a validated sharing protocol.

Assertions remain enabled. No special-case input blacklist is used.
Regression tests cover both reproduced failure strings, their prefixes, nearby
inputs, recovery to normal queries, and deterministic randomized queries.
This is not a complete audit of upstream synchronization/export or persistence.
