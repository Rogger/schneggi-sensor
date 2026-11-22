# Reset diagnostics

Measurements start when the Zigbee stack starts, not only after a successful
join, so the sensor attributes hold real values even if no join signal follows
a reset. Joining still triggers an immediate measurement.

The Basic cluster's Location Description (`0x0010`) on endpoint 1 holds the
last reset and the number of resets since power-on, for example `wdt n3`. A
battery sensor polls its parent only every 2 minutes, so an on-demand read from
ZHA usually times out. Instead the sensor sends the value to the coordinator as
an attribute report once per boot, as soon as it is joined, and retries with each
measurement until the report is delivered. zigpy caches the reported value: in
Home Assistant open the device, choose **⋮ → Download diagnostics**, and search
the file for `location_desc`. No re-interview is required. The USB-powered CO2
sensor sends the same report and also answers **⋮ → Manage Zigbee device →
Basic → location_desc → Read attribute**.

| Text | Cause |
| --- | --- |
| `power` | Power-on or brownout (counter restarts at 0) |
| `wdt` | Hardware watchdog: the Zigbee scheduler stalled for 120 s |
| `lockup` | CPU lockup |
| `pin` | Reset pin or programmer |
| `debug` | Debugger reset |
| `unknown` | Reset proven by the counter, but no hardware cause recorded |
| `sw<code>` | Software reset with the code below |

| Code | `app_reboot()` reason |
| --- | --- |
| 0 | Not `app_reboot()`, e.g. a Zephyr fatal error |
| 1 | OTA storage or watchdog initialization failed |
| 2 | Zigbee startup or commissioning failed |
| 3 | Measurement alarm could not be scheduled |
| 4 | Rejoin retry alarm could not be scheduled |
| 5 | Watchdog service alarm could not be scheduled |
| 6 | Trial image failed its startup checks |
| 7 | OTA transfer stalled |
| 8 | OTA Upgrade End response missing |
| 9 | OTA image installed |
| 10 | Network leave during an OTA session |

The count (`n`, at most 15) and the code are kept in the nRF52840 `GPREGRET2`
register, which survives every reset except power-on and brownout. The value is
read once at boot; a later attribute write from ZHA is replaced at the next reset.
A sensor that is reset while it cannot reach the network reports the value after
it joins again.
