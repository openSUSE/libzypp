# KeyRing test data

The passphrase for the key pair is `zypp-devel`.

## Renewed keys (bsc#1284494)

Made by `./make-fixture.py` (needs gpg). It writes only the `renew-*.asc` files and leaves the
other files alone.

| File | Content |
|---|---|
| `renew-old.asc` | newest self-signature 2025-01-01, expires 2025-06-01 (rpm release `67748580`) |
| `renew-new.asc` | the same key, renewed: self-signature 2025-05-01, expires 2027-01-01 (release `6812b980`) |
| `renew-new-plus-other.asc` | `renew-new.asc` followed by an unrelated key |
| `renew-sub-old.asc` | the primary key never expires, the signing subkey expires 2025-06-01 |
| `renew-sub-new.asc` | the same key with the subkey renewed (expires 2099-12-31) |

In `renew-sub-*` only the subkey binding signature differs, so `created()` is the same in both.

### Different subkey sets

One key. Subkey A expires 2025-06-01 and is renewed (expires 2099-12-31). Subkey B (expires
2026-01-01) is added later.

| File | Subkeys |
|---|---|
| `renew-sub2-a.asc` | A |
| `renew-sub2-ab.asc` | A and B |
| `renew-sub2-ab-ext.asc` | A renewed, and B |
| `renew-sub2-a-ext.asc` | A renewed, B never added |
