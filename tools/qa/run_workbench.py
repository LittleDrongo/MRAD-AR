"""Run an isolated Workbench check without changing an existing editor session.

Requires the installed Arma Reforger Tools build. Evidence is written beneath
the explicitly supplied profile, never into the game installation or registry.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import pathlib
import re
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workbench", type=pathlib.Path, required=True)
    parser.add_argument("--project", type=pathlib.Path, required=True)
    parser.add_argument("--addons-dir", required=True)
    parser.add_argument("--profile", type=pathlib.Path, required=True)
    parser.add_argument("--action", choices=("validate", "export", "tests", "wind", "localize"), default="validate")
    parser.add_argument("--configuration", default="PC")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    if not args.workbench.is_file() or not args.project.is_file():
        parser.error("Workbench executable and project must exist")

    profile = args.profile.resolve()
    profile.mkdir(parents=True, exist_ok=True)
    settings = profile / "workbench.ini"
    if not settings.exists():
        settings.write_text("", encoding="utf-8")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    evidence = profile / (args.action + "-" + stamp)
    evidence.mkdir()
    command = [
        str(args.workbench.resolve()), "-noFocus", "-exitAfterInit",
        "-disableCrashReporter", "-noBackend", "-noThrow", "-VMErrorMode", "fatal",
        "-profile", str(profile), "-logsDir", str(evidence / "logs"),
        "-forceSettings", str(settings), "-addonsDir", args.addons_dir,
        "-gproj", str(args.project.resolve()),
    ]
    if args.action == "validate":
        command += ["-wbModule=ScriptEditor", "-validate=" + args.configuration]
    elif args.action == "wind":
        command += ["-wbModule=WorldEditor", "-plugin=MRAD_WindProbePlugin"]
    elif args.action == "localize":
        (evidence / "localization-built").mkdir()
        command += ["-wbModule=LocalizationEditor", "-run", "-load", "{8ED63B60FABF6B61}UI/Lang/MradLang.st", "-build", "-saveDir", str(evidence / "localization-built")]
    else:
        plugin = "MRAD_SourceExportPlugin" if args.action == "export" else "MRAD_ValidatePlugin"
        command += ["-wbModule=ResourceManager", "-plugin=" + plugin]

    startup = None
    if sys.platform == "win32":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
    timed_out = False
    with (evidence / "stdout.log").open("wb") as out, (evidence / "stderr.log").open("wb") as err:
        process = subprocess.Popen(command, cwd=args.workbench.parent, stdout=out, stderr=err, startupinfo=startup)
        print(json.dumps({"pid": process.pid, "evidence": str(evidence)}), flush=True)
        try:
            returncode = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()  # only the process started by this invocation
            returncode = process.wait()

    output = (evidence / "stdout.log").read_text(encoding="utf-8", errors="replace")
    # The GUI executable can write Print() only to its own logs on Windows.
    for name in ("script.log", "console.log"):
        log = evidence / "logs" / name
        if log.exists():
            output += "\n" + log.read_text(encoding="utf-8", errors="replace")
    marker = {
        "validate": "Script validation successful.",
        "export": "MRAD_EXPORT_DONE",
        "tests": "MRAD_VALIDATE_RESULT",
        "wind": "MRAD_WIND_PROBE_RESULT",
        "localize": "MRAD_LOCALIZATION_GENERATED",
    }[args.action]
    if args.action == "localize":
        locales = ("cs_cz", "de_de", "en_us", "es_es", "fr_fr", "it_it", "ja_jp", "ko_kr", "pl_pl", "pt_br", "ru_ru", "uk_ua", "zh_cn")
        generated = []
        for locale in locales:
            matches = list((evidence / "localization-built").rglob("*." + locale + ".conf"))
            if len(matches) != 1:
                continue
            data = matches[0].read_bytes()
            keys = set(re.findall(rb"MRAD_[A-Za-z0-9_]+", data))
            generated.append({"locale": locale, "path": str(matches[0]), "keys": len(keys), "sha256": hashlib.sha256(data).hexdigest()})
        (evidence / "localization_manifest.json").write_text(json.dumps(generated, indent=2), encoding="utf-8")
        if len(generated) == 13 and all(item["keys"] == 50 for item in generated):
            output += "\n" + marker
    passed = not timed_out and returncode == 0 and marker in output
    if args.action == "tests":
        passed = passed and bool(re.search(r"MRAD_VALIDATE_RESULT checks=[1-9][0-9]* failures=0(?:\s|$)", output))
        passed = passed and "MRAD_QA_FAIL" not in output
    elif args.action == "wind":
        passed = passed and bool(re.search(r"MRAD_WIND_PROBE_RESULT samples=360 valid=360 failures=0 unavailable=0(?:\s|$)", output))
        passed = passed and "MRAD_EDITOR_NOTEPAD_RESULT editMode=1 hasRT=0 held=0" in output
        passed = passed and "MRAD_WIND_PROBE_FAIL" not in output
    passed = passed and not re.search(r"SCRIPT\s+\(E\)", output)
    report = {"action": args.action, "configuration": args.configuration,
              "process_exit_code": returncode, "timed_out": timed_out,
              "completion_marker_found": marker in output, "passed": passed,
              "command": command, "evidence": str(evidence)}
    (evidence / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
