# Testing strategy

WiBox Media uses several test layers because a host runner cannot honestly
reproduce the GK7102S analog front end, GADI drivers or MTD flash behavior.
Coverage numbers are regression gates, not a claim that hardware behavior is
perfect.

## Host layers

1. Pure C tests exercise state machines, parsers, range handling and concurrent
   call-session sequencing.
2. Integration tests use real local sockets for MQTT, RTSP and Prometheus while
   replacing only the remote broker or physical device boundary.
3. Hardware-contract tests replace UART, watchdog, GADI and MTD syscalls and
   verify exact frames, ioctl order, limits and failure handling.
4. Image and boot-contract tests inspect the generated cramfs and production
   scripts.
5. Device smoke tests remain necessary for analog audio, real camera content,
   Sofia warmup and actual watchdog/MTD behavior.

## Coverage gate

Run:

```bash
make coverage
```

The coverage runner uses GCC `gcov` line and branch instrumentation. Each listed
production module has its own minimum, so a well-covered small module cannot
hide a regression in another module. The initial gates are:

| Module | Lines | Branches |
|---|---:|---:|
| `call_session.c` | 95% | 75% |
| `config.c` | 90% | 65% |
| `mqtt.c` | 65% | 35% |
| `runtime_config.c` | 95% | 90% |
| `uart_protocol.c` | 95% | 90% |
| `sip_sdp.c` | 95% | 85% |
| `intercom.c` | 90% | 80% |
| `prometheus.c` | 75% | 55% |
| `rtsp_stream.c` | 60% | 40% |
| `audio_hw.c` | 85% | 70% |
| `video_worker.c` | 95% | 85% |
| `hardware_watchdog.c` | 75% | 55% |

These are floors, not targets. New host-testable modules are added as their
hardware boundaries are isolated. CI fails when coverage drops below a floor.

## Evidence rules

- A mocked test may prove orchestration, command bytes and failure recovery.
- It may not claim that an analog signal, camera sensor or kernel driver works.
- A source-text assertion is acceptable only for packaging or boot-script
  invariants, never as a substitute for runtime behavior.
- Every OpenSpec scenario must map to an automated host test, an image contract
  or an explicitly named device smoke test. The traceability validator rejects
  missing mappings.
