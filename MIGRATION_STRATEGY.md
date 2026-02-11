# NCS Migration Strategy (Schneggi Sensor)

## Current State
- Current SDK: `nRF Connect SDK v2.5.2` (from `west.yml`).
- Product profile: Zigbee sleepy end device on `nRF52840`, I2C sensors (`SHTC3`, `SCD4x`), low-power focus.
- Key constraint: Zigbee support changes significantly in NCS 3.x (moved to add-ons).

## Migration Goals
- Keep Zigbee behavior stable (join, reporting, sleepy end-device behavior).
- Preserve low-power performance and battery reporting.
- Reduce migration risk by separating platform/tooling changes from protocol-stack changes.

## Recommended Path

### Phase 1: Stabilize on late 2.x (recommended first target: `v2.9.2`)
- Upgrade incrementally from `v2.5.2` to a late 2.x release.
- Keep Zigbee in-tree during this phase to minimize architecture churn.
- At each step:
  - Update `west.yml` `sdk-nrf` revision.
  - Build debug + production.
  - Flash and validate core runtime behavior.
- Why: this captures many fixes while avoiding immediate Zigbee add-on migration complexity.

### Phase 2: Decide 3.x strategy (Go/No-Go gate)
- Evaluate whether upgrading to 3.x is worth Zigbee add-on migration effort for this product.
- If yes:
  - Migrate tooling and flashing workflow (nRF Util defaults, current installation flow).
  - Integrate Zigbee add-on model required by 3.x.
  - Re-run full protocol and power validation.
- If no:
  - Stay pinned on validated late 2.x baseline and backport only necessary fixes.

### Phase 3: Optional 3.x migration (target latest patch, currently `v3.2.1`)
- Perform only after a successful add-on prototype branch.
- Land in stages:
  - Tooling/build changes.
  - Zigbee add-on integration.
  - Runtime tuning and power regressions.

## Validation Gates (must pass)

### Build/Tooling
- `debug` and `production` builds are reproducible.
- Flashing works consistently with selected runner/tooling.

### Zigbee Functional
- Network join/rejoin reliability.
- Attribute reporting cadence unchanged (or intentionally changed).
- Sleepy end-device polling and parent retention behavior.
- OTA/FOTA path (if used) still operational.

### Sensor/Data
- `SHTC3` and `SCD4x` readings valid and stable.
- Battery voltage sampling and reporting still correct.

### Power
- Idle/sleep current within acceptable delta of current baseline.
- Wake/sampling/report cycles do not introduce drift in average current.

## Risk Register
- Zigbee stack migration risk in 3.x: **high**.
- Tooling and runner behavior differences (`west flash` + nRF Util): **medium**.
- Sensor-driver/API changes across Zephyr/NCS updates: **medium**.
- Hidden board/devicetree regressions: **medium**.

## Branching and Execution Plan
- Keep this strategy work on: `chore/ncs-migration-strategy`.
- Create execution branches:
  - `migration/ncs-2.9.x`
  - `migration/ncs-3.x-prototype`
- Merge only when each validation gate is green and measured.

## Rollback Plan
- If any phase fails critical Zigbee stability or power targets:
  - Stop forward migration.
  - Revert to last validated SDK tag branch.
  - Open a focused fix branch for blockers before retrying.
