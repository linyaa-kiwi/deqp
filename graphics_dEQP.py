# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import shutil
import tempfile
import re
import xml.etree.cElementTree as et
from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import service_stopper

class graphics_dEQP(test.test):
    """
    Run the drawElements Quality Program test suite.
    """
    version = 1
    _services = None
    _blacklist = []
    _gpu_type = None
    _cpu_type = None
    _board = None

    DEQP_BASEDIR = "/usr/local/deqp"
    DEQP_MODULES = {
        "dEQP-EGL"    : "egl",
        "dEQP-GLES2"  : "gles2",
        "dEQP-GLES3"  : "gles3",
        "dEQP-GLES31" : "gles31"
    }

    def initialize(self):
        self._gpu_type = utils.get_gpu_family()
        self._cpu_type = utils.get_cpu_soc_family()
        self._board = utils.get_board()
        self._services = service_stopper.ServiceStopper(["ui", "powerd"])


    def cleanup(self):
        if self._services:
            self._services.restore_services()


    def _get_blacklist(self):
        bl_file = "blacklist"
        bl_json = ""
        bl_data = {}

        # Read blacklist json file
        bl_path = os.path.join(self.bindir, bl_file)
        if os.path.isfile(bl_path):
            with open(bl_path, "r") as bl_f:
                for line in bl_f.readlines():
                    if line.strip().startswith("#"):
                        # Ignore comments
                        continue
                    bl_json += line
            # Load json data
            try:
                bl_data = json.loads(bl_json)
            except:
                raise error.TestFail("Bad JSON data in %s" % bl_file)

        # Parse json data
        for bl_set_name in bl_data:
            bl_set = bl_data[bl_set_name]
            matches = {}

            # Criteria section defines what configuration this
            # blacklist set applies to.
            if "criteria" in bl_set:
                bl_set_criteria = bl_set["criteria"]

                # Only criteria is CPU architecture right now
                # More will likely be added
                if "cpu_arch" in bl_set_criteria:
                    cpu_arch = utils.get_cpu_arch()
                    matches["cpu_arch"] = ""
                    if cpu_arch in bl_set_criteria["cpu_arch"]:
                        matches["cpu_arch"] = cpu_arch

                # Check if all criteria were met
                criteria_met = True
                for criterion in matches:
                    if matches[criterion] == "":
                        criteria_met = False
                        break

                # If criteria weren't met, ignore this blacklist set
                if not criteria_met:
                    continue

            # If no tests are specified, all tests are blacklisted,
            # so create message and exit.
            if not "tests" in bl_set:
                msg = "All tests blacklisted"
                num_criteria = len(matches)
                if num_criteria:
                    msg += " for"
                    remaining_criteria = num_criteria
                    for criterion in matches:
                        remaining_criteria -= 1
                        msg += " %s=%s" % (criterion, matches[criterion])
                        if remaining_criteria:
                            msg += " and"
                raise error.TestFail(msg)

            # Add regular expressions in this set to global blacklist
            bl_set_tests = bl_set["tests"]
            for failure_category in bl_set_tests:
                for bl_regex in bl_set_tests[failure_category]:
                    self._blacklist.append(re.compile(bl_regex))


    def _generate_case_list_file(self, exe):
        initial_dir = os.path.abspath(os.path.curdir)
        base_dir = os.path.dirname(exe)

        # Must be in the exe directory when running the exe for it to
        # find it's test data files!
        os.chdir(base_dir)
        result = utils.run("%s --deqp-runmode=txt-caselist "
                           "--deqp-surface-type=fbo" % exe,
                           stderr_is_expected=False,
                           stdout_tee=utils.TEE_TO_LOGS,
                           stderr_tee=utils.TEE_TO_LOGS)
        os.chdir(initial_dir)

        pattern = "Dumping all test case names in '.*' to file " \
                  "'(?P<caselist_path>.*)'.."
        match = re.search(pattern, result.stdout)
        return os.path.join(base_dir, match.group("caselist_path"))


    def _test_cases(self, exe, test_filter):
        with open(self._generate_case_list_file(exe)) as caselist_file:
            pattern = "^TEST: (?P<testcase>.*)"
            for line in caselist_file:
                match = re.search(pattern, line)
                if match is not None:
                    testcase = match.group("testcase").strip()
                    if testcase.startswith(test_filter):
                        if any(bl_regex.search(testcase) for bl_regex in
                               self._blacklist):
                            logging.info("Blacklisted: " + testcase)
                            continue
                        yield match.group("testcase")


    def _parse_test_result(self, result_filename):
        xml = ""
        xml_start = False
        xml_complete = False
        result = "ParseTestResultFail"

        try:
            with open(result_filename) as result_file:
                for line in result_file.readlines():
                    # If the test terminates early, the XML will be incomplete
                    # and should not be parsed
                    if line.startswith("#terminateTestCaseResult"):
                        result = line.strip().split()[1]
                        break
                    # Will only see #endTestCaseResult if the test does not
                    # terminate early
                    if line.startswith("#endTestCaseResult"):
                        xml_complete = True
                        break
                    if xml_start:
                        xml += line
                    elif line.startswith("#beginTestCaseResult"):
                        xml_start = True
            if xml_complete:
                root = et.fromstring(xml)
                result = root.find("Result").get("StatusCode").strip()
        except:
            # Default result of ParseTestResultFail is sufficient
            pass

        return result


    def run_once(self, opts=[]):
        test_results = {}
        test_failures = 0
        test_count = 0

        options = dict(
            filter="",
            timeout=10 * 60,  # 10 minutes max per test
            ignore_blacklist="False"
        )
        options.update(utils.args_to_dict(opts))
        logging.info("Test Options: %s", options)

        timeout = int(options["timeout"])
        test_filter = options["filter"]
        ignore_blacklist = options["ignore_blacklist"] == "True"

        if not test_filter:
            raise error.TestError("No dEQP test filter specified")

        # Some information to help postprocess logs into blacklists later.
        logging.info('ChromeOS BOARD = %s', self._board)
        logging.info('ChromeOS CPU family = %s', self._cpu_type)
        logging.info('ChromeOS GPU family = %s', self._gpu_type)
        logging.info('dEQP test filter = %s', test_filter)

        # Determine module from filter
        test_prefix = test_filter.split('.')[0]
        if test_prefix in self.DEQP_MODULES:
            module = self.DEQP_MODULES[test_prefix]
        else:
            raise error.TestError("Invalid test filter: %s" % test_filter)

        # Read blacklist file
        #if not ignore_blacklist:
        #    self._get_blacklist()

        # The tests produced too many tiny logs for uploading to GS.
        log_path = os.path.join(tempfile.gettempdir(), "deqp-%s-logs" % module)
        shutil.rmtree(log_path, ignore_errors=True)
        os.mkdir(log_path)
        mod_path = os.path.join(self.DEQP_BASEDIR, "modules", module)
        mod_exe = os.path.join(mod_path, "deqp-%s" % module)

        # TODO(ihf): Investigate why frecon needs to be killed. ALso restart
        # after test.
        self._services.stop_services()
        utils.run('pkill frecon', ignore_status=True)

        # Must be in the mod_exe directory when running the mod_exe for it
        # to find it's test data files!
        os.chdir(mod_path)

        for test_case in self._test_cases(mod_exe, test_filter):
            logging.info('TestCase: %s', test_case)
            test_count += 1
            log_file = "%s.log" % os.path.join(log_path, test_case)
            command = ("%s --deqp-case=%s --deqp-log-filename=%s "
                       "--deqp-surface-type=fbo --deqp-log-images=disable "
                       "--deqp-surface-width=256 --deqp-surface-height=256 "
                       "--deqp-watchdog=enable "
                       % (mod_exe, test_case, log_file))
            try:
                utils.run(command, timeout=timeout,
                          stderr_is_expected=False,
                          ignore_status=True,
                          stdout_tee=utils.TEE_TO_LOGS,
                          stderr_tee=utils.TEE_TO_LOGS)
                result = self._parse_test_result(log_file)
            except error.CmdTimeoutError:
                result = "TestTimeout"
            except error.CmdError:
                result = "CommandFailed"
            except:
                result = "UnexpectedError"
            logging.info('Result: %s', result)
            if result.lower() not in ["pass", "notsupported"]:
                test_failures += 1
            test_results[result] = test_results.get(result, 0) + 1

        self.write_perf_keyval(test_results)

        if test_failures:
            raise error.TestFail("%d/%d tests failed."
                                 % (test_failures, test_count))

        if test_count == 0:
            raise error.TestError("No test cases found for filter:%s!"
                                  % (test_filter))
