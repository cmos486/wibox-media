#!/usr/bin/env python3
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMMON_CFLAGS = [
    "-Wall", "-Wextra", "-Werror", "-std=gnu99", "-O0",
    "-fprofile-arcs", "-ftest-coverage", "-Isrc/sip_media",
]
TARGETS = {
    "sip_media.c": (30.0, 20.0),
    "call_session.c": (95.0, 75.0),
    "config.c": (98.0, 90.0),
    "mqtt.c": (72.0, 45.0),
    "runtime_config.c": (98.0, 88.0),
    "uart_protocol.c": (98.0, 90.0),
    "sip_sdp.c": (98.0, 85.0),
    "sip_calling.c": (90.0, 70.0),
    "intercom.c": (95.0, 85.0),
    "prometheus.c": (80.0, 60.0),
    "rtsp_stream.c": (75.0, 60.0),
    "audio_hw.c": (95.0, 85.0),
    "video_worker.c": (98.0, 95.0),
    "h264_annexb.c": (97.0, 88.0),
    "snapshot_file.c": (98.0, 88.0),
    "firmware_update.c": (60.0, 45.0),
    "hardware_watchdog.c": (85.0, 75.0),
}
AGGREGATE_LINE_GATE = 68.0
AGGREGATE_BRANCH_GATE = 50.0


def run(command, *, cwd=ROOT, env=None, capture=False):
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.run(
        [str(part) for part in command],
        cwd=cwd,
        env=env,
        check=True,
        text=True,
        capture_output=capture,
    )


def compile_object(source, output, extra_flags=()):
    run(["gcc", *COMMON_CFLAGS, *extra_flags, "-c", source, "-o", output])


def compile_test(source, objects, output, extra_flags=()):
    run(["gcc", *COMMON_CFLAGS, *extra_flags, source, *objects,
         "--coverage", "-o", output])


def parse_gcov(gcno, build_dir):
    result = run(["gcov", "-b", "-c", gcno], cwd=build_dir, capture=True)
    sys.stdout.write(result.stdout)
    current = None
    metrics = {}
    for line in result.stdout.splitlines():
        match = re.match(r"File '(.+)'", line)
        if match:
            current = Path(match.group(1)).name
            metrics.setdefault(current, {})
            continue
        if current is None:
            continue
        match = re.match(r"Lines executed:([0-9.]+)% of ([0-9]+)", line)
        if match:
            # gcov prints an aggregate "Lines executed" footer after the
            # final file without a new File marker. Keep the first value for
            # each file so that test-source totals cannot inflate production.
            metrics[current].setdefault("lines", float(match.group(1)))
            metrics[current].setdefault("line_total", int(match.group(2)))
            continue
        match = re.match(r"Taken at least once:([0-9.]+)% of ([0-9]+)", line)
        if match:
            metrics[current].setdefault("branches", float(match.group(1)))
            metrics[current].setdefault("branch_total", int(match.group(2)))
    return metrics


def main():
    all_metrics = {}
    makefile = (ROOT / "src/sip_media/Makefile").read_text(encoding="utf-8")
    source_match = re.search(r"^SOURCES\s*=\s*(.+)$", makefile, re.MULTILINE)
    if not source_match:
        raise RuntimeError("unable to read production SOURCES from sip_media Makefile")
    production_sources = set(source_match.group(1).split())
    covered_daemon_sources = set(TARGETS) - {"firmware_update.c"}
    if production_sources != covered_daemon_sources:
        missing = sorted(production_sources - covered_daemon_sources)
        stale = sorted(covered_daemon_sources - production_sources)
        raise RuntimeError(
            f"coverage target drift: missing={missing} stale={stale}")
    with tempfile.TemporaryDirectory(prefix="wibox-coverage-") as temporary:
        build = Path(temporary)
        (build / "src").symlink_to(ROOT / "src", target_is_directory=True)

        call_object = build / "call_session.o"
        config_object = build / "config.o"
        runtime_config_object = build / "runtime_config.o"
        uart_protocol_object = build / "uart_protocol.o"
        sip_sdp_object = build / "sip_sdp.o"
        intercom_object = build / "intercom.o"
        prometheus_object = build / "prometheus.o"
        rtsp_object = build / "rtsp_stream.o"
        audio_hw_object = build / "audio_hw.o"
        video_worker_object = build / "video_worker.o"
        h264_object = build / "h264_annexb.o"
        snapshot_file_object = build / "snapshot_file.o"
        watchdog_object = build / "hardware_watchdog.o"
        compile_object("src/sip_media/call_session.c", call_object, ["-pthread"])
        compile_object("src/sip_media/config.c", config_object)
        compile_object("src/sip_media/runtime_config.c", runtime_config_object)
        compile_object("src/sip_media/uart_protocol.c", uart_protocol_object)
        compile_object("src/sip_media/sip_sdp.c", sip_sdp_object)
        compile_object("src/sip_media/intercom.c", intercom_object)
        compile_object("src/sip_media/prometheus.c", prometheus_object, [
            "-pthread", '-DWIBOX_VERSION="coverage-test"',
            '-DWIBOX_COMMIT="test-commit"',
            '-DWIBOX_BUILD_TIMESTAMP="2026-07-14T00:00:00Z"',
        ])
        compile_object("src/sip_media/rtsp_stream.c", rtsp_object, ["-pthread"])
        compile_object("src/sip_media/audio_hw.c", audio_hw_object,
                       ["-pthread", "-DWIBOX_AUDIO_HW_TEST", "-Iinclude/adi"])
        compile_object("src/sip_media/video_worker.c", video_worker_object,
                       ["-DWIBOX_VIDEO_WORKER_TEST"])
        compile_object("src/sip_media/h264_annexb.c", h264_object)
        compile_object("src/sip_media/snapshot_file.c", snapshot_file_object)
        compile_object("src/sip_media/hardware_watchdog.c", watchdog_object,
                       ["-pthread"])

        call_flow = build / "call-flow"
        call_edge = build / "call-edge"
        config_test = build / "config-test"
        runtime_config_test = build / "runtime-config-test"
        uart_protocol_test = build / "uart-protocol-test"
        sip_sdp_test = build / "sip-sdp-test"
        intercom_test = build / "intercom-test"
        prometheus_test = build / "prometheus-test"
        rtsp_test = build / "rtsp-test"
        audio_hw_test = build / "audio-hw-test"
        video_worker_test = build / "video-worker-test"
        h264_test = build / "h264-annexb-test"
        snapshot_file_test = build / "snapshot-file-test"
        sip_calling_test = build / "sip-calling-test"
        sip_media_test = build / "sip-media-test"
        firmware_update_test = build / "firmware-update-test"
        watchdog_test = build / "hardware-watchdog-test"
        compile_test("tests/call_flow_e2e.c", [call_object], call_flow, ["-pthread"])
        compile_test("tests/call_session_edge_test.c", [call_object], call_edge,
                     ["-pthread"])
        compile_test("tests/config_coverage_test.c", [config_object], config_test)
        compile_test("tests/runtime_config_test.c", [runtime_config_object],
                     runtime_config_test)
        compile_test("tests/uart_protocol_test.c", [uart_protocol_object],
                     uart_protocol_test)
        compile_test("tests/sip_sdp_test.c", [sip_sdp_object], sip_sdp_test)
        compile_test("tests/sip_calling_mock_test.c",
                     ["tests/fake_pjsip.c", sip_sdp_object], sip_calling_test,
                     ["-Itests/fakes"])
        compile_test("tests/sip_media_orchestration_test.c",
                     ["tests/fake_pjsip.c", "src/sip_media/config.c",
                      "src/sip_media/runtime_config.c",
                      "src/sip_media/call_session.c",
                      "src/sip_media/uart_protocol.c"],
                     sip_media_test, ["-pthread", "-Itests/fakes"])
        compile_test("tests/intercom_hw_mock_test.c", [intercom_object],
                     intercom_test, ["-Wl,--wrap=access", "-Wl,--wrap=open",
                                      "-Wl,--wrap=write", "-Wl,--wrap=close"])
        compile_test("tests/prometheus_integration_test.c", [prometheus_object],
                     prometheus_test, ["-pthread", "-Wl,--wrap=time"])
        compile_test("tests/rtsp_integration_test.c", [rtsp_object], rtsp_test,
                     ["-pthread"])
        compile_test("tests/audio_hw_mock_test.c", [audio_hw_object], audio_hw_test,
                     ["-pthread", "-Iinclude/adi", "-Wl,--wrap=open",
                      "-Wl,--wrap=write", "-Wl,--wrap=close"])
        compile_test("tests/video_worker_mock_test.c", [video_worker_object],
                     video_worker_test)
        compile_test("tests/h264_annexb_test.c", [h264_object], h264_test,
                     ["-Wl,--wrap=malloc"])
        compile_test("tests/snapshot_file_test.c", [snapshot_file_object],
                     snapshot_file_test,
                     ["-Wl,--wrap=open", "-Wl,--wrap=write",
                      "-Wl,--wrap=fsync", "-Wl,--wrap=close",
                      "-Wl,--wrap=rename"])
        compile_test("tests/firmware_update_mock_test.c", [], firmware_update_test,
                     ["-pthread", "-Isrc", "-Wl,--wrap=open", "-Wl,--wrap=write",
                      "-Wl,--wrap=close", "-Wl,--wrap=ioctl",
                      "-Wl,--wrap=umount2", "-Wl,--wrap=fsync",
                      "-Wl,--wrap=reboot"])
        compile_test("tests/hardware_watchdog_mock_test.c", [watchdog_object],
                     watchdog_test, ["-pthread", "-Wl,--wrap=open",
                                     "-Wl,--wrap=fcntl", "-Wl,--wrap=ioctl",
                                     "-Wl,--wrap=write", "-Wl,--wrap=close",
                                     "-Wl,--wrap=clock_gettime"])

        run([call_flow])
        run([call_edge])
        run([config_test])
        run([runtime_config_test])
        run([uart_protocol_test])
        run([sip_sdp_test])
        run([sip_calling_test])
        run([sip_media_test])
        run([intercom_test])
        run([prometheus_test])
        run([rtsp_test])
        run([audio_hw_test])
        run([video_worker_test])
        run([h264_test])
        run([snapshot_file_test])
        run([firmware_update_test])
        run([watchdog_test])

        mqtt_harness = build / "mqtt-native"
        env = os.environ.copy()
        env["WIBOX_COVERAGE"] = "1"
        env["WIBOX_MQTT_HARNESS"] = str(mqtt_harness)
        run([sys.executable, "tests/mqtt_native_mock.py"], env=env)

        gcno_files = [
            build / "call_session.gcno",
            build / "config.gcno",
            build / "mqtt-native-mqtt.gcno",
            build / "runtime_config.gcno",
            build / "uart_protocol.gcno",
            build / "sip_sdp.gcno",
            build / "intercom.gcno",
            build / "prometheus.gcno",
            build / "rtsp_stream.gcno",
            build / "audio_hw.gcno",
            build / "video_worker.gcno",
            build / "h264_annexb.gcno",
            build / "snapshot_file.gcno",
            build / "hardware_watchdog.gcno",
        ]
        sip_calling_gcno = list(build.glob("*sip_calling_mock_test.gcno"))
        if len(sip_calling_gcno) != 1:
            raise RuntimeError(f"unexpected SIP calling gcov metadata: {sip_calling_gcno}")
        gcno_files.append(sip_calling_gcno[0])
        sip_media_gcno = list(build.glob("*sip_media_orchestration_test.gcno"))
        if len(sip_media_gcno) != 1:
            raise RuntimeError(f"unexpected SIP media gcov metadata: {sip_media_gcno}")
        gcno_files.append(sip_media_gcno[0])
        firmware_gcno = list(build.glob("*firmware_update_mock_test.gcno"))
        if len(firmware_gcno) != 1:
            raise RuntimeError(f"unexpected firmware updater gcov metadata: {firmware_gcno}")
        gcno_files.append(firmware_gcno[0])
        for gcno in gcno_files:
            if not gcno.exists():
                raise RuntimeError(f"missing gcov metadata: {gcno}")
            all_metrics.update(parse_gcov(gcno, build))

    failed = False
    rows = []
    covered_lines = 0.0
    total_lines = 0
    covered_branches = 0.0
    total_branches = 0
    print("\nCoverage gates:")
    for source, (min_lines, min_branches) in TARGETS.items():
        metric = all_metrics.get(source, {})
        lines = metric.get("lines", 0.0)
        branches = metric.get("branches", 0.0)
        passed = lines >= min_lines and branches >= min_branches
        failed = failed or not passed
        status = "PASS" if passed else "FAIL"
        print(f"  {status} {source}: lines={lines:.2f}% (min {min_lines:.2f}%), "
              f"branches={branches:.2f}% (min {min_branches:.2f}%)")
        rows.append((source, lines, min_lines, branches, min_branches, status))
        line_total = metric.get("line_total", 0)
        branch_total = metric.get("branch_total", 0)
        covered_lines += line_total * lines / 100.0
        total_lines += line_total
        covered_branches += branch_total * branches / 100.0
        total_branches += branch_total

    aggregate_lines = covered_lines * 100.0 / total_lines if total_lines else 0.0
    aggregate_branches = (covered_branches * 100.0 / total_branches
                          if total_branches else 0.0)
    aggregate_pass = (aggregate_lines >= AGGREGATE_LINE_GATE and
                      aggregate_branches >= AGGREGATE_BRANCH_GATE)
    failed = failed or not aggregate_pass
    aggregate_status = "PASS" if aggregate_pass else "FAIL"
    print(f"  {aggregate_status} aggregate production: "
          f"lines={aggregate_lines:.2f}% (min {AGGREGATE_LINE_GATE:.2f}%), "
          f"branches={aggregate_branches:.2f}% "
          f"(min {AGGREGATE_BRANCH_GATE:.2f}%)")

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as output:
            output.write("## WiBox code coverage\n\n")
            output.write("| Module | Lines | Gate | Branches | Gate | Status |\n")
            output.write("|---|---:|---:|---:|---:|---|\n")
            for row in rows:
                output.write(f"| `{row[0]}` | {row[1]:.2f}% | {row[2]:.2f}% | "
                             f"{row[3]:.2f}% | {row[4]:.2f}% | {row[5]} |\n")
            output.write(f"| **Aggregate production** | {aggregate_lines:.2f}% | "
                         f"{AGGREGATE_LINE_GATE:.2f}% | {aggregate_branches:.2f}% | "
                         f"{AGGREGATE_BRANCH_GATE:.2f}% | {aggregate_status} |\n")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
