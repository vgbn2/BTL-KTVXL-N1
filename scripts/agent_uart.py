#!/usr/bin/env python3
"""Non-interactive PuTTY/Plink serial CLI for AI agents driving STM32F103 USART1."""

import argparse
import glob
import json
import math
import os
import queue
import select
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import time

READ_ONLY_COMMANDS = frozenset({"INFO_REQUEST", "INFO_REQUEST:GROUP", "GET_ANGLE"})
LINE_ENDINGS = {"crlf": b"\r\n", "lf": b"\n", "cr": b"\r"}


def emit_result(
    as_json,
    exit_code,
    *,
    ok,
    mode,
    port=None,
    baud=115200,
    command=None,
    line_ending="crlf",
    allow_write=False,
    expect=None,
    matched_expect=None,
    lines=None,
    stderr_lines=None,
    error=None,
    extra=None,
):
    payload = {
        "ok": ok,
        "mode": mode,
        "port": port,
        "baud": baud,
        "command": command,
        "line_ending": line_ending,
        "allow_write": allow_write,
        "expect": expect,
        "matched_expect": matched_expect,
        "lines": list(lines or []),
        "stderr_lines": list(stderr_lines or []),
        "error": error,
    }
    if extra:
        payload.update(extra)

    if as_json:
        print(json.dumps(payload, ensure_ascii=True))
    else:
        if error:
            print(f"ERROR: {error}", file=sys.stderr)
        for line in payload["lines"]:
            print(line)
        if not error and extra and "message" in extra:
            print(extra["message"])
    return exit_code


def probe_ports():
    if os.name == "nt":
        ps = shutil.which("powershell") or shutil.which("powershell.exe") or "powershell.exe"
        try:
            res = subprocess.run(
                [
                    ps,
                    "-NoProfile",
                    "-Command",
                    "[System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object",
                ],
                capture_output=True,
                text=True,
                timeout=5.0,
                check=False,
            )
            if res.returncode != 0:
                return [], []
            ports = [line.strip() for line in res.stdout.splitlines() if line.strip()]
            return ports, []
        except (OSError, subprocess.SubprocessError):
            return [], []

    tty_ports = sorted(p for p in (glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")) if os.path.exists(p))
    by_id = sorted(p for p in glob.glob("/dev/serial/by-id/*") if os.path.exists(p))
    seen = {os.path.realpath(p) for p in tty_ports}
    ports = list(tty_ports) + [p for p in by_id if os.path.realpath(p) not in seen]
    return ports, by_id


def check_port_access(port):
    if os.name == "nt":
        return None
    if not os.path.exists(port):
        return f"Serial port does not exist: {port}"
    try:
        mode = os.stat(port).st_mode
    except OSError as exc:
        return f"Cannot stat serial port {port}: {exc}"
    if not stat.S_ISCHR(mode):
        return f"Serial port is not a character device: {port}"
    if not os.access(port, os.R_OK | os.W_OK):
        user = os.environ.get("USER", "<your-user>")
        return (
            f"Missing read/write permission for {port}. "
            f"Add your user to the 'dialout' group and re-login: "
            f'sudo usermod -aG dialout "{user}"'
        )
    return None


def resolve_port(port_arg):
    if port_arg:
        err = check_port_access(port_arg)
        return (None, err) if err else (port_arg, None)

    ports, _ = probe_ports()
    if len(ports) == 0:
        return None, "No serial ports detected. Connect adapter or pass --port."
    if len(ports) > 1:
        return None, f"Multiple serial ports detected ({', '.join(ports)}); specify one with --port."
    selected = ports[0]
    err = check_port_access(selected)
    return (None, err) if err else (selected, None)


def validate_command(raw_cmd, allow_write):
    if raw_cmd is None:
        return None, None
    cmd = raw_cmd.rstrip("\r\n")
    if not cmd:
        return None, "Command must not be empty."
    if "\r" in cmd or "\n" in cmd or "\x00" in cmd:
        return None, "Command must be a single line without embedded CR, LF, or NUL."
    if cmd not in READ_ONLY_COMMANDS and not allow_write:
        return (
            None,
            f"Write-safety gate blocked command '{cmd}'; pass --allow-write to permit state-mutating or custom commands.",
        )
    return cmd, None


def find_plink(override):
    if override:
        resolved = shutil.which(override) if os.sep not in override and "/" not in override else override
        if resolved and os.path.isfile(resolved) and (os.name == "nt" or os.access(resolved, os.X_OK)):
            return resolved, None
        return None, f"Specified --plink executable is missing or not executable: {override}"

    found = shutil.which("plink") or (shutil.which("plink.exe") if os.name == "nt" else None)
    if found:
        return found, None

    if os.name == "nt":
        candidates = [
            os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "PuTTY", "plink.exe"),
            os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"), "PuTTY", "plink.exe"),
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "plink.exe"),
        ]
        for path in candidates:
            if os.path.isfile(path):
                return path, None

    return None, "plink executable not found in PATH or standard PuTTY directory (override with --plink)."


def split_lines(raw_bytes):
    text = raw_bytes.decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    return [line for line in text.split("\n") if line != ""]


def read_process_streams_posix(proc, timeout):
    out_fd = proc.stdout.fileno()
    err_fd = proc.stderr.fileno()
    os.set_blocking(out_fd, False)
    os.set_blocking(err_fd, False)

    out_buf = bytearray()
    err_buf = bytearray()
    open_fds = {out_fd: out_buf, err_fd: err_buf}
    deadline = time.monotonic() + timeout

    while open_fds:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        readable, _, _ = select.select(list(open_fds.keys()), [], [], remaining)
        if not readable:
            break
        for fd in readable:
            try:
                chunk = os.read(fd, 4096)
            except BlockingIOError:
                continue
            except OSError:
                open_fds.pop(fd, None)
                continue
            if not chunk:
                open_fds.pop(fd, None)
            else:
                open_fds[fd].extend(chunk)

    early_rc = proc.poll()
    terminate_process(proc)

    for fd, buf in ((out_fd, out_buf), (err_fd, err_buf)):
        while True:
            try:
                chunk = os.read(fd, 4096)
            except (BlockingIOError, OSError):
                break
            if not chunk:
                break
            buf.extend(chunk)

    return bytes(out_buf), bytes(err_buf), early_rc


def read_process_streams_windows(proc, timeout):
    out_buf = bytearray()
    err_buf = bytearray()
    q = queue.Queue()

    def pump(stream, tag):
        try:
            fd = stream.fileno()
            while True:
                chunk = os.read(fd, 4096)
                if not chunk:
                    break
                q.put((tag, chunk))
        except OSError:
            pass
        finally:
            q.put((tag, None))

    t_out = threading.Thread(target=pump, args=(proc.stdout, "out"), daemon=True)
    t_err = threading.Thread(target=pump, args=(proc.stderr, "err"), daemon=True)
    t_out.start()
    t_err.start()

    eof_count = 0
    deadline = time.monotonic() + timeout
    while eof_count < 2:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        try:
            tag, chunk = q.get(timeout=remaining)
        except queue.Empty:
            break
        if chunk is None:
            eof_count += 1
        elif tag == "out":
            out_buf.extend(chunk)
        else:
            err_buf.extend(chunk)

    early_rc = proc.poll()
    terminate_process(proc)
    t_out.join(timeout=0.5)
    t_err.join(timeout=0.5)
    while not q.empty():
        tag, chunk = q.get_nowait()
        if chunk:
            (out_buf if tag == "out" else err_buf).extend(chunk)

    return bytes(out_buf), bytes(err_buf), early_rc


def terminate_process(proc):
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=0.5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=1.0)


def run_plink_session(plink_bin, port, baud, cmd, line_ending, timeout):
    argv = [plink_bin, "-batch", "-serial", port, "-sercfg", f"{baud},8,n,1,N"]
    proc = subprocess.Popen(
        argv,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0,
    )
    try:
        if cmd is not None and proc.stdin is not None:
            payload = cmd.encode("utf-8") + LINE_ENDINGS[line_ending]
            try:
                proc.stdin.write(payload)
                proc.stdin.flush()
            except BrokenPipeError:
                pass

        if os.name == "nt":
            raw_out, raw_err, early_rc = read_process_streams_windows(proc, timeout)
        else:
            raw_out, raw_err, early_rc = read_process_streams_posix(proc, timeout)
    finally:
        terminate_process(proc)
        for stream in (proc.stdin, proc.stdout, proc.stderr):
            if stream is not None:
                try:
                    stream.close()
                except OSError:
                    pass

    return split_lines(raw_out), split_lines(raw_err), early_rc


def run_self_test(as_json):
    if not hasattr(os, "openpty"):
        return emit_result(
            as_json,
            1,
            ok=False,
            mode="self-test",
            error="--self-test requires POSIX PTY support (os.openpty).",
        )

    import tty

    os.makedirs("/tmp/opencode", exist_ok=True)
    tmpdir = tempfile.mkdtemp(prefix="agent-uart-selftest-", dir="/tmp/opencode")
    mock_plink = os.path.join(tmpdir, "plink")
    master_fd = None
    slave_fd = None
    stop_event = threading.Event()
    responder = None

    mock_plink_src = (
        "#!/usr/bin/env python3\n"
        "import os, re, select, signal, sys, termios, tty\n"
        "args = sys.argv[1:]\n"
        "if '-batch' not in args or '-serial' not in args or '-sercfg' not in args:\n"
        "    sys.stderr.write('invalid plink args\\n')\n"
        "    sys.exit(2)\n"
        "port = args[args.index('-serial') + 1]\n"
        "sercfg = args[args.index('-sercfg') + 1]\n"
        "if not re.match(r'^\\d+,8,n,1,N$', sercfg):\n"
        "    sys.stderr.write('invalid sercfg\\n')\n"
        "    sys.exit(2)\n"
        "signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))\n"
        "pfd = os.open(port, os.O_RDWR | os.O_NOCTTY)\n"
        "try:\n"
        "    tty.setraw(pfd)\n"
        "except termios.error:\n"
        "    pass\n"
        "in_fd = sys.stdin.fileno()\n"
        "out_fd = sys.stdout.fileno()\n"
        "watch = [in_fd, pfd]\n"
        "try:\n"
        "    while watch:\n"
        "        r, _, _ = select.select(watch, [], [], 0.2)\n"
        "        for fd in r:\n"
        "            data = os.read(fd, 4096)\n"
        "            if not data:\n"
        "                if fd == in_fd:\n"
        "                    watch.remove(in_fd)\n"
        "                    continue\n"
        "                raise SystemExit(0)\n"
        "            os.write(pfd if fd == in_fd else out_fd, data)\n"
        "finally:\n"
        "    os.close(pfd)\n"
    )

    try:
        with open(mock_plink, "w", encoding="utf-8") as fp:
            fp.write(mock_plink_src)
        os.chmod(mock_plink, 0o755)

        master_fd, slave_fd = os.openpty()
        tty.setraw(slave_fd)
        slave_path = os.ttyname(slave_fd)
        os.set_blocking(master_fd, False)

        def pty_device_responder():
            buf = bytearray()
            while not stop_event.is_set():
                r, _, _ = select.select([master_fd], [], [], 0.05)
                if not r:
                    continue
                try:
                    chunk = os.read(master_fd, 4096)
                except (BlockingIOError, OSError):
                    continue
                if not chunk:
                    continue
                buf.extend(chunk)
                while True:
                    nl_pos = -1
                    for sep in (b"\r", b"\n"):
                        idx = buf.find(sep)
                        if idx != -1 and (nl_pos == -1 or idx < nl_pos):
                            nl_pos = idx
                    if nl_pos == -1:
                        break
                    cmd_line = buf[:nl_pos].decode("utf-8", errors="replace").strip()
                    consume = nl_pos + 1
                    if consume < len(buf) and buf[nl_pos : nl_pos + 2] == b"\r\n":
                        consume += 1
                    del buf[:consume]
                    if not cmd_line:
                        continue
                    if cmd_line == "GET_ANGLE":
                        resp = b"ROLL:1.2\r\nPITCH:-0.4\r\n"
                    elif cmd_line in ("INFO_REQUEST", "INFO_REQUEST:GROUP"):
                        resp = b"GROUP_NAME: Nhom 1\r\nHARDWARE_USED: STM32F103C8, MPU6050, ILI9341, W25Qxx\r\n"
                    elif cmd_line == "CALIBRATE_IMU":
                        resp = b"CALIBRATE:OK\r\nACC_OFFSET:0.010,-0.020,0.005\r\nGYRO_OFFSET:0.001,-0.002,0.000\r\n"
                    elif cmd_line.startswith("SET_FILTER_ALPHA:"):
                        val = cmd_line.split(":", 1)[1]
                        resp = f"FILTER_ALPHA:{val}\r\n".encode("utf-8")
                    else:
                        resp = b"ERROR:UNKNOWN_CMD\r\n"
                    os.write(master_fd, resp)

        responder = threading.Thread(target=pty_device_responder, daemon=True)
        responder.start()

        script_path = os.path.abspath(__file__)
        clean_env = {**os.environ, "PYTHONDONTWRITEBYTECODE": "1"}

        def invoke(cli_args):
            return subprocess.run(
                [sys.executable, "-B", script_path, *cli_args],
                capture_output=True,
                text=True,
                env=clean_env,
                timeout=10.0,
                check=False,
            )

        checks = []

        # (a) Write-gate blocks mutating and unrecognized commands without --allow-write (exit 2)
        for blocked_cmd in ("CALIBRATE_IMU", "SET_FILTER_ALPHA:0.95", "UNKNOWN_CMD"):
            res = invoke(["--port", slave_path, "--plink", mock_plink, "--command", blocked_cmd])
            if res.returncode != 2:
                raise AssertionError(f"Expected exit 2 for blocked command {blocked_cmd}, got {res.returncode}")
            checks.append(f"write-gate blocked {blocked_cmd} (exit 2)")

        # (b) Read-only GET_ANGLE over PTY + mock plink succeeds and parses ROLL:/PITCH:
        res_b = invoke(
            ["--port", slave_path, "--plink", mock_plink, "--command", "GET_ANGLE", "--timeout", "0.35", "--json"]
        )
        if res_b.returncode != 0:
            raise AssertionError(f"GET_ANGLE failed ({res_b.returncode}): {res_b.stdout} {res_b.stderr}")
        data_b = json.loads(res_b.stdout)
        if not data_b.get("ok") or not any(l.startswith("ROLL:") for l in data_b.get("lines", [])) or not any(
            l.startswith("PITCH:") for l in data_b.get("lines", [])
        ):
            raise AssertionError(f"Unexpected GET_ANGLE JSON payload: {data_b}")
        checks.append("GET_ANGLE read-only PTY roundtrip parsed ROLL:/PITCH:")

        # (c) --allow-write permits SET_FILTER_ALPHA:0.980 and --expect FILTER_ALPHA:0.980 passes
        res_c = invoke(
            [
                "--port",
                slave_path,
                "--plink",
                mock_plink,
                "--command",
                "SET_FILTER_ALPHA:0.980",
                "--allow-write",
                "--expect",
                "FILTER_ALPHA:0.980",
                "--timeout",
                "0.35",
            ]
        )
        if res_c.returncode != 0 or "FILTER_ALPHA:0.980" not in res_c.stdout:
            raise AssertionError(f"SET_FILTER_ALPHA failed ({res_c.returncode}): {res_c.stdout} {res_c.stderr}")
        checks.append("--allow-write SET_FILTER_ALPHA:0.980 matched --expect")

        # (d) --expect MISSING returns exit 1
        res_d = invoke(
            [
                "--port",
                slave_path,
                "--plink",
                mock_plink,
                "--command",
                "GET_ANGLE",
                "--expect",
                "MISSING",
                "--timeout",
                "0.35",
                "--json",
            ]
        )
        if res_d.returncode != 1:
            raise AssertionError(f"Expected exit 1 for --expect MISSING, got {res_d.returncode}")
        data_d = json.loads(res_d.stdout)
        if data_d.get("ok") is not False or data_d.get("matched_expect") is not False:
            raise AssertionError(f"Unexpected --expect MISSING JSON payload: {data_d}")
        checks.append("--expect MISSING returned exit 1 and matched_expect=false")

        # (e) --probe-ports --json returns valid JSON
        res_e = invoke(["--probe-ports", "--json"])
        if res_e.returncode != 0:
            raise AssertionError(f"--probe-ports --json failed ({res_e.returncode})")
        data_e = json.loads(res_e.stdout)
        if data_e.get("ok") is not True or not isinstance(data_e.get("ports"), list):
            raise AssertionError(f"Unexpected --probe-ports JSON payload: {data_e}")
        checks.append("--probe-ports --json emitted valid JSON")

        # Verify no bytecode artifacts were created inside scripts/
        pycache_dir = os.path.join(os.path.dirname(script_path), "__pycache__")
        if os.path.exists(pycache_dir):
            raise AssertionError(f"Unexpected bytecode directory created: {pycache_dir}")

        return emit_result(
            as_json,
            0,
            ok=True,
            mode="self-test",
            lines=checks,
            extra={"passed": len(checks), "message": f"SELF-TEST PASS ({len(checks)}/{len(checks)} checks)"},
        )
    except Exception as exc:
        return emit_result(as_json, 1, ok=False, mode="self-test", error=str(exc))
    finally:
        stop_event.set()
        if responder is not None:
            responder.join(timeout=1.0)
        for fd in (master_fd, slave_fd):
            if fd is not None:
                try:
                    os.close(fd)
                except OSError:
                    pass
        shutil.rmtree(tmpdir, ignore_errors=True)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Non-interactive PuTTY/Plink serial CLI for STM32F103 USART1 (115200 8-N-1)."
    )
    parser.add_argument("--probe-ports", action="store_true", help="List detected serial ports and exit 0.")
    parser.add_argument("--port", default=None, help="Serial device path (Linux /dev/tty*, Windows COM*).")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200).")
    parser.add_argument("--command", default=None, help="Command line to send over UART.")
    parser.add_argument(
        "--allow-write",
        action="store_true",
        help="Allow state-mutating (CALIBRATE_IMU, SET_FILTER_ALPHA:*) or unrecognized commands.",
    )
    parser.add_argument("--expect", default=None, help="Fail with exit 1 if substring is absent from received lines.")
    parser.add_argument("--timeout", type=float, default=2.0, help="Read window timeout in seconds (> 0, default 2.0).")
    parser.add_argument(
        "--line-ending",
        choices=("crlf", "lf", "cr"),
        default="crlf",
        help="Line terminator appended to --command (default: crlf).",
    )
    parser.add_argument("--plink", default=None, help="Override path to plink executable.")
    parser.add_argument("--json", action="store_true", help="Emit single structured JSON object to stdout.")
    parser.add_argument("--self-test", action="store_true", help="Run deterministic offline PTY self-test.")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)

    if args.baud <= 0:
        return emit_result(
            args.json,
            2,
            ok=False,
            mode="validate",
            baud=args.baud,
            command=args.command,
            line_ending=args.line_ending,
            allow_write=args.allow_write,
            expect=args.expect,
            error=f"--baud must be positive (got {args.baud}).",
        )

    if not math.isfinite(args.timeout) or args.timeout <= 0.0:
        return emit_result(
            args.json,
            2,
            ok=False,
            mode="validate",
            baud=args.baud,
            command=args.command,
            line_ending=args.line_ending,
            allow_write=args.allow_write,
            expect=args.expect,
            error=f"--timeout must be > 0 (got {args.timeout}).",
        )

    if args.self_test:
        return run_self_test(args.json)

    if args.probe_ports:
        ports, by_id = probe_ports()
        extra_links = [link for link in by_id if link not in ports]
        if not ports:
            msg = "No serial ports detected."
        elif extra_links:
            msg = "\n".join(f"by-id: {link}" for link in extra_links)
        else:
            msg = ""
        return emit_result(
            args.json,
            0,
            ok=True,
            mode="probe-ports",
            baud=args.baud,
            lines=ports,
            extra={"ports": ports, "by_id": by_id, **({"message": msg} if msg else {})},
        )

    cmd, cmd_err = validate_command(args.command, args.allow_write)
    if cmd_err:
        return emit_result(
            args.json,
            2,
            ok=False,
            mode="command",
            port=args.port,
            baud=args.baud,
            command=args.command.rstrip("\r\n") if args.command is not None else None,
            line_ending=args.line_ending,
            allow_write=args.allow_write,
            expect=args.expect,
            error=cmd_err,
        )

    port, port_err = resolve_port(args.port)
    if port_err:
        return emit_result(
            args.json,
            1,
            ok=False,
            mode="command" if cmd is not None else "read",
            port=args.port,
            baud=args.baud,
            command=cmd,
            line_ending=args.line_ending,
            allow_write=args.allow_write,
            expect=args.expect,
            error=port_err,
        )

    plink_bin, plink_err = find_plink(args.plink)
    if plink_err:
        return emit_result(
            args.json,
            1,
            ok=False,
            mode="command" if cmd is not None else "read",
            port=port,
            baud=args.baud,
            command=cmd,
            line_ending=args.line_ending,
            allow_write=args.allow_write,
            expect=args.expect,
            error=plink_err,
        )

    try:
        lines, stderr_lines, early_rc = run_plink_session(
            plink_bin, port, args.baud, cmd, args.line_ending, args.timeout
        )
    except OSError as exc:
        return emit_result(
            args.json,
            1,
            ok=False,
            mode="command" if cmd is not None else "read",
            port=port,
            baud=args.baud,
            command=cmd,
            line_ending=args.line_ending,
            allow_write=args.allow_write,
            expect=args.expect,
            error=f"Failed to launch plink ({plink_bin}): {exc}",
        )

    mode = "command" if cmd is not None else "read"
    if early_rc is not None and early_rc != 0:
        detail = "; ".join(stderr_lines) if stderr_lines else f"exit code {early_rc}"
        return emit_result(
            args.json,
            1,
            ok=False,
            mode=mode,
            port=port,
            baud=args.baud,
            command=cmd,
            line_ending=args.line_ending,
            allow_write=args.allow_write,
            expect=args.expect,
            lines=lines,
            stderr_lines=stderr_lines,
            error=f"plink failed ({detail})",
        )

    matched_expect = None
    if args.expect is not None:
        matched_expect = any(args.expect in line for line in lines)
        if not matched_expect:
            return emit_result(
                args.json,
                1,
                ok=False,
                mode=mode,
                port=port,
                baud=args.baud,
                command=cmd,
                line_ending=args.line_ending,
                allow_write=args.allow_write,
                expect=args.expect,
                matched_expect=False,
                lines=lines,
                stderr_lines=stderr_lines,
                error=f"Expected substring {args.expect!r} not found in received lines.",
            )

    return emit_result(
        args.json,
        0,
        ok=True,
        mode=mode,
        port=port,
        baud=args.baud,
        command=cmd,
        line_ending=args.line_ending,
        allow_write=args.allow_write,
        expect=args.expect,
        matched_expect=matched_expect,
        lines=lines,
        stderr_lines=stderr_lines,
    )


if __name__ == "__main__":
    sys.exit(main())
