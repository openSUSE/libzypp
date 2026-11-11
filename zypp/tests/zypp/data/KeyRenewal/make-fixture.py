#!/usr/bin/env python3
"""Regenerate the key renewal test fixture (bsc#1284494, bsc#1289557).

Needs gpg (with gpgsplit), rpmsign, rpmkeys and createrepo_c. Run by hand, never during the build.

    ./make-fixture.py [OUTDIR]                  (default: the directory of this script)
    ./make-fixture.py --sub-chain-only [OUTDIR]

The files are described in readme.md. All keys are TEST KEYS without passphrase.

--sub-chain-only makes only sub-r1, sub-r2, sub-r2-badbinding and sub-foreign, from the committed
sub-private.asc. All other files are left alone.
"""
import argparse
import datetime
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent


class Gpg:
    """gpg with its own home directory."""

    def __init__(self, home):
        self.home = str(home)
        self.env = dict(os.environ, GNUPGHOME=self.home)

    def run(self, *args, input=None, cwd=None):
        """Run a command in this home; return its stdout. Unprotected keys: no passphrase."""
        return subprocess.run(args, input=input, cwd=cwd, env=self.env, check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout

    def gpg(self, *args, input=None, faked_time=None):
        time = ["--faked-system-time", faked_time] if faked_time else []
        return self.run("gpg", "--batch", "--quiet", "--passphrase", "", *time, *args, input=input)

    def fingerprints(self, *select):
        out = self.gpg("--list-keys", "--with-colons", *select).decode()
        return [line.split(":")[9] for line in out.splitlines() if line.startswith("fpr:")]

    def export(self, fpr, path, faked_time=None):
        """Export the key as rpm would import it. gpg leaves out subkeys expired at that time."""
        path.write_bytes(self.gpg("--armor", "--export-options", "export-minimal", "--export", fpr,
                                  faked_time=faked_time))

    def export_secret(self, fpr, path):
        path.write_bytes(self.gpg("--pinentry-mode", "loopback", "--armor", "--export-secret-keys", fpr))

    def kill_agent(self):
        subprocess.run(["gpgconf", "--kill", "all"], env=self.env, check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


class ScratchGpg:
    """A Gpg in a temporary home; its agent is stopped at the end."""

    def __enter__(self):
        self._dir = tempfile.TemporaryDirectory()
        self.gpg = Gpg(self._dir.name)
        return self.gpg

    def __exit__(self, *exc):
        self.gpg.kill_agent()
        self._dir.cleanup()


def key_info(path):
    """(fingerprint, subkey expiry dates as YYYY-MM-DD) of a key file."""
    with ScratchGpg() as g:
        out = g.gpg("--show-keys", "--with-colons", str(path)).decode()
    fpr = next(line.split(":")[9] for line in out.splitlines() if line.startswith("fpr:"))
    subs = [line.split(":")[6] for line in out.splitlines() if line.startswith("sub:")]
    return fpr, [datetime.datetime.fromtimestamp(int(s), datetime.timezone.utc).strftime("%Y-%m-%d") if s else "never"
                 for s in subs]


def packets(path, workdir):
    """Split the key file into one file per OpenPGP packet (gpgsplit), in packet order."""
    workdir.mkdir()
    with ScratchGpg() as g:
        binary = g.gpg("--dearmor", input=path.read_bytes())
        g.run("gpgsplit", "--prefix", "p-", input=binary, cwd=workdir)
    return sorted(workdir.iterdir())


def write_key(packet_files, path):
    """Join packet files to an armored public key file."""
    with ScratchGpg() as g:
        armored = g.gpg("--enarmor", input=b"".join(p.read_bytes() for p in packet_files)).decode()
    armored = armored.replace("PGP ARMORED FILE", "PGP PUBLIC KEY BLOCK")
    path.write_text("\n".join(line for line in armored.split("\n") if not line.startswith("Comment:")))


def rpm_verdict(keyfile, package):
    """What rpmkeys says about the package if the rpm database only knows the key."""
    with tempfile.TemporaryDirectory() as db:
        subprocess.run(["rpm", "--dbpath", db, "--initdb"], check=True)
        subprocess.run(["rpm", "--dbpath", db, "--import", str(keyfile)], check=True)
        return subprocess.run(["rpmkeys", "--dbpath", db, "-Kv", str(package)], text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout


def make_repo(g, repo, srcrpm, signer):
    """A signed rpm-md repo with one package, both signed by signer. No repomd.xml.key:
    the tests add the key version they need."""
    shutil.rmtree(repo, ignore_errors=True)
    (repo / "x86_64").mkdir(parents=True)
    shutil.copy(srcrpm, repo / "x86_64/pkg.rpm")
    g.run("rpmsign", "--addsign", "--define", f"_gpg_name {signer}", "--define", f"_gpg_path {g.home}",
          str(repo / "x86_64/pkg.rpm"))
    g.run("createrepo_c", "--no-database", "--simple-md-filenames", "--general-compress-type=gz", str(repo))
    g.gpg("--yes", "--armor", "--detach-sign", "--local-user", signer,
          "-o", str(repo / "repodata/repomd.xml.asc"), str(repo / "repodata/repomd.xml"))


def make_keys(out, srcrpm):
    """The key in three versions with repo/, and the subkey key with repo-sub/."""
    with ScratchGpg() as g:
        # the key, in three versions
        g.gpg("--quick-gen-key", "zypp key renewal testcase <zypp-devel@opensuse.org>", "rsa3072", "sign",
              "2025-06-01", faked_time="20250101T000000")
        fpr = g.fingerprints()[0]
        g.export(fpr, out / "key-v0.asc")
        g.gpg("--quick-set-expire", fpr, "2099-12-31", faked_time="20250301T000000")
        g.export(fpr, out / "key-v1.asc")
        g.gpg("--quick-set-expire", fpr, "2099-12-31", faked_time="20250501T000000")
        g.export(fpr, out / "key-v2.asc")
        g.export_secret(fpr, out / "key-private.asc")
        make_repo(g, out / "repo", srcrpm, fpr)

        # a second key: the primary key never expires, the signing subkey does
        uid = "zypp subkey renewal testcase <zypp-devel@opensuse.org>"
        g.gpg("--quick-gen-key", uid, "rsa3072", "cert", "never", faked_time="20250101T000000")
        sfpr = g.fingerprints(uid)[0]
        g.gpg("--quick-add-key", sfpr, "rsa3072", "sign", "2025-06-01", faked_time="20250101T000000")
        ssub = g.fingerprints(uid)[1]
        g.export(sfpr, out / "sub-v0.asc", faked_time="20250301T000000")
        g.gpg("--quick-set-expire", sfpr, "2099-12-31", ssub, faked_time="20250501T000000")
        g.export(sfpr, out / "sub-v1.asc")
        g.export_secret(sfpr, out / "sub-private.asc")
        make_repo(g, out / "repo-sub", srcrpm, ssub + "!")
    return fpr, sfpr


def make_sub_chain(out):
    """From sub-private.asc: the subkey renewed twice, and two broken variants."""
    with ScratchGpg() as g, tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        g.gpg("--import", str(out / "sub-private.asc"))
        fpr, sub = g.fingerprints()[:2]
        g.gpg("--quick-set-expire", fpr, "2025-10-01", sub, faked_time="20250601T000000")
        g.export(fpr, out / "sub-r1.asc", faked_time="20250701T000000")
        g.gpg("--quick-set-expire", fpr, "2099-12-31", sub, faked_time="20250901T000000")
        g.export(fpr, out / "sub-r2.asc", faked_time="20250915T000000")

        # invalid binding signature: the last packet of a key with one subkey is its binding signature
        parts = packets(out / "sub-r2.asc", tmp / "r2")
        assert parts[-2].suffix == ".public_subkey" and parts[-1].suffix == ".sig", parts
        binding = bytearray(parts[-1].read_bytes())
        binding[-1] ^= 0xFF          # still well-formed, but the signature does not verify
        parts[-1].write_bytes(binding)
        write_key(parts, out / "sub-r2-badbinding.asc")

        # sub-v1's primary key and user id, with a subkey (and its binding signature) of an unrelated key
        with ScratchGpg() as donor:
            donor.gpg("--quick-gen-key", "zypp foreign subkey donor <zypp-devel@opensuse.org>", "rsa3072",
                      "cert", "never", faked_time="20250101T000000")
            dfpr = donor.fingerprints()[0]
            donor.gpg("--quick-add-key", dfpr, "rsa3072", "sign", "never", faked_time="20250101T000000")
            donor.export(dfpr, tmp / "donor.asc", faked_time="20250301T000000")
        base = packets(out / "sub-v1.asc", tmp / "v1")
        foreign = packets(tmp / "donor.asc", tmp / "donor")
        first_sub = next(n for n, p in enumerate(base) if p.suffix == ".public_subkey")
        donor_sub = next(n for n, p in enumerate(foreign) if p.suffix == ".public_subkey")
        write_key(base[:first_sub] + foreign[donor_sub:donor_sub + 2], out / "sub-foreign.asc")

    for name in ("sub-r1", "sub-r2", "sub-r2-badbinding", "sub-foreign"):
        if key_info(out / f"{name}.asc")[0] != fpr:
            sys.exit(f"{name}.asc: wrong fingerprint")
    for name in ("sub-r1", "sub-r2"):
        print(f"{name}.asc: subkey expires {key_info(out / f'{name}.asc')[1][0]}")


def check(out, fpr, sfpr):
    for v in ("v0", "v1", "v2"):
        if key_info(out / f"key-{v}.asc")[0] != fpr:
            sys.exit(f"key-{v}.asc: wrong fingerprint")
    for v in ("v0", "v1"):
        if key_info(out / f"sub-{v}.asc")[0] != sfpr:
            sys.exit(f"sub-{v}.asc: wrong fingerprint")
    if not re.search(r"Signature, key ID .*: OK", rpm_verdict(out / "key-v2.asc", out / "repo/x86_64/pkg.rpm")):
        sys.exit("package signature does not verify")
    # the package signed by the renewed subkey verifies with the renewed key
    if not re.search(r"Signature, key ID .*: OK", rpm_verdict(out / "sub-v1.asc", out / "repo-sub/x86_64/pkg.rpm")):
        sys.exit("subkey package signature does not verify")
    # with the old key (expired subkey) rpm refuses it; the tests rely on that, but it depends on the rpm in use
    if "NOTTRUSTED" not in rpm_verdict(out / "sub-v0.asc", out / "repo-sub/x86_64/pkg.rpm"):
        print("note: this rpm does not refuse a signature of an expired subkey")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("outdir", nargs="?", default=HERE, type=pathlib.Path)
    parser.add_argument("--sub-chain-only", action="store_true")
    parser.add_argument("--srcrpm", type=pathlib.Path, default=HERE / "../RpmPkgSigCheck/unsigned.rpm",
                        help="the unsigned package to sign")
    args = parser.parse_args()
    out = args.outdir.resolve()
    out.mkdir(parents=True, exist_ok=True)

    if args.sub_chain_only:
        make_sub_chain(out)
    else:
        fpr, sfpr = make_keys(out, args.srcrpm.resolve())
        make_sub_chain(out)
        check(out, fpr, sfpr)
    print(f"done: {out}")


if __name__ == "__main__":
    main()
