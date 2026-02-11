# NCS Migration Validation Checklist

Use one copy of this checklist per migration step (example: `v2.5.2 -> v2.6.0`).

## Step Metadata
- From version:
- To version:
- Date:
- Tester:
- Branch:
- Commit:

## Build and Tooling
- [ ] `west update` completed without manifest conflicts.
- [ ] Debug build succeeds.
- [ ] Production build succeeds.
- [ ] Flash succeeds on target hardware.
- [ ] Serial/RTT logs accessible (as expected for selected config).

## Zigbee Core Behavior
- [ ] Device joins network successfully.
- [ ] Device rejoins after reboot.
- [ ] Sleepy end-device behavior remains stable.
- [ ] Reporting reaches coordinator/Home Assistant.
- [ ] No repeated leave/join loops.

## Sensor and Battery
- [ ] SHTC3 values update and look sane.
- [ ] SCD4x values update and look sane (if hardware variant includes CO2 sensor).
- [ ] Battery voltage reading updates and is plausible.
- [ ] Measurement intervals match config expectations.

## Power and Stability
- [ ] No unexpected wakeups observed.
- [ ] Average current remains within acceptable delta.
- [ ] No crash/reset over extended runtime test.

## Regression Notes
- Observed behavior changes:
- Known limitations introduced:
- Workarounds used:

## Decision
- [ ] PASS: proceed to next migration step.
- [ ] BLOCKED: fix required before proceeding.
- [ ] ROLLBACK: return to last validated version.

## Follow-up Tasks
- [ ] Update `MIGRATION_STRATEGY.md` with findings if needed.
- [ ] Create issues for blockers/regressions.
