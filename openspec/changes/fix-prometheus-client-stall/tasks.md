# Tasks

## 1. Specify the Failure Contract

- [x] 1.1 Define bounded handling of idle or incomplete Prometheus clients.
- [x] 1.2 Define monotonic, non-negative daemon uptime.

## 2. Implement the Exporter Fix

- [x] 2.1 Add a receive deadline to accepted exporter clients.
- [x] 2.2 Replace wall-clock uptime subtraction with monotonic elapsed time.

## 3. Add Regression Coverage

- [x] 3.1 Verify an idle client cannot indefinitely block `/healthz`.
- [x] 3.2 Verify backwards wall-clock movement cannot produce negative uptime.

## 4. Verify and Deploy

- [x] 4.1 Run exporter tests and repository verification appropriate to the change.
- [x] 4.2 Build and deploy the volatile development runtime to the approved WiBox.
- [x] 4.3 Verify `/healthz`, `/metrics` and idle-client recovery on the device.
