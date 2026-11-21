










# i2c-pulse-diag




Lightweight C11 command-line diagnostic utility for Linux I2C buses (/dev/i2c-X), multi-sensor register/bit-profile evaluation, statistical drift tracking, strict threshold exit codes, and JSON report generation.

## Supported Profiles
- raw: Unshifted 16-bit register reading (V).
- ina219: Bus voltage register format with 3-bit right shift and 0.004 V/LSB default scale (V).
- ads1115: 16-bit ADC reading with 0.000125 V/LSB default scale (V).
- tmp102: 12-bit temperature alignment (>> 4) with 0.0625 C/LSB default scale (C).
- mcp9808: 13-bit ambient temperature magnitude mask (0x1FFF) with 0.0625 C/LSB default scale (C).
- sht3x_t: Temperature evaluation profile in degrees Celsius (C).
- sht3x_h: Relative humidity evaluation profile in %RH.
- bme280_p: Barometric pressure evaluation profile in hPa.

## Build
make clean && make

## Usage
./i2c-pulse-diag --scan -b 1
./i2c-pulse-diag --sim 24960 --profile ina219 -n 3 -i 100 -o report.json
./i2c-pulse-diag --sim 32000 --profile ina219 --exit-code
./i2c-pulse-diag -b 1 -a 0x40 -r 0x02 --profile ina219 -n 5 -i 200 -o report.json

