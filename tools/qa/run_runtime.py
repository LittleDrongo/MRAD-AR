"""Run the MRAD development world on an isolated local Reforger server."""
from __future__ import annotations
import argparse
import datetime
import json
import pathlib
import re
import socket
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', type=pathlib.Path, required=True)
    parser.add_argument('--addons-dir', required=True)
    parser.add_argument('--profile', type=pathlib.Path, required=True)
    parser.add_argument('--world', default='{CBA77A6A1C5E4B21}Worlds/MRAD_QA/MRAD_QA.ent')
    parser.add_argument('--timeout', type=int, default=90)
    parser.add_argument('--port', type=int, default=2001, help='Debug world hosting uses the native fixed port 2001')
    parser.add_argument('--native-fire', action='store_true',
                        help='Observe six native QA shots and require the velocity comparison result')
    parser.add_argument('--showcase', action='store_true', help='Load the original Arland showcase and inspect its placed notebooks')
    args = parser.parse_args()
    if args.showcase and args.native_fire:
        parser.error('The showcase smoke check and the native-fire fixture use different worlds')
    if args.showcase:
        args.world = 'Worlds/ArlandTest2/GM_Arland.ent'
    if not args.server.is_file():
        parser.error('Server executable must exist')
    if args.port != 2001:
        parser.error('Debug -server <world> ignores port overrides in 1.9; this runner only supports actual port 2001')
    # Never displace an already running editor/server. This debug mode uses
    # the native wildcard listener; it does not change Windows firewall rules.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as check:
        if sys.platform == 'win32':
            check.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        check.bind(('0.0.0.0', args.port))
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    evidence = args.profile.resolve() / ('runtime-' + stamp)
    evidence.mkdir(parents=True)
    command = [str(args.server.resolve()), '-server', args.world,
               '-addonsDir', args.addons_dir, '-addons', 'CBA77A6A1C5E4B20',
               '-profile', str(evidence / 'profile'), '-logsDir', str(evidence / 'logs'),
               '-noBackend',
               '-maxFPS', '30', '-noThrow', '-VMErrorMode', 'fatal', '-disableCrashReporter']
    if args.native_fire:
        command.append('-mradNativeFire')
    if args.showcase:
        command.append('-mradShowcaseSmoke')
    startup = None
    if sys.platform == 'win32':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
    timed_out = False
    with (evidence / 'stdout.log').open('wb') as out, (evidence / 'stderr.log').open('wb') as err:
        process = subprocess.Popen(command, cwd=args.server.parent, stdout=out, stderr=err, startupinfo=startup)
        print(json.dumps({'pid': process.pid, 'evidence': str(evidence)}), flush=True)
        try:
            returncode = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            returncode = process.wait()
    output = ''
    for path in (evidence / 'stdout.log', evidence / 'logs' / 'console.log', evidence / 'logs' / 'script.log'):
        if path.exists():
            output += '\n' + path.read_text(encoding='utf-8', errors='replace')
    complete = bool(re.search(r'MRAD_RUNTIME_DONE validRows=[1-9][0-9]* failures=0(?:\s|$)', output))
    if args.showcase:
        complete = bool(re.search(r'MRAD_SHOWCASE_RESULT notebooks=[1-9][0-9]* failures=0(?:\s|$)', output))
    native_fire_complete = bool(re.search(
        r'MRAD_NATIVE_FIRE_RESULT originalShots=3 modifiedShots=3 [^\r\n]* failures=0(?:\s|$)', output))
    passed = returncode == 0 and not timed_out and complete and 'MRAD_RUNTIME_FAILURE' not in output
    passed = passed and not re.search(r'SCRIPT\s+\(E\)', output)
    if args.native_fire:
        passed = passed and native_fire_complete and 'MRAD_NATIVE_FIRE_FAIL' not in output
    report = {'passed': bool(passed), 'process_exit_code': returncode, 'timed_out': timed_out,
              'completion_marker_found': complete, 'native_fire_requested': args.native_fire,
              'showcase_requested': args.showcase,
              'native_fire_completion_marker_found': native_fire_complete,
              'command': command, 'evidence': str(evidence)}
    (evidence / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
