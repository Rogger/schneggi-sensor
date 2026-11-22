# Battery power management

The non-CO2 debug and production profiles enable I2C runtime power management.
The application enables it on the sensor bus; the TWIM driver suspends the
controller and applies its sleep pin configuration between transfers.
Polling, sampling intervals, and SHTC3 measurement accuracy are unchanged.

Failed SHTC3 fetches attempt a sleep command after a 13 ms settling delay,
working around the SDK driver's missing error-path cleanup without changing
the shared SDK. Cleanup is best-effort: an inaccessible sensor cannot be forced
to sleep. The original fetch error is retained and no failed sample is reported.
The command and conversion timing follow the
[SHTC3 datasheet](https://sensirion.com/file/datasheet_shtc3).

Battery-profile rejoin delays grow from 1 second exponentially to a one-hour
cap (1, 2, 4, ..., 1024, 2048, 3600 seconds). Successful reconnection resets the
backoff. After prolonged outages, recovery may therefore wait up to an hour
before the next attempt, plus commissioning time. USB-powered CO2 profiles keep
their 15-minute cap and do not enable I2C runtime PM or battery monitoring.

Battery measurements run hourly in debug builds and daily in production builds.
If sampling or updating either battery attribute fails, the firmware retries on
the next sensor cycle (one or ten minutes respectively). A successful update
starts the next battery interval. The firmware attempts to disable the divider
on every path and logs cleanup failures.

The firmware restarts if Zigbee startup or scheduling a required measurement,
rejoin, or watchdog alarm fails, so it cannot stay running indefinitely without
that work. Ordinary join failures still use the rejoin backoff described above.
Alarm replacement removes previous instances and uses overflow-safe conversion
for the full supported sampling range of 1–1440 minutes. Repeated Identify
requests keep one blink alarm; scheduling failure stops the LED animation.

Validate on hardware before relying on battery-life estimates: compare at least
an hour of production-profile current capture before/after, with the debugger
disconnected; check repeated sensor reads, transient I2C failures, and recovery
after a prolonged coordinator outage. Host tests cannot verify physical sleep
current or bus suspend/resume behavior.
