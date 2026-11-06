#!/usr/bin/env python3
"""Regenerate the renew-*.asc keys of the KeyRing tests (bsc#1284494). Needs gpg.

    ./make-fixture.py [OUTDIR]          (default: the directory of this script)

Writes only the renew-*.asc files described in readme.md; the other files here are left alone.
The keys are generated anew: their fingerprints change, the dates (and thus the rpm releases) don't.
"""
import argparse
import datetime
import os
import pathlib
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

    def gpg(self, *args, faked_time=None):
        time = ["--faked-system-time", faked_time] if faked_time else []
        return subprocess.run(["gpg", "--batch", "--quiet", "--passphrase", "", *time, *args], env=self.env,
                              check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout

    def fingerprints(self, *select):
        out = self.gpg("--list-keys", "--with-colons", *select).decode()
        return [line.split(":")[9] for line in out.splitlines() if line.startswith("fpr:")]

    def export(self, fpr, faked_time=None):
        """The key as rpm would import it. gpg leaves out subkeys expired at that time."""
        return self.gpg("--armor", "--export-options", "export-minimal", "--export", fpr, faked_time=faked_time)

    def kill_agent(self):
        subprocess.run(["gpgconf", "--kill", "all"], env=self.env, check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


class ScratchGpg:
    """A Gpg in a temporary home (optionally a copy of another one); its agent is stopped at the end."""

    def __init__(self, copy_of=None):
        self._copy_of = copy_of

    def __enter__(self):
        self._dir = tempfile.TemporaryDirectory()
        home = pathlib.Path(self._dir.name) / "home"
        if self._copy_of:
            self._copy_of.kill_agent()
            shutil.copytree(self._copy_of.home, home, ignore=shutil.ignore_patterns("S.*"))
        else:
            home.mkdir(mode=0o700)
        self.gpg = Gpg(home)
        return self.gpg

    def __exit__(self, *exc):
        self.gpg.kill_agent()
        self._dir.cleanup()


def key_info(path):
    """(fingerprints, subkey expiry dates as YYYY-MM-DD) of a key file."""
    with ScratchGpg() as g:
        lines = g.gpg("--show-keys", "--with-colons", str(path)).decode().splitlines()
    fprs = [line.split(":")[9] for line in lines if line.startswith("fpr:")]
    subs = [line.split(":")[6] for line in lines if line.startswith("sub:")]
    return fprs, [datetime.datetime.fromtimestamp(int(s), datetime.timezone.utc).strftime("%Y-%m-%d")
                  if s else "never" for s in subs]


def make_renew(out):
    """renew-old/new: one key, renewed; renew-new-plus-other: renew-new followed by an unrelated key."""
    with ScratchGpg() as g:
        g.gpg("--quick-gen-key", "zypp renew test <renew@zypp.test>", "rsa2048", "sign", "2025-06-01",
              faked_time="20250101T000000")
        fpr = g.fingerprints()[0]
        (out / "renew-old.asc").write_bytes(g.export(fpr))
        g.gpg("--quick-set-expire", fpr, "2027-01-01", faked_time="20250501T000000")
        new = g.export(fpr)
        (out / "renew-new.asc").write_bytes(new)
        g.gpg("--quick-gen-key", "zypp other test <other@zypp.test>", "rsa2048", "sign", "never")
        other = g.fingerprints()[-1]
        (out / "renew-new-plus-other.asc").write_bytes(new + g.export(other))
    return fpr


def make_renew_sub(out):
    """renew-sub-old/new: the primary key never expires, the signing subkey expired and was renewed."""
    with ScratchGpg() as g:
        g.gpg("--quick-gen-key", "zypp subkey renewal testcase <zypp-devel@opensuse.org>", "rsa3072", "cert",
              "never", faked_time="20250101T000000")
        fpr = g.fingerprints()[0]
        g.gpg("--quick-add-key", fpr, "rsa3072", "sign", "2025-06-01", faked_time="20250101T000000")
        sub = g.fingerprints()[1]
        (out / "renew-sub-old.asc").write_bytes(g.export(fpr, faked_time="20250301T000000"))
        g.gpg("--quick-set-expire", fpr, "2099-12-31", sub, faked_time="20250501T000000")
        (out / "renew-sub-new.asc").write_bytes(g.export(fpr))
    return fpr


def make_renew_sub2(out):
    """renew-sub2-*: one key with different subkey sets. Subkey A is renewed, subkey B is added later."""
    with ScratchGpg() as g:
        g.gpg("--quick-gen-key", "zypp subkey count testcase <zypp-devel@opensuse.org>", "rsa3072", "cert",
              "never", faked_time="20250101T000000")
        fpr = g.fingerprints()[0]
        g.gpg("--quick-add-key", fpr, "rsa3072", "sign", "2025-06-01", faked_time="20250101T000000")
        sub_a = g.fingerprints()[1]
        (out / "renew-sub2-a.asc").write_bytes(g.export(fpr, faked_time="20250301T000000"))

        with ScratchGpg(copy_of=g) as without_b:       # this branch never gets B
            without_b.gpg("--quick-set-expire", fpr, "2099-12-31", sub_a, faked_time="20250501T000000")
            (out / "renew-sub2-a-ext.asc").write_bytes(without_b.export(fpr, faked_time="20250515T000000"))

        g.gpg("--quick-add-key", fpr, "rsa3072", "sign", "2026-01-01", faked_time="20250201T000000")
        (out / "renew-sub2-ab.asc").write_bytes(g.export(fpr, faked_time="20250301T000000"))
        g.gpg("--quick-set-expire", fpr, "2099-12-31", sub_a, faked_time="20250501T000000")
        (out / "renew-sub2-ab-ext.asc").write_bytes(g.export(fpr, faked_time="20250515T000000"))
    return fpr


def check(out, name, fpr, subkey_expiries):
    fprs, expiries = key_info(out / f"{name}.asc")
    if fprs[0] != fpr or expiries != subkey_expiries:
        sys.exit(f"{name}.asc: unexpected key data {fprs[0]} {expiries}")
    print(f"{name}.asc: ok, subkeys expire {expiries or '-'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("outdir", nargs="?", default=HERE, type=pathlib.Path)
    out = parser.parse_args().outdir.resolve()
    out.mkdir(parents=True, exist_ok=True)

    fpr = make_renew(out)
    check(out, "renew-old", fpr, [])
    check(out, "renew-new", fpr, [])
    if len(key_info(out / "renew-new-plus-other.asc")[0]) != 2:
        sys.exit("renew-new-plus-other.asc: expected two keys")

    fpr = make_renew_sub(out)
    check(out, "renew-sub-old", fpr, ["2025-06-01"])
    check(out, "renew-sub-new", fpr, ["2099-12-31"])

    fpr = make_renew_sub2(out)
    check(out, "renew-sub2-a", fpr, ["2025-06-01"])
    check(out, "renew-sub2-a-ext", fpr, ["2099-12-31"])
    check(out, "renew-sub2-ab", fpr, ["2025-06-01", "2026-01-01"])
    check(out, "renew-sub2-ab-ext", fpr, ["2099-12-31", "2026-01-01"])
    print(f"done: {out}")


if __name__ == "__main__":
    main()
