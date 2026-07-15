# Installation and Recovery Specification

## Purpose

Define safe first installation and progressively more invasive recovery paths
for the constrained WiBox flash layout.

## Requirements

### Requirement: Factory backup before first flash

Operators SHALL back up factory MTD partitions before replacing `mtd4`, with at
least the original `/usr` image preserved off-device.

#### Scenario: A stock device is prepared

- GIVEN shell or serial access to stock firmware
- WHEN first installation is planned
- THEN `mtd0` through `mtd6` can be copied to a computer
- AND the backup is completed before writing custom firmware

### Requirement: Persistent WiFi before first flash

`/mnt/mtd/wpa_supplicant.conf` SHALL be created and checked before first custom
flash because loss of WiFi may otherwise require serial recovery.

#### Scenario: WiFi config is absent

- GIVEN a stock device has no persistent WiFi configuration
- WHEN installation preparation is reviewed
- THEN flashing is deferred until WiFi is configured or serial recovery is available

### Requirement: Version-aware shell access

Stock B007 and B010 MAY use telnet when available. B013 and unverified newer
firmware SHALL be treated as serial-only for initial access.

#### Scenario: Stock B013 is installed

- GIVEN the stock version blocks telnet
- WHEN an operator needs a shell
- THEN the documented TTL console path is used

### Requirement: Verified first image transfer

The first custom image SHALL be downloaded on a capable computer, transferred
to `/tmp`, and checked by MD5 before writing `mtd4`.

#### Scenario: Network first install

- GIVEN stock `wget` cannot fetch the GitHub asset
- WHEN the image is transferred with netcat
- THEN computer and device checksums are compared before flash

### Requirement: Partition-size safety

Release images SHALL fit the `mtd4` capacity of `0x00b10000` bytes.

#### Scenario: An image is too large

- GIVEN an image exceeds the `/usr` partition
- WHEN build or recovery validation checks its size
- THEN the image is rejected

### Requirement: Least-invasive recovery order

Recovery SHALL prefer, in order, custom-firmware SSH/updater, Linux shell with
network, Linux shell repair without network, and U-Boot serial recovery.

#### Scenario: Custom firmware still has SSH

- GIVEN Linux, networking and SSH work
- WHEN recovery is required
- THEN the verified on-device updater is used instead of U-Boot

#### Scenario: Linux does not provide a shell

- GIVEN normal boot cannot reach shell access
- WHEN recovery is required
- THEN the serial U-Boot YMODEM erase/write procedure is used for `mtd4`

### Requirement: Post-recovery checks

After recovery, operators SHALL verify release metadata, cramfs/kernel errors,
daemon logs and network state as applicable.

#### Scenario: Recovery boot completes

- GIVEN a known-good image was restored
- WHEN Linux boots
- THEN installed release metadata and daemon health are checked
