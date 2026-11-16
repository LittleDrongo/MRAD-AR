"""Run a local server, a live replication client, and a separate JIP client.

Every process has its own profile/log directory. Only processes started by this
invocation can be terminated. No existing editor or game session is controlled.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import pathlib
import re
import socket
import subprocess
import sys
import time


# The local -server entry point forwards this string to joining clients.
# A resource-name GUID prefix loads on the host but is not a valid world file
# path for the client handshake in 1.9.0.37.
WORLD = 'Worlds/MRAD_NetworkQA/MRAD_NetworkQA.ent'
QA_ADDON = 'CBA77A6A1C5E4B20'
LOCAL_RPL_PORT = 2001


def source_manifest() -> dict[str, str]:
    """Fingerprint task-owned engine inputs, excluding generated caches."""
    checkout = pathlib.Path(__file__).resolve().parents[2]
    roots = (checkout / 'MRAD', checkout / 'tools' / 'qa' / 'MRAD_QA')
    extensions = {'.c', '.gproj', '.et', '.conf', '.layout', '.st', '.meta', '.ent', '.layer'}
    manifest = {}
    for root in roots:
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.suffix.lower() in extensions:
                key = root.name + '/' + path.relative_to(root).as_posix()
                manifest[key] = hashlib.sha256(path.read_bytes()).hexdigest()
    return manifest


def read_output(directory: pathlib.Path) -> str:
    chunks = []
    for path in (directory / 'stdout.log', directory / 'logs' / 'script.log', directory / 'logs' / 'console.log'):
        if path.exists():
            chunks.append(path.read_text(encoding='utf-8', errors='replace'))
    return '\n'.join(chunks)


def wait_for_exit(process: subprocess.Popen, timeout: float) -> int:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = process.poll()
        if result is not None:
            return result
        time.sleep(0.25)
    raise TimeoutError(f'process {process.pid} exceeded its deadline')


def wait_for_marker(process: subprocess.Popen, directory: pathlib.Path, marker: str, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if marker in read_output(directory):
            return
        if process.poll() is not None:
            raise RuntimeError(f'server exited before {marker}')
        time.sleep(0.25)
    raise TimeoutError(f'server did not produce {marker}')


def verify_listener(process: subprocess.Popen, directory: pathlib.Path) -> dict:
    """Confirm that the port used by local -client belongs to our server."""
    output = read_output(directory)
    match = re.search(r'Starting RPL server, listening on address ([^\s,]+)', output)
    if not match or not match.group(1).endswith(':' + str(LOCAL_RPL_PORT)):
        raise RuntimeError('Server did not report the expected local RPL listener on port 2001')
    if process.poll() is not None:
        raise RuntimeError('Server exited before its listener could be verified')
    # netstat is read-only. Checking the owner prevents a client from accidentally
    # joining another process if the engine failed to acquire its default port.
    if sys.platform != 'win32':
        raise RuntimeError('The installed Windows engine runner requires Windows listener ownership checks')
    result = subprocess.run(['netstat.exe', '-ano', '-p', 'udp'], capture_output=True,
                            text=True, check=True, timeout=10)
    owners = set()
    endpoints = []
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) < 4 or fields[0].upper() != 'UDP':
            continue
        if fields[1].rsplit(':', 1)[-1] == str(LOCAL_RPL_PORT):
            owners.add(int(fields[-1]))
            endpoints.append(fields[1])
    if owners != {process.pid}:
        raise RuntimeError(f'UDP 2001 owner mismatch: expected PID {process.pid}, observed {sorted(owners)}')
    return {'reported_endpoint': match.group(1), 'udp_endpoints': endpoints, 'owner_pid': process.pid}


def client_failure(output: str, description: str) -> RuntimeError:
    details = [line.strip() for line in output.splitlines()
               if re.search(r'MRAD_NETWORK_FAIL|Unable to set world|Unable to initialize|SCRIPT\s+\(E\)|Client.*[Dd]isconnect', line)]
    return RuntimeError(description + (': ' + ' | '.join(dict.fromkeys(details)) if details else ''))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', type=pathlib.Path, required=True)
    parser.add_argument('--client', type=pathlib.Path, required=True)
    parser.add_argument('--addons-dir', required=True)
    parser.add_argument('--profile', type=pathlib.Path, required=True)
    parser.add_argument('--port', type=int, choices=(LOCAL_RPL_PORT,), default=LOCAL_RPL_PORT,
                        help='Local -server/-client mode in the installed engine uses 2001; hosting config overrides do not apply')
    parser.add_argument('--client-timeout', type=int, default=65)
    parser.add_argument('--ads', action='store_true', help='Also run the bounded owned-client ADS and production-RPC checks after the live data snapshot')
    args = parser.parse_args()
    if not args.server.is_file() or not args.client.is_file():
        parser.error('Both installed server and game executables must exist')
    # Do not displace an existing local service or alter firewall rules.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as check:
        if sys.platform == 'win32':
            check.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        check.bind(('0.0.0.0', args.port))

    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    evidence = args.profile.resolve() / ('network-' + stamp)
    evidence.mkdir(parents=True)
    snapshot = source_manifest()
    snapshot_text = json.dumps(snapshot, sort_keys=True, indent=2)
    snapshot_sha256 = hashlib.sha256(snapshot_text.encode('utf-8')).hexdigest()
    (evidence / 'source-manifest.json').write_text(snapshot_text, encoding='utf-8')
    processes: list[subprocess.Popen] = []
    files = []
    records = []
    startup = None
    if sys.platform == 'win32':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE

    def start(role: str, executable: pathlib.Path, extra: list[str]) -> tuple[subprocess.Popen, pathlib.Path, dict]:
        current = source_manifest()
        if current != snapshot:
            changed = sorted(key for key in set(snapshot) | set(current) if snapshot.get(key) != current.get(key))
            raise RuntimeError('Task sources changed during the multi-process test; keep the PC snapshot frozen until all peers exit: ' + ', '.join(changed[:12]))
        directory = evidence / role
        directory.mkdir()
        command = [str(executable.resolve()), *extra,
                   '-addonsDir', args.addons_dir, '-addons', QA_ADDON,
                   '-profile', str(directory / 'profile'), '-logsDir', str(directory / 'logs'),
                   '-noBackend', '-noThrow', '-VMErrorMode', 'fatal', '-disableCrashReporter',
                   '-maxFPS', '30', '-noFocus', '-forceUpdate', '-noSound']
        if args.ads:
            command.append('-mradNetworkADS')
        out = (directory / 'stdout.log').open('wb')
        err = (directory / 'stderr.log').open('wb')
        files.extend((out, err))
        process = subprocess.Popen(command, cwd=executable.parent, stdout=out, stderr=err, startupinfo=startup)
        processes.append(process)
        record = {'role': role, 'pid': process.pid, 'command': command, 'evidence': str(directory)}
        records.append(record)
        print(json.dumps(record), flush=True)
        return process, directory, record

    problem = None
    passed = False
    try:
        server, server_directory, server_record = start('server', args.server,
            ['-server', WORLD, '-nds', '0'])
        wait_for_marker(server, server_directory, 'MRAD_NETWORK_READY phase=1 awaitingFirstClient=1', 35)
        server_record['listener'] = verify_listener(server, server_directory)
        window = ['-window', '-screenWidth', '1024', '-screenHeight', '720']
        first, first_directory, first_record = start('client-live', args.client,
            ['-client', '127.0.0.1', *window])
        first_record['exit_code'] = wait_for_exit(first, args.client_timeout)
        first_output = read_output(first_directory)
        first_record['passed'] = (first_record['exit_code'] == 0
            and 'MRAD_NETWORK_CLIENT_RESULT snapshots=2 firstPhase=1 lastPhase=2 failures=0' in first_output
            and 'MRAD_NETWORK_FAIL' not in first_output
            and not re.search(r'SCRIPT\s+\(E\)', first_output))
        if args.ads:
            first_record['passed'] = first_record['passed'] and 'MRAD_ADS_CLIENT_RESULT stages=14 actionDispatches=1 failures=0' in first_output
        if not first_record['passed']:
            raise client_failure(first_output, 'The first client did not verify both initial and changed replicated state')
        second, second_directory, second_record = start('client-jip', args.client,
            ['-client', '127.0.0.1', *window])
        second_record['exit_code'] = wait_for_exit(second, args.client_timeout)
        second_output = read_output(second_directory)
        second_record['passed'] = (second_record['exit_code'] == 0
            and 'MRAD_NETWORK_CLIENT_RESULT snapshots=1 firstPhase=2 lastPhase=2 failures=0' in second_output
            and 'MRAD_NETWORK_FAIL' not in second_output
            and not re.search(r'SCRIPT\s+\(E\)', second_output))
        if not second_record['passed']:
            raise client_failure(second_output, 'The separate JIP client did not verify the already changed replicated state')
        server_record['exit_code'] = wait_for_exit(server, 15)
        server_output = read_output(server_directory)
        server_record['passed'] = (server_record['exit_code'] == 0
            and bool(re.search(r'MRAD_NETWORK_SERVER_RESULT phase=2 finalAcks=[2-9][0-9]* failures=0(?:\s|$)', server_output))
            and 'MRAD_NETWORK_FAIL' not in server_output
            and not re.search(r'SCRIPT\s+\(E\)', server_output))
        if args.ads:
            server_record['passed'] = server_record['passed'] and 'MRAD_ADS_AUTHORITY_RESULT stages=14 failures=0' in server_output and 'MRAD_ADS_AUTHORITY_FAIL' not in server_output
        passed = first_record['passed'] and second_record['passed'] and server_record['passed']
    except (TimeoutError, RuntimeError, OSError, subprocess.SubprocessError) as error:
        problem = str(error)
    finally:
        for process in processes:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
        for handle in files:
            handle.close()
        scope = 'Actual network row/page/tick replication and JIP'
        if args.ads:
            scope += '; owned-client scripted native ADS entry, production RPC permission/clamp/reset and ActionManager dispatch'
        scope += '; no physical-key mapping, item transfer or impact claim'
        report = {'passed': bool(passed), 'problem': problem, 'evidence': str(evidence), 'processes': records,
                  'source_manifest': str(evidence / 'source-manifest.json'),
                  'source_sha256': snapshot_sha256, 'scope': scope}
        (evidence / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False), flush=True)
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
