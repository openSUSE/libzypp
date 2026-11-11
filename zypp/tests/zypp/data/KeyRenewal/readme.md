# Key renewal test data

Used by the key renewal tests (bsc#1284494, bsc#1289557). Regenerate with `./make-fixture.py`
(needs gpg, rpmsign, rpmkeys, createrepo_c). Not needed to build libzypp.

All keys are **test keys without passphrase**. Never use them for anything else.

## Key with a renewed primary key

Three versions of one key, with the same fingerprint.

| File | Content |
|---|---|
| `key-v0.asc` | newest self-signature 2025-01-01, expires 2025-06-01 |
| `key-v1.asc` | newest self-signature 2025-03-01, expires 2099-12-31 |
| `key-v2.asc` | newest self-signature 2025-05-01, expires 2099-12-31 (the renewed key) |
| `key-private.asc` | the secret key |
| `repo/` | signed rpm-md repo with one package (`pkg-test42`), signed with this key |

## Key with a renewed signing subkey

The primary key never expires, the signing subkey does. Only the subkey binding signature changes,
so the user id self-signature (and zypp's creation date of the key) is the same in all versions.

| File | Content |
|---|---|
| `sub-v0.asc` | the subkey expires 2025-06-01 (exported while it was still valid) |
| `sub-v1.asc` | the subkey was renewed, expires 2099-12-31 |
| `sub-private.asc` | the secret key |
| `repo-sub/` | like `repo/`, but package and metadata are signed by the subkey |

### Renewed twice, and broken variants

Made from `sub-private.asc` by `./make-fixture.py --sub-chain-only` (needs `gpgsplit`, part of
gpg), which leaves all other files alone.

| File | Content |
|---|---|
| `sub-r1.asc` | the subkey renewed, expires 2025-10-01 (binding signature 2025-06-01) |
| `sub-r2.asc` | renewed again, expires 2099-12-31 (binding signature 2025-09-01) |
| `sub-r2-badbinding.asc` | `sub-r2.asc` with an invalid subkey binding signature |
| `sub-foreign.asc` | the key of `sub-v1.asc` with a subkey (and binding) of an unrelated key |

The binding signatures are newer than the user id self-signature, so rpm names these keys by a newer
release than zypp computes.

## Notes

- The repos have no `repodata/repomd.xml.key`. The tests copy a repo and add the key version they need.
