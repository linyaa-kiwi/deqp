# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import bz2
import glob
import logging
import os
import shutil
import tempfile
import xml.etree.cElementTree as et
from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import service_stopper


class graphics_dEQP(test.test):
    """Run the drawElements Quality Program test suite.
    """
    version = 1
    _services = None
    _board = None
    _cpu_type = None
    _gpu_type = None
    _filter = None
    _width = 256  # Use smallest width for which all tests run/pass.
    _height = 256  # Use smallest height for which all tests run/pass.
    _timeout = 40  # Slightly larger than the dEQP watchdog timeout at 30s.

    DEQP_BASEDIR = '/usr/local/deqp'
    DEQP_MODULES = {
        'dEQP-EGL':    'egl',
        'dEQP-GLES2':  'gles2',
        'dEQP-GLES3':  'gles3',
        'dEQP-GLES31': 'gles31'
    }

    def initialize(self):
        self._gpu_type = utils.get_gpu_family()
        self._cpu_type = utils.get_cpu_soc_family()
        self._board = utils.get_board()
        self._services = service_stopper.ServiceStopper(['ui', 'powerd'])

    def cleanup(self):
        if self._services:
            self._services.restore_services()

    def _parse_test_results(self, result_filename):
        """Handles result files with one or more test results.

        @param result_filename: log file to parse.

        @return: dictionary of parsed test results.
        """
        xml = ''
        xml_start = False
        xml_complete = False
        xml_bad = False
        result = 'ParseTestResultFail'
        test_results = {}

        if not os.path.isfile(result_filename):
            return {}

        with open(result_filename) as result_file:
            for line in result_file.readlines():
                # If the test terminates early, the XML will be incomplete
                # and should not be parsed.
                if line.startswith('#terminateTestCaseResult'):
                    result = line.strip().split()[1]
                    xml_bad = True
                # Will only see #endTestCaseResult if the test does not
                # terminate early.
                elif line.startswith('#endTestCaseResult'):
                    xml_complete = True
                elif xml_start:
                    xml += line
                elif line.startswith('#beginTestCaseResult'):
                    # If we see another begin before an end then something is
                    # wrong.
                    if xml_start:
                        xml_bad = True
                    else:
                        xml_start = True

                if xml_complete or xml_bad:
                    if xml_complete:
                        root = et.fromstring(xml)
                        result = root.find('Result').get('StatusCode').strip()
                        xml_complete = False
                    test_results[result] = test_results.get(result, 0) + 1
                    xml_bad = False
                    xml_start = False
                    result = 'ParseTestResultFail'
                    xml = ''

        return test_results

    def _bootstrap_new_test_cases(self, executable, test_filter):
        """Ask dEQP for all test cases and removes non-Pass'ing ones.

        This function assumes that the '*.Pass' file does not exist and that
        everything else found in the directory should be removed from the tests
        to run. This can be used incrementally updating failing/hangin tests
        over several runs.

        @param executable: location '/usr/local/deqp/modules/gles2/deqp-gles2'.
        @param test_filter: string like 'dEQP-GLES2.info', 'dEQP-GLES3.stress'.

        @return: List of dEQP tests to run.
        """
        test_cases = []
        not_passing_cases = []
        # We did not find passing cases in expectations. Assume everything else
        # that is there should not be run this time.
        expectations_dir = os.path.join(self.bindir, 'expectations',
                                        self._gpu_type)
        subset_spec = '%s.*' % test_filter
        subset_paths = glob.glob(os.path.join(expectations_dir, subset_spec))
        for subset_file in subset_paths:
            not_passing_cases.extend(
                bz2.BZ2File(subset_file).read().splitlines())

        # Now ask dEQP executable nicely for whole list of tests. Needs to be
        # run in executable directory. Output file is plain text file named
        # e.g. 'dEQP-GLES2-cases.txt'.
        command = ('%s '
                   '--deqp-runmode=txt-caselist '
                   '--deqp-surface-type=fbo ' % executable)
        logging.info('Running command %s', command)
        utils.run(command,
                  timeout=60,
                  stderr_is_expected=False,
                  ignore_status=False,
                  stdin=None,
                  stdout_tee=utils.TEE_TO_LOGS,
                  stderr_tee=utils.TEE_TO_LOGS)

        # Now read this caselist file.
        caselist_name = '%s-cases.txt' % test_filter.split('.')[0]
        caselist_file = os.path.join(os.path.dirname(executable), caselist_name)
        if not os.path.isfile(caselist_file):
            raise error.TestError('No caselist file at %s!' % caselist_file)

        # And remove non-Pass'ing expectations from caselist.
        caselist = open(caselist_file).read().splitlines()
        # Contains lines like "TEST: dEQP-GLES2.capability"
        test_cases = []
        match = 'TEST: %s' % test_filter
        logging.info('Bootstrapping test cases matching "%s".', match)
        for case in caselist:
            if case.startswith(match):
                test = case.split('TEST: ')[1]
                logging.info('Found test "%s".', test)
                test_cases.append(test)

        test_cases = list(set(test_cases) - set(not_passing_cases))
        if not test_cases:
            raise error.TestError('Unable to bootstrap %s!' % test_filter)
        test_cases.sort()
        return test_cases

    def _get_test_cases(self, executable, test_filter, subset):
        """Gets the test cases for 'Pass', 'Fail' etc. expectations.

        This function supports bootstrapping of new GPU families and dEQP
        binaries. In particular if there are not 'Pass' expectations found for
        this GPU family it will query the dEQP executable for a list of all
        available tests. It will then remove known non-'Pass'ing tests from this
        list to avoid getting into hangs/crashes etc.

        @param executable: location '/usr/local/deqp/modules/gles2/deqp-gles2'.
        @param test_filter: string like 'dEQP-GLES2.info', 'dEQP-GLES3.stress'.
        @param subset: string from 'Pass', 'Fail', 'Timeout' etc.

        @return: List of dEQP tests to run.
        """
        expectations_dir = os.path.join(self.bindir, 'expectations',
                                        self._gpu_type)
        subset_name = '%s.%s.bz2' % (test_filter, subset)
        subset_path = os.path.join(expectations_dir, subset_name)
        if not os.path.isfile(subset_path):
            if subset != 'Pass':
                raise error.TestError('No subset file found for %s!' %
                                      subset_path)
            return self._bootstrap_new_test_cases(executable, test_filter)

        test_cases = bz2.BZ2File(subset_path).read().splitlines()
        if not test_cases:
            raise error.TestError('No test cases found in subset file %s!' %
                                  subset_path)
        return test_cases

    def run_tests_individually(self, executable, test_cases):
        """Runs tests as isolated from each other, but slowly.

        This function runs each test case separately as a command.
        This means a new context for each test etc. Failures will be more
        isolated, but runtime quite high due to overhead.

        @param executable: dEQP executable path.
        @param test_cases: List of dEQP test case strings.

        @return: dictionary of test results.
        """
        test_results = {}
        width = self._width
        height = self._height

        log_path = os.path.join(tempfile.gettempdir(), '%s-logs' % self._filter)
        shutil.rmtree(log_path, ignore_errors=True)
        os.mkdir(log_path)

        i = 0
        for test_case in test_cases:
            i += 1
            logging.info('[%d/%d] TestCase: %s', i, len(test_cases), test_case)
            log_file = '%s.log' % os.path.join(log_path, test_case)
            command = ('%s '
                       '--deqp-case=%s '
                       '--deqp-surface-type=fbo '
                       '--deqp-log-images=disable '
                       '--deqp-watchdog=enable '
                       '--deqp-surface-width=%d '
                       '--deqp-surface-height=%d '
                       '--deqp-log-filename=%s' %
                       (executable, test_case, width, height, log_file))

            try:
                logging.info('Running single: %s', command)
                utils.run(command,
                          timeout=self._timeout,
                          stderr_is_expected=False,
                          ignore_status=True,
                          stdout_tee=utils.TEE_TO_LOGS,
                          stderr_tee=utils.TEE_TO_LOGS)
                result_counts = self._parse_test_results(log_file)
                if result_counts:
                    result = result_counts.keys()[0]
                else:
                    result = 'Unknown'
            except error.CmdTimeoutError:
                result = 'TestTimeout'
            except error.CmdError:
                result = 'CommandFailed'
            except:
                result = 'UnexpectedError'

            logging.info('Result: %s', result)
            test_results[result] = test_results.get(result, 0) + 1

        return test_results

    def run_tests_hasty(self, executable, test_cases):
        """Runs tests as quickly as possible.

        This function runs all the test cases, but does not isolate tests and
        may take shortcuts/not run all tests to provide maximum coverage at
        minumum runtime.

        @param executable: dEQP executable path.
        @param test_cases: List of dEQP test case strings.

        @return: dictionary of test results.
        """
        # TODO(ihf): It saves half the test time to use 32*32 but a few tests
        # fail as they need surfaces larger than 200*200.
        width = self._width
        height = self._height
        command = ('%s '
                   '--deqp-stdin-caselist '
                   '--deqp-surface-type=fbo '
                   '--deqp-log-images=disable '
                   '--deqp-visibility=hidden '
                   '--deqp-watchdog=enable '
                   '--deqp-surface-width=%d '
                   '--deqp-surface-height=%d ' % (executable, width, height))

        log_file = os.path.join(tempfile.gettempdir(),
                                '%s_hasty.log' % self._filter)
        if os.path.exists(log_file):
            os.remove(log_file)
        command += '--deqp-log-filename=' + log_file

        logging.info('Running hasty %d test cases: %s', len(test_cases),
                     command)
        # We are trying to handle all errors by parsing the log file.
        try:
            utils.run(command,
                      timeout=3600,  # All tests combined less than 1h in hasty.
                      stderr_is_expected=False,
                      ignore_status=False,
                      stdin='\n'.join(test_cases),
                      stdout_tee=utils.TEE_TO_LOGS,
                      stderr_tee=utils.TEE_TO_LOGS)
        except:
            pass

        return self._parse_test_results(log_file)

    def run_once(self, opts=[]):
        options = dict(filter='',
                       timeout=self._timeout,
                       subset_to_run='Pass',  # Pass, Fail, Timeout etc.
                       hasty='False',
                       shard=1,  # TODO(ihf): Support sharding for bvt-cq.
                       shards=1)
        options.update(utils.args_to_dict(opts))
        logging.info('Test Options: %s', options)

        hasty = (options['hasty'] == 'True')
        self._timeout = int(options['timeout'])
        self._filter = options['filter']
        if not self._filter:
            raise error.TestError('No dEQP test filter specified')

        # Some information to help postprocess logs into blacklists later.
        logging.info('ChromeOS BOARD = %s', self._board)
        logging.info('ChromeOS CPU family = %s', self._cpu_type)
        logging.info('ChromeOS GPU family = %s', self._gpu_type)
        logging.info('dEQP test filter = %s', self._filter)

        # Determine module from filter.
        test_prefix, test_group = self._filter.split('.')
        if test_prefix in self.DEQP_MODULES:
            module = self.DEQP_MODULES[test_prefix]
        else:
            raise error.TestError('Invalid test filter: %s' % self._filter)

        executable_path = os.path.join(self.DEQP_BASEDIR, 'modules', module)
        executable = os.path.join(executable_path, 'deqp-%s' % module)

        self._services.stop_services()

        # Must be in the executable directory when running for it to find it's
        # test data files!
        os.chdir(executable_path)
        test_cases = self._get_test_cases(executable, self._filter,
                                          options['subset_to_run'])
        logging.info('Test cases:')
        logging.info(test_cases)

        test_results = {}
        if hasty:
            logging.info('Running in hasty mode.')
            test_results = self.run_tests_hasty(executable, test_cases)
        else:
            logging.info('Running each test individually.')
            test_results = self.run_tests_individually(executable, test_cases)

        logging.info('Test results:')
        logging.info(test_results)
        self.write_perf_keyval(test_results)

        test_count = 0
        test_failures = 0
        for result in test_results:
            test_count += test_results[result]
            if result.lower() not in ['pass', 'notsupported']:
                test_failures += test_results[result]
        # The text "Completed all tests." is used by the process_log.py script
        # and should always appear at the end of a completed test run.
        logging.info('Completed all tests. Saw %d tests and %d failures.',
                     test_count, test_failures)

        if test_failures:
            raise error.TestFail('%d/%d tests failed.' %
                                 (test_failures, test_count))

        if test_count == 0:
            raise error.TestError('No test cases found for filter: %s!' %
                                  self._filter)
