#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pathlib import Path
from osctiny import Osc
from lxml import etree

import keyring
import argparse
import os
import docker


orgCwd = os.getcwd()

parser = argparse.ArgumentParser(
                    prog = 'runtest',
                    description = 'Run extended libzypp tests against a PR' )

parser.add_argument('-u', "--username", help="Username for OBS login")
parser.add_argument('-d', "--directory", help="The directory containing tests", default=orgCwd )
parser.add_argument('-t', "--test", help="Name of the test to run" )

pgroup = parser.add_mutually_exclusive_group(required=False)
pgroup.add_argument('-p', "--password", help="Username for OBS login", default=None )
pgroup.add_argument('-k', '--keyring', action='store_true', default=False, help="Login using credentials from keyring ( requires username )")

parser.add_argument('pr', nargs=1, help="Number of PR to test"   )           # positional argument

args = parser.parse_args()

username = None
password = None

if ( args.username ):
    username = args.username
    if ( args.password ):
        password = args.password
    elif ( args.keyring ):
        print("Trying to read credentials from keyring")
        ring = keyring.get_keyring()
        creds = ring.get_credential( "api.opensuse.org", username)
        if not creds:
            print("No credentials in keyring")
            exit(1)
        username = creds.username
        password = creds.password
    else:
        print("--username must be combined with --password or --keyring\n")
        parser.print_help()
        exit(1)
else:
    print("Using anonymous login")


# Basic Auth with username + password
osc = Osc(
    url="https://api.opensuse.org",
    username=username,
    password=password,
)

pName = "home:zypp-team:openSUSE:libzypp:PR-"+args.pr[0]

# check if project has published libzypp
#res = osc.projects.get_meta("home:zypp-team:openSUSE:libzypp:PR-437")
#print(etree.tostring(res, pretty_print=True,encoding='unicode'))

res = osc.build.get( pName, "libzypp", "openSUSE_Tumbleweed")

packagesPublished = False
for result in res.result:
    if packagesPublished:
        break
    if result.get("project") == pName and result.get("code") == "published":
        for status in result.status:
            if status.get("package") == "libzypp" and status.get("code") == "succeeded":
                packagesPublished = True
                break

if packagesPublished:
    print("Libzypp was published in project: " + pName + " ...proceeding" )
else:
    print("No libzypp package published in project "+pName+" try again at a later time")
    exit(1)

# iterate over the tests
exclude_folders = ["common_files", "testrunner"]

def run_test( testname, testdir ):
    print("Running test "+testname)
    os.chdir( testdir )
    result = False
    try:
        # bootstrap containers
        exit_code = os.system("docker-compose --ansi=never up --build -d --wait")
        if exit_code != 0:
            return result

        #container is up and running, now we need to install the built libzypp with our testpackage
        client = docker.from_env()

        c = client.containers.get( testname+"-app-1" )

        ret = c.exec_run("zypper -n ar "+"https://download.opensuse.org/repositories/home:/zypp-team:/openSUSE:/libzypp:/PR-"+args.pr[0]+"/openSUSE_Tumbleweed PR"+args.pr[0])
        if ret[0] != 0:
            print("Running zypper failed: ")
            print(ret[1].decode("utf-8") )
            return result

        print ( "Added PR Repo" )

        ret = c.exec_run("zypper -n --gpg-auto-import-keys ref")
        if ret[0] != 0:
            print("Running zypper failed: ")
            print(ret[1].decode("utf-8") )
            return result

        print ( "Refreshed PR Repo" )

        ret = c.exec_run("zypper -n up --allow-vendor-change libzypp")
        print(ret[1].decode("utf-8"))
        if ret[0] != 0:
            return result

        print ( "Updated libzypp, preparing test!" )
        ret = c.exec_run("/root/launch.sh")
        if ret[0] != 0:
            print("Running bootstrap failed: ")
            print(ret[1].decode("utf-8") )
            return result

        print ( "Running test!" )
        ret = c.exec_run("zypp-testrunner")
        if ret[0] != 0:
            print("Running tests failed: ")
        else:
            result=True
            print("Test success")

        print(ret[1].decode("utf-8") )
        return result
    finally:
        print("Cleaning up")
        os.system("docker-compose --ansi=never down")
        os.chdir(orgCwd)

hasError = False
canContinue = True

#if test is set stop after the first one
if args.test:
    canContinue = False

for entry in os.listdir( args.directory ):
    if ( args.test and args.test != entry ):
        continue
    print ( "Looking at "+entry )
    f = os.path.join( args.directory, entry )
    if entry in exclude_folders:
        print("Skipping folder: "+entry+" since its in exclude list")
        continue

    if not os.path.isdir(f):
        print("Skipping file: "+entry)
        continue

    if not run_test( entry, f ):
        hasError = True

    if not canContinue or hasError:
        break

if hasError:
    exit(1)

exit(0)
