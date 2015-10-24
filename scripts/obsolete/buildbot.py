import sys
import os
import shutil
import time
import datetime
import cPickle
import traceback
import s3
import util
import util2
import build
import subprocess
from util import file_remove_try_hard, pretty_print_secs
from util import Serializable, create_dir
from util import load_config, strip_empty_lines
from util import verify_path_exists, verify_started_in_right_directory
import runtests

TIME_BETWEEN_PRE_RELEASE_BUILDS_IN_SECS = 60 * 60 * 8  # 8hrs

@util2.memoize
def cert_path():
    scripts_dir = os.path.realpath(os.path.dirname(__file__))
    cert_path = os.path.join(scripts_dir, "cert.pfx")
    return verify_path_exists(cert_path)

def email_msg(msg):
    c = load_config()
    if not c.HasNotifierEmail():
        print("email_build_failed() not ran because not c.HasNotifierEmail()")
        return
    sender, senderpwd = c.GetNotifierEmailAndPwdMustExist()
    subject = "SumatraPDF buildbot failed"
    util.sendmail(sender, senderpwd, ["kkowalczyk@gmail.com"], subject, msg)

def verify_can_send_email():
    c = load_config()
    if not c.HasNotifierEmail():
        print("can't run. scripts/config.py missing notifier_email and/or notifier_email_pwd")
        sys.exit(1)

def is_git_up_to_date():
    out = subprocess.check_output(["git", "pull"])
    return "Already up-to-date" in out

def ignore_pre_release_build_error(s):
    # it's possible we did a pre-release build outside of buildbot and that
    # shouldn't be a fatal error
    if "already exists in s3" in s:
        return True
    return False

def build_pre_release():
    try:
        cert_dst_path = os.path.join("scripts", "cert.pfx")
        if not os.path.exists(cert_dst_path):
            shutil.copyfile(cert_path(), cert_dst_path)
        print("Building pre-release")
        build.build_pre_release()
    except BaseException, e:
        s = str(e)
        print(s)
        # a bit of a hack. not every kind of failure should stop the buildbot
        if not ignore_pre_release_build_error(s):
            traceback.print_exc()
            raise

def buildbot_loop():
    time_of_last_change = None
    while True:
        if not is_git_up_to_date():
            # there was a new checking, it resets the wait time
            time_of_last_change = datetime.datetime.now()
            print("New checkins detected, sleeping for 15 minutes, %s until pre-release" %
                  pretty_print_secs(TIME_BETWEEN_PRE_RELEASE_BUILDS_IN_SECS))
            time.sleep(60 * 15)  # 15 mins
            continue

        if time_of_last_change == None:
            # no changes since last pre-relase, sleep until there is a checkin
            print("No checkins since last pre-release, sleeping for 15 minutes")
            time.sleep(60 * 15)  # 15 mins
            continue

        td = datetime.datetime.now() - time_of_last_change
        secs_until_prerelease = TIME_BETWEEN_PRE_RELEASE_BUILDS_IN_SECS - \
            int(td.total_seconds())
        if secs_until_prerelease > 0:
            print("Sleeping for 15 minutes, %s until pre-release" %
                  pretty_print_secs(secs_until_prerelease))
            time.sleep(60 * 15)  # 15 mins
            continue

        build_pre_release()
        time_of_last_change = None

def main():
    verify_can_send_email()
    cert_path()  # early check and ensures value is memoized
    verify_started_in_right_directory()
    # to avoid problems, we build a separate source tree, just for the buildbot
    src_path = os.path.join("..", "sumatrapdf_buildbot")
    verify_path_exists(src_path)
    conf = load_config()
    s3.set_secrets(conf.aws_access, conf.aws_secret)
    s3.set_bucket("kjkpub")
    os.chdir(src_path)

    # test_email_tests_failed()
    #build_version("8190", skip_release=True)
    # test_build_html_index()
    # build_sizes_json()
    # build_curr(force=True)

    buildbot_loop()

if __name__ == "__main__":
    try:
        main()
    except BaseException, e:
        msg = "buildbot failed\nException: " + str(e) + "\n" + traceback.format_exc() + "\n"
        print(msg)
        email_msg(msg)

