#!/usr/bin/env python

"""
Creates a 7-zip archive of the sources and uploads them to s3.

It's meant to be run after tagging a realease (it checks out
the sources from svn's /tags/${ver}rel branch).

Run as: ./scripts/upload_sources.bat 2.4

The sources are uploaded as:
https://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-${ver}-src.7z
(e.g. https://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-2.5-src.7z)

"""

import os
import sys
import re
import shutil
import util
import util2
import s3


@util2.memoize
def get_top_dir():
    scripts_dir = os.path.realpath(os.path.dirname(__file__))
    return os.path.realpath(os.path.join(scripts_dir, "..", ".."))


# Parse output of svn info and return revision number indicated by
# "Last Changed Rev" field or, if that doesn't exist, by "Revision" field
def extract_url_ver_from_svn_out(txt):
    url = re.findall(r'(?m)^URL: (.+)$', txt)
    assert url
    ver = re.findall(r'(?m)^Last Changed Rev: (\d+)', txt)
    if not ver:
        ver = re.findall(r'(?m)^Revision: (\d+)', txt)
    return url[0].strip(), ver[0].strip()


def get_url_ver():
    raise BaseException("NYI for git")
    (out, err) = util.run_cmd_throw("svn", "info")
    return extract_url_ver_from_svn_out(out)


def get_tmp_src_dir_name(ver):
    return


def ensure_7z_exists():
    util.run_cmd_throw("7z")


def usage_and_exit():
    print("Usage: ./scripts/upload_sources.py ver")
    sys.exit(1)


def upload(ver):
    svn_url = "https://sumatrapdf.googlecode.com/svn/tags/%srel" % ver
    src_dir_name = "SumatraPDF-%s-src" % ver
    archive_name = src_dir_name + ".7z"
    s3_path = "sumatrapdf/rel/" + archive_name
    print("svn_url: '%s'\ndir_name: '%s'\narchive_name: %s\ns3_path: %s" % (svn_url, src_dir_name, archive_name, s3_path))
    s3.verify_doesnt_exist(s3_path)

    os.chdir(get_top_dir())
    util.run_cmd_throw("svn", "export", svn_url, src_dir_name)
    util.run_cmd_throw("7z", "a", "-r", archive_name, src_dir_name)
    s3.upload_file_public(archive_name, s3_path)
    shutil.rmtree(src_dir_name)
    os.remove(archive_name)


# - check out the sources from /svn/tags/${ver}rel to SumatraPDF-$ver-src directory
# - 7-zip them
# - upload to s3
# - delete temporary directory
def main():
    if len(sys.argv) < 2:
        usage_and_exit()
    #print("top_dir: '%s'" % get_top_dir())
    ensure_7z_exists()
    conf = util.load_config()
    assert conf.aws_access is not None, "conf.py is missing"
    s3.set_secrets(conf.aws_access, conf.aws_secret)
    s3.set_bucket("kjkpub")

    ver = sys.argv[1]
    #print("ver: '%s'" % ver)
    upload(ver)


if __name__ == "__main__":
    main()
