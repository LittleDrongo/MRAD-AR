"""One automatically closing GUI-executable QA run; capture only its own HWND.

No keyboard/mouse input, foreground changes, or inspection of other windows.
Requires Windows, Python 3.10+, and Pillow (only for --visual captures).
The caller must authorize these isolated client invocations.
"""
from pathlib import Path
import argparse, ctypes as C, datetime, hashlib, json, re, socket, subprocess, sys, time
from ctypes import wintypes as W


def capture_owned(process, exe, output):
    """PrintWindow client pixels; never capture the desktop or another PID."""
    from PIL import Image, ImageStat
    if process.poll() is not None:
        raise RuntimeError('Own child has already exited')
    u, k, g = C.windll.user32, C.windll.kernel32, C.windll.gdi32
    u.SetProcessDPIAware()
    callback_type = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    u.EnumWindows.argtypes = [callback_type, W.LPARAM]
    u.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
    u.GetClientRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
    u.GetDC.argtypes = [W.HWND]; u.GetDC.restype = W.HDC
    u.ReleaseDC.argtypes = [W.HWND, W.HDC]
    u.PrintWindow.argtypes = [W.HWND, W.HDC, W.UINT]
    k.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD)]
    length = W.DWORD(32768); path = C.create_unicode_buffer(length.value)
    if not k.QueryFullProcessImageNameW(W.HANDLE(int(process._handle)), 0, path, C.byref(length)):
        raise RuntimeError('Cannot validate own child executable')
    if Path(path.value) != exe:
        raise RuntimeError('Executable mismatch')
    windows = []
    @callback_type
    def enum(hwnd, unused):
        pid = W.DWORD(); u.GetWindowThreadProcessId(hwnd, C.byref(pid))
        if pid.value == process.pid:
            rect = W.RECT(); u.GetClientRect(hwnd, C.byref(rect))
            if rect.right >= 100 and rect.bottom >= 100:
                windows.append((rect.right * rect.bottom, hwnd, rect.right, rect.bottom))
        return True
    u.EnumWindows(enum, 0)
    if not windows:
        raise RuntimeError('Own client has no render window')
    _, hwnd, width, height = max(windows)
    if process.poll() is not None:
        raise RuntimeError('Own child exited before capture')
    pid = W.DWORD(); u.GetWindowThreadProcessId(hwnd, C.byref(pid))
    if pid.value != process.pid:
        raise RuntimeError('Window ownership changed')
    g.CreateCompatibleDC.argtypes = [W.HDC]; g.CreateCompatibleDC.restype = W.HDC
    g.CreateCompatibleBitmap.argtypes = [W.HDC, C.c_int, C.c_int]; g.CreateCompatibleBitmap.restype = W.HBITMAP
    g.SelectObject.argtypes = [W.HDC, W.HANDLE]; g.SelectObject.restype = W.HANDLE
    g.DeleteObject.argtypes = [W.HANDLE]; g.DeleteDC.argtypes = [W.HDC]
    class BitmapHeader(C.Structure):
        _fields_ = [('size', W.DWORD), ('width', W.LONG), ('height', W.LONG), ('planes', W.WORD), ('bits', W.WORD), ('compression', W.DWORD), ('image_size', W.DWORD), ('xppm', W.LONG), ('yppm', W.LONG), ('used', W.DWORD), ('important', W.DWORD)]
    class BitmapInfo(C.Structure):
        _fields_ = [('header', BitmapHeader), ('colors', W.DWORD * 3)]
    g.GetDIBits.argtypes = [W.HDC, W.HBITMAP, W.UINT, W.UINT, W.LPVOID, C.POINTER(BitmapInfo), W.UINT]
    dc = u.GetDC(hwnd); mem = g.CreateCompatibleDC(dc); bmp = g.CreateCompatibleBitmap(dc, width, height)
    old = g.SelectObject(mem, bmp)
    try:
        result = u.PrintWindow(hwnd, mem, 3)
        info = BitmapInfo(); info.header = BitmapHeader(C.sizeof(BitmapHeader), width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0)
        pixels = C.create_string_buffer(width * height * 4)
        scanlines = g.GetDIBits(mem, bmp, 0, height, pixels, C.byref(info), 0)
        if result == 0 or scanlines != height:
            raise RuntimeError('PrintWindow or pixel read failed')
        image = Image.frombuffer('RGB', (width, height), pixels.raw, 'raw', 'BGRX', 0, 1)
        image.save(output)
        return {'path': str(output), 'pid': process.pid, 'hwnd': hwnd, 'width': width, 'height': height, 'meanRGB': ImageStat.Stat(image).mean}
    finally:
        g.SelectObject(mem, old); g.DeleteObject(bmp); g.DeleteDC(mem); u.ReleaseDC(hwnd, dc)


def read_logs(evidence):
    return '\n'.join(path.read_text(encoding='utf-8', errors='replace')
                     for path in (evidence / 'stdout.log', evidence / 'logs' / 'console.log', evidence / 'logs' / 'script.log')
                     if path.exists())


def run_phase(args, evidence, profile, phase, visual):
    # Installed 1.9 local -server mode uses UDP 2001; -bindIP/-port are ignored.
    # Do not collide with another service or change firewall/network settings.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as check:
        check.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        check.bind(('0.0.0.0', args.port))
    evidence.mkdir()
    command = [str(args.game), '-server', args.world,
               '-addonsDir', args.addons_dir, '-addons', 'CBA77A6A1C5E4B20',
               '-profile', str(profile), '-logsDir', str(evidence / 'logs'),
               '-window', '-screenWidth', str(args.width), '-screenHeight', str(args.height),
               '-noFocus', '-forceUpdate', '-noBackend', '-noSound', '-noSplash', '-maxFPS', '30',
               '-noThrow', '-VMErrorMode', 'fatal', '-disableCrashReporter']
    if visual:
        command.append('-mradUIVisual')
    if args.native_fire:
        command.append('-mradNativeFire')
    if phase == 'write':
        command.append('-mradBindingWrite')
    elif phase == 'read':
        command.append('-mradBindingRead')
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    timed_out = False
    captures, capture_errors = [], []
    with (evidence / 'stdout.log').open('wb') as out, (evidence / 'stderr.log').open('wb') as err:
        process = subprocess.Popen(command, cwd=args.game.parent, stdout=out, stderr=err, startupinfo=startup)
        launch = {'pid': process.pid, 'phase': phase, 'evidence': str(evidence), 'command': command}
        (evidence / 'launch.json').write_text(json.dumps(launch, indent=2), encoding='utf-8')
        print(json.dumps(launch), flush=True)
        deadline, ready_at = time.monotonic() + args.timeout, None
        try:
            while process.poll() is None and time.monotonic() < deadline:
                if visual:
                    log = evidence / 'logs' / 'script.log'
                    text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
                    if 'MRAD_SETTINGS_VISUAL_READY' in text and ready_at is None:
                        ready_at = time.monotonic()
                    attempt = len(captures) + len(capture_errors)
                    if ready_at is not None and attempt < 2 and time.monotonic() - ready_at >= 1 + 6 * attempt:
                        try:
                            captures.append(capture_owned(process, args.game, evidence / ('settings-mrad-' + str(attempt + 1) + '.png')))
                            print(json.dumps({'capture': captures[-1]}), flush=True)
                        except Exception as error:
                            capture_errors.append(str(error))
                time.sleep(0.25)
            timed_out = process.poll() is None
        finally:
            if process.poll() is None:
                process.kill()  # retained handle refers only to this invocation's child
            returncode = process.wait()
    output = read_logs(evidence)
    console_values = re.findall(r'MRAD_RUNTIME_BEGIN[^\r\n]*consoleApp=(\w+)', output)
    console_value = console_values[-1] if console_values else None
    runtime_pass = bool(re.search(r'MRAD_RUNTIME_DONE validRows=[1-9][0-9]* failures=0(?:\s|$)', output))
    settings_pass = bool(re.search(r'MRAD_SETTINGS_RESULT checks=[1-9][0-9]* failures=0 profile=PASS ui=LAYOUT_AND_ROWS_PASS(?:\s|$)', output))
    rt_created = 'MRAD_NOTEBOOK_PASS client in-hand state creates its render target' in output
    notebook_pass = bool(re.search(r'MRAD_NOTEBOOK_RESULT checks=[1-9][0-9]* failures=0(?:\s|$)', output))
    visual_pass = bool(re.search(r'MRAD_SETTINGS_VISUAL_RESULT checks=[1-9][0-9]* failures=0 tab=MRAD rows=9(?:\s|$)', output))
    input_phase = {'write': 'WRITE', 'read': 'READ_RESTORE'}.get(phase)
    input_pass = input_phase is None or bool(re.search(r'MRAD_INPUT_RESULT phase=' + input_phase + r' checks=[1-9][0-9]* failures=0(?:\s|$)', output))
    native_fire_pass = not args.native_fire or bool(re.search(r'MRAD_NATIVE_FIRE_RESULT originalShots=3 modifiedShots=3 [^\r\n]* failures=0(?:\s|$)', output))
    script_errors = bool(re.search(r'SCRIPT\s+\(E\)', output))
    # Successful diagnostic strings must not be classified as missing translations.
    error_lines = [line for line in output.splitlines() if re.search(r'\((?:E|W)\)', line)]
    unresolved_mrad = sorted(set(line for line in error_lines if 'MRAD_' in line and re.search(r'(string|locali[sz]|translat)', line, re.I)))
    unknown_keywords = sorted(set(re.findall(r"Unknown keyword/data[^\r\n]*", output)))
    gui_errors = sorted(set(re.findall(r"GUI\s+\(E\)[^\r\n]*", output)))
    mrad_resource_errors = sorted(set(line for line in error_lines if '(E)' in line and re.search(r'(RESOURCES|DEFAULT|GUI)', line) and re.search(r'(\$MRAD:|MRAD_|RangeCard[/\\]|Notepad_LP)', line)))
    assertions_pass = returncode == 0 and not timed_out and runtime_pass and settings_pass and notebook_pass and rt_created and input_pass and native_fire_pass and console_value in ('0', 'false') and not script_errors
    integration_pass = assertions_pass and not unknown_keywords and not unresolved_mrad and not mrad_resource_errors
    if visual:
        integration_pass = integration_pass and visual_pass and bool(captures) and not capture_errors
    report = {'phase': phase, 'integration_pass': integration_pass, 'assertions_pass': assertions_pass,
              'process_exit_code': returncode, 'timed_out': timed_out, 'consoleApp': console_value,
              'runtime_pass': runtime_pass, 'settings_layout_and_native_rows_pass': settings_pass,
              'notebook_render_target_created': rt_created, 'notebook_pass': notebook_pass,
              'input_phase_pass': input_pass if input_phase else None, 'script_errors': script_errors,
              'native_fire_requested': args.native_fire, 'native_fire_pass': native_fire_pass if args.native_fire else None,
              'unknown_keywords': unknown_keywords, 'unresolved_mrad_strings': unresolved_mrad,
              'mrad_resource_errors': mrad_resource_errors, 'gui_errors': gui_errors,
              'actual_native_menu_probe_pass': visual_pass if visual else None,
              'captures': captures, 'capture_errors': capture_errors,
              'visual_review': 'PENDING' if captures else 'NOT_RUN',
              'input_capture_acceptance': 'NOT_RUN', 'evidence': str(evidence), 'command': command}
    (evidence / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report), flush=True)
    for line in dict.fromkeys(output.splitlines()):
        if re.search(r'MRAD_(?:RUNTIME_BEGIN|RUNTIME_DONE|SETTINGS_RESULT|NOTEBOOK_RESULT|SETTINGS_VISUAL_RESULT|INPUT_RESULT|NATIVE_FIRE_RESULT)|MRAD_\w+_(?:FAIL|FAILURE)', line):
            print(line.encode('ascii', 'replace').decode(), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True, help='ArmaReforgerSteamDiag.exe')
    parser.add_argument('--addons-dir', required=True, help='Comma-separated installed game, MRAD and tools/qa roots')
    parser.add_argument('--profile', type=Path, required=True, help='Task-only evidence parent; a fresh child profile is created')
    parser.add_argument('--world', default='{CBA77A6A1C5E4B21}Worlds/MRAD_QA/MRAD_QA.ent')
    parser.add_argument('--port', type=int, choices=(2001,), default=2001,
                        help='Installed local -server mode uses UDP 2001; used for exclusive preflight only')
    parser.add_argument('--timeout', type=int, default=60, help='Seconds per child process')
    parser.add_argument('--width', type=int, default=1440)
    parser.add_argument('--height', type=int, default=900)
    parser.add_argument('--visual', action='store_true', help='Open actual Settings/MRAD and capture only the owned client HWND')
    parser.add_argument('--binding-roundtrip', action='store_true', help='Two separate clients share the fresh profile: write, restart/read/restore')
    parser.add_argument('--native-fire', action='store_true', help='Opt in to native-fire observation and require its strict completion marker')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('This GUI/PrintWindow runner requires Windows')
    args.game = args.game.resolve()
    if not args.game.is_file():
        parser.error('Game executable must exist')
    if args.visual:
        try:
            import PIL  # dependency checked before launching a client
        except ImportError:
            parser.error('--visual requires Pillow installed in this Python environment')
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    evidence = args.profile.resolve() / ('gui-' + stamp)
    evidence.mkdir(parents=True)
    profile = evidence / 'profile'  # deliberately shared by both phases, initially absent
    first_phase = 'write' if args.binding_roundtrip else 'single'
    first = run_phase(args, evidence / first_phase, profile, first_phase, args.visual)
    reports = [first]
    persisted = []
    if args.binding_roundtrip:
        for path in profile.rglob('*.conf'):
            data = path.read_bytes()
            text = data.decode('utf-8', errors='replace')
            if all(value in text for value in ('MRAD_ResetZeroing', 'KC_RCONTROL', 'KC_NUMPAD6')):
                persisted.append({'path': str(path.relative_to(profile)), 'sha256': hashlib.sha256(data).hexdigest()})
                (evidence / ('written-binding-' + str(len(persisted)) + '.conf')).write_bytes(data)
        if first['process_exit_code'] == 0 and not first['timed_out']:
            # Also attempts exact restoration when write assertions failed.
            reports.append(run_phase(args, evidence / 'read', profile, 'read', False))
    backup_removed = not list(profile.rglob('qa-input-backup.json'))
    binding_pass = None
    if args.binding_roundtrip:
        # The separate process's active-map read is the persistence assertion;
        # the plain-text storage copy is additional evidence when available.
        binding_pass = len(reports) == 2 and all(r['input_phase_pass'] for r in reports) and backup_removed
    passed = all(r['integration_pass'] for r in reports) and (not args.binding_roundtrip or binding_pass)
    summary = {'passed': passed, 'binding_roundtrip_pass': binding_pass, 'persisted_binding_files': persisted,
               'binding_backup_removed': backup_removed, 'physical_capture': 'NOT_RUN',
               'visual_review': 'PENDING' if args.visual else 'NOT_RUN',
               'evidence': str(evidence), 'profile': str(profile),
               'phase_reports': [str(Path(r['evidence']) / 'result.json') for r in reports]}
    (evidence / 'result.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary), flush=True)
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
