from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import commands
import json
import sys

# cmd, parameter, whether test non-JSON
all_tests = [
    ['list', '', True],
    ['list', '--detail', True],
    ['hdd', '--all', True],
    ['hdd', '%s', True],
    ['info', '%s', True],
    ['sensor', '%s', True],
    ['sensor', '%s --thresholds', False],
    ['led', '%s', True],
    ['fan', '%s', True],
    ['gpio', '%s', True],
    ['tag', '%s', True],
    ['event', '--log %s', False],
    ['event', '--status %s', True],
    ['phyerr', '%s', True],
    ['config', '%s', True],
    ['version', '', True],
]

def find_device(binary):
    ret = []
    fullcmd = '%s list 2> /dev/null' % (binary)
    output = commands.getoutput(fullcmd)
    for line in output.split('\n'):
        device = line.split()[0]
        if device.startswith('/dev/sg'):
            ret.append(device)
    print('Testing on %s\n' % str(ret))
    return ret

def testJSONCommand(binary, cmd, parameters):
    fullcmd = '%s %s --json %s 2>/dev/null' % (binary, cmd, parameters)
    output = commands.getoutput(fullcmd)

    try:
        json_output = json.loads(output)
    except:
        print('Fail JSON: %s' % fullcmd)
        print(output)
        return False
    print('Pass JSON: %s' % (fullcmd))
    return True

def testNoneJSONCommand(binary, cmd, parameters):
    fullcmd = '%s %s %s 2>/dev/null' % (binary, cmd, parameters)
    output = commands.getoutput(fullcmd)

    unexpected = ['{', '}', ',']
    for c in unexpected:
        try:
            if c in output:
                print('Fail None JSON: %s' % fullcmd)
                return False
        except:
            print('Expection in testNoneJSONCommand')
    print('Pass None JSON: %s' % (fullcmd))
    return True


def usage():
    print('Usage:')
    print('\t%s <ocpjbod_bin>')

def main():
    if len(sys.argv) != 2:
        usage()

    binary = sys.argv[1]
    devices = find_device(binary)
    if len(devices) == 0:
        print('Cannot find a valid JBOD.')
        return

    total_test = 0
    passed_test = 0

    for device in devices:
        for test in all_tests:
            cmd = test[0]
            parameters = test[1]
            testNoneJSON = test[2]
            if '%s' in parameters:
                parameters = parameters % device
            total_test += 1
            if testJSONCommand(binary, cmd, parameters):
                if not testNoneJSON or \
                   testNoneJSONCommand(binary, cmd, parameters):
                    passed_test += 1

    print('\nPassed %.2f %% (%d of %d) tests.' % (
            100 * passed_test / total_test, passed_test, total_test))

main()
