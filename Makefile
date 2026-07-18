.DEFAULT_GOAL := help

.PHONY: \
	docker docker-shell build build-media prepare-base test verify verify-image \
	test-mqtt test-call-flow test-call-session-edge test-config test-runtime-config \
	test-uart-protocol test-sip-sdp test-sip-calling test-sip-media-orchestration test-intercom test-prometheus test-rtsp \
	test-audio-hw test-video-worker test-h264-annexb test-snapshot-file \
	test-firmware-update test-hardware-watchdog test-app-watchdog \
	test-runtime-contract test-spec-coverage test-device-contract \
	test-watchdog test-wifi-portal coverage \
	deploy-runtime verify-device verify-runtime verify-mqtt device-status \
	build-inside extract patch pack clean help

BUILD_DIR := cramfs
BASE_IMAGE := mtd4
DATE := $(shell date +%y%m%d-%H%M)
BUILD_IMAGE := wibox-build-tool:latest

WIBOX_IP ?= 192.168.0.196
WIBOX_USER ?= root
WIBOX_PASS ?= qv2008

docker:
	docker build -t $(BUILD_IMAGE) .

docker-shell:
	docker run --rm -it -v $(PWD):/build $(BUILD_IMAGE) bash

build: prepare-base build-media
	docker run --rm -v $(PWD):/build $(BUILD_IMAGE) make build-inside

prepare-base:
	@if [ ! -f "$(BUILD_DIR)/lib/libssl.so.1.1" ] || [ ! -f "$(BUILD_DIR)/lib/libcrypto.so.1.1" ]; then \
		echo "[*] Extracting base image for build libraries"; \
		docker run --rm -v $(PWD):/build $(BUILD_IMAGE) make extract; \
	fi

build-media: prepare-base
	BUILD_IMAGE=$(BUILD_IMAGE) scripts/build_wibox_media_daemon.sh
	rm -f src/sip_media/sip_media src/sip_media/wibox-media-daemon src/sip_media/*.o

test: test-mqtt test-call-flow test-call-session-edge test-config \
	test-runtime-config test-uart-protocol test-sip-sdp test-sip-calling \
	test-sip-media-orchestration test-intercom \
	test-prometheus test-rtsp test-audio-hw test-video-worker \
	test-h264-annexb test-snapshot-file test-firmware-update \
	test-hardware-watchdog test-app-watchdog test-runtime-contract \
	test-spec-coverage test-device-contract test-watchdog test-wifi-portal

test-mqtt:
	tests/mqtt_native_mock.py

test-call-flow:
	@set -e; bin=/tmp/wibox_call_flow_e2e; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread -Isrc/sip_media \
			tests/call_flow_e2e.c src/sip_media/call_session.c -o "$$bin"; \
		"$$bin"

test-call-session-edge:
	@set -e; bin=/tmp/wibox_call_session_edge_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread -Isrc/sip_media \
			tests/call_session_edge_test.c src/sip_media/call_session.c -o "$$bin"; \
		"$$bin"

test-config:
	@set -e; bin=/tmp/wibox_config_coverage_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Isrc/sip_media \
			tests/config_coverage_test.c src/sip_media/config.c -o "$$bin"; \
		"$$bin"

test-runtime-config:
	@set -e; bin=/tmp/wibox_runtime_config_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Isrc/sip_media \
			tests/runtime_config_test.c src/sip_media/runtime_config.c -o "$$bin"; \
		"$$bin"

test-uart-protocol:
	@set -e; bin=/tmp/wibox_uart_protocol_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Isrc/sip_media \
			tests/uart_protocol_test.c src/sip_media/uart_protocol.c -o "$$bin"; \
		"$$bin"

test-sip-sdp:
	@set -e; bin=/tmp/wibox_sip_sdp_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Isrc/sip_media \
			tests/sip_sdp_test.c src/sip_media/sip_sdp.c -o "$$bin"; \
		"$$bin"

test-sip-calling:
	@set -e; bin=/tmp/wibox_sip_calling_mock_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Itests/fakes -Isrc/sip_media \
			tests/sip_calling_mock_test.c tests/fake_pjsip.c \
			src/sip_media/sip_sdp.c -o "$$bin"; \
		"$$bin"

test-sip-media-orchestration:
	@set -e; bin=/tmp/wibox_sip_media_orchestration_test; \
		trap 'rm -f "$$bin" /tmp/wibox-orchestration-test.pipe' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread \
			-Itests/fakes -Isrc/sip_media \
			tests/sip_media_orchestration_test.c tests/fake_pjsip.c \
			src/sip_media/config.c src/sip_media/runtime_config.c \
			src/sip_media/call_session.c src/sip_media/uart_protocol.c \
			-o "$$bin"; \
		"$$bin"

test-intercom:
	@set -e; bin=/tmp/wibox_intercom_hw_mock_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Isrc/sip_media \
			tests/intercom_hw_mock_test.c src/sip_media/intercom.c \
			-Wl,--wrap=access -Wl,--wrap=open -Wl,--wrap=write -Wl,--wrap=close \
			-o "$$bin"; \
		"$$bin"

test-prometheus:
	@set -e; bin=/tmp/wibox_prometheus_integration_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread -Isrc/sip_media \
			-DWIBOX_VERSION='"coverage-test"' -DWIBOX_COMMIT='"test-commit"' \
			-DWIBOX_BUILD_TIMESTAMP='"2026-07-14T00:00:00Z"' \
			tests/prometheus_integration_test.c src/sip_media/prometheus.c \
			-Wl,--wrap=time -o "$$bin"; \
		"$$bin"

test-rtsp:
	@set -e; bin=/tmp/wibox_rtsp_integration_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread -Isrc/sip_media \
			tests/rtsp_integration_test.c src/sip_media/rtsp_stream.c -o "$$bin"; \
		"$$bin"

test-audio-hw:
	@set -e; bin=/tmp/wibox_audio_hw_mock_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread -DWIBOX_AUDIO_HW_TEST \
			-Isrc/sip_media -Iinclude/adi tests/audio_hw_mock_test.c \
			src/sip_media/audio_hw.c -Wl,--wrap=open -Wl,--wrap=write \
			-Wl,--wrap=close -o "$$bin"; \
		"$$bin"

test-video-worker:
	@set -e; bin=/tmp/wibox_video_worker_mock_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -DWIBOX_VIDEO_WORKER_TEST \
			-Isrc/sip_media tests/video_worker_mock_test.c \
			src/sip_media/video_worker.c -o "$$bin"; \
		"$$bin"

test-h264-annexb:
	@set -e; bin=/tmp/wibox_h264_annexb_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Isrc/sip_media \
			tests/h264_annexb_test.c src/sip_media/h264_annexb.c \
			-Wl,--wrap=malloc -o "$$bin"; \
		"$$bin"

test-snapshot-file:
	@set -e; bin=/tmp/wibox_snapshot_file_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -Isrc/sip_media \
			tests/snapshot_file_test.c src/sip_media/snapshot_file.c \
			-Wl,--wrap=open -Wl,--wrap=write -Wl,--wrap=fsync \
			-Wl,--wrap=close -Wl,--wrap=rename -o "$$bin"; \
		"$$bin"

test-firmware-update:
	@set -e; bin=/tmp/wibox_firmware_update_mock_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread -Isrc \
			tests/firmware_update_mock_test.c \
			-Wl,--wrap=open -Wl,--wrap=write -Wl,--wrap=close \
			-Wl,--wrap=ioctl -Wl,--wrap=umount2 -Wl,--wrap=fsync \
			-Wl,--wrap=reboot -o "$$bin"; \
		"$$bin"

test-hardware-watchdog:
	@set -e; bin=/tmp/wibox_hardware_watchdog_mock_test; \
		trap 'rm -f "$$bin"' EXIT; \
		$(CC) -Wall -Wextra -Werror -std=gnu99 -pthread -Isrc/sip_media \
			tests/hardware_watchdog_mock_test.c src/sip_media/hardware_watchdog.c \
			-Wl,--wrap=open -Wl,--wrap=fcntl -Wl,--wrap=ioctl \
			-Wl,--wrap=write -Wl,--wrap=close -Wl,--wrap=clock_gettime \
			-o "$$bin"; \
		"$$bin"

test-app-watchdog:
	sh tests/app_watchdog_integration_test.sh

test-runtime-contract:
	python3 tests/runtime_contract_test.py

test-spec-coverage:
	python3 scripts/verify_spec_test_coverage.py

test-device-contract:
	sh -n tests/device_acceptance.sh

test-watchdog:
	sh tests/watchdog_defaults_test.sh

test-wifi-portal:
	sh tests/wifi_portal_test.sh

coverage:
	python3 tests/code_coverage.py

verify: test verify-image

verify-image:
	@scripts/verify_image.sh

deploy-runtime: build-media
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/deploy_runtime.sh

verify-device:
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/verify_device.sh

verify-runtime:
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/verify_runtime.sh

verify-mqtt:
	scripts/verify_mqtt.py

device-status:
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/device_status.sh

build-inside: extract patch pack

extract:
	rm -rf $(BUILD_DIR)
	cramfsck -x $(BUILD_DIR) $(BASE_IMAGE)

patch:
	@for PATCH in scripts/??_*.sh; do \
		echo ">> $$PATCH"; \
		ROOTFS=$(BUILD_DIR) sh $$PATCH; \
		echo "----"; \
	done | tee -a patch.log
	@touch $(BUILD_DIR)/patched

pack:
	rm -f $(BUILD_DIR)/patched 2>/dev/null
	mkdir -p release
	mkcramfs -e 0 -v $(BUILD_DIR) release/image-$(DATE)
	ln -sf image-$(DATE) release/latest
	@echo ""
	@echo "=== BUILD DONE ==="
	@ls -la release/latest
	@md5sum release/image-$(DATE)

clean:
	rm -f include/bin/dbclient include/bin/ipctool include/bin/scp
	rm -f include/etc/wibox-release
	rm -f src/sip_media/sip_media src/sip_media/wibox-media-daemon src/sip_media/*.o
	@if docker image inspect $(BUILD_IMAGE) >/dev/null 2>&1; then \
		docker run --rm -v $(PWD):/build $(BUILD_IMAGE) bash -lc "rm -rf /build/$(BUILD_DIR) /build/.verify-image-root /build/patch.log /build/release /build/include/sbin"; \
	else \
		rm -rf $(BUILD_DIR) .verify-image-root patch.log release include/sbin 2>/dev/null || true; \
	fi

help:
	@echo "WiBox Media"
	@echo ""
	@echo "Common:"
	@echo "  make docker          Build the local firmware build image"
	@echo "  make build           Build media binaries and release/latest"
	@echo "  make build-media     Build wibox-media-daemon and firmware_update only"
	@echo "  make test            Run all host integration and E2E tests"
	@echo "  make test-call-flow  Run the call workflow scenario matrix"
	@echo "  make coverage        Measure line/branch coverage and enforce thresholds"
	@echo "  make verify          Run local tests and verify release/latest"
	@echo "  make verify-image    Inspect release/latest contents"
	@echo "  make clean           Remove local build artifacts"
	@echo ""
	@echo "Device development:"
	@echo "  make deploy-runtime  Run current daemon from /tmp on a WiBox"
	@echo "  make verify-device   Verify active runtime and MQTT against a WiBox"
	@echo "  make device-status   Show process, config and recent daemon log"
	@echo ""
	@echo "Variables:"
	@echo "  WIBOX_IP=$(WIBOX_IP)"
	@echo "  WIBOX_USER=$(WIBOX_USER)"
	@echo "  WIBOX_PASS=<hidden>"
	@echo ""
	@echo "First install and recovery are documented in docs/getting_started.md and docs/recovery.md."
	@echo "Routine upgrades use /usr/bin/firmware_update or Home Assistant."
