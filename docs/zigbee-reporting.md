# Zigbee endpoints and reporting

## CO2 variant: two temperature endpoints

CO2 firmware exposes two temperatures: SHTC3 ambient temperature on endpoint 1 and SCD40 temperature on endpoint 2. The SCD40 reading uses the same sample as CO2 and can be affected by self-heating and enclosure conditions; use SHTC3 as the primary ambient reading. The existing SHTC3 humidity entity is unchanged.

After upgrading CO2 firmware to a build with the second temperature endpoint,
Home Assistant ZHA must discover endpoint 2. Re-interview/reconfigure the device;
if the endpoint list remains cached, remove and pair the device again. Both
temperature entities use the standard Temperature Measurement cluster, so no
custom quirk is required. You can rename them in HA to **Temperature SHTC3** and
**Temperature SCD40**; endpoint 1 is SHTC3 and endpoint 2 is SCD40. The new reading
uses the same sampling interval, 0.1 °C fallback change threshold, daily refresh,
and standard Configure Reporting support as the existing temperature reading.

## Reporting configuration

ZHA can configure the standard minimum interval, maximum interval, and
reportable change for endpoint 1's Temperature Measurement (`0x0402`) and
Relative Humidity Measurement (`0x0405`) measured-value (`0x0000`) attributes.
Their changes are in hundredths of a degree Celsius and hundredths of a percent
relative humidity. For settings that differ from ZBOSS's defaults, ZBOSS applies
them to every new sensor reading. Without a configuration, or after reporting
is restored to defaults, the firmware retains its 0.1 °C / 1% change thresholds
and 24-hour refresh. A request for ZBOSS's exact default settings (5-second
minimum, no periodic maximum, zero change) is indistinguishable from a reset
and also uses the firmware fallback.

The CO2 Concentration Measurement (`0x040D`) measured value is refreshed on
every sample, so its Zigbee minimum and maximum reporting intervals can be
configured. ZBOSS treats the single-precision CO2 value as a
changed/unchanged attribute and does not apply a numeric reportable-change
threshold to it. The sensor still samples every minute in debug builds and
every ten minutes in production builds; configuring a shorter reporting
interval does not produce a newer measurement.

## Repair of manufacturer-specific reporting slots

CO2 sensors upgraded to the endpoint-2 firmware were seen sending empty
Report Attributes frames for endpoint 2 several times per second, flagged as
manufacturer-specific with code `0x0000`. ZHA acknowledges each one; the backlog
of acknowledgements (thousands per sensor) delayed every other request to the
device by more than an hour, so OTA updates timed out. The firmware has no
manufacturer-specific attributes, so before every measurement it checks its
reporting slots: a slot with another manufacturer code is reset to the standard
code, and a slot for a missing attribute or a duplicate is released. Repairs are
logged and saved to NVRAM. After installing the fix, restart Home Assistant (or
reload ZHA) to drop the backlog that has already queued up.
