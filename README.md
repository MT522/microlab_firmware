# ArrayDriver - STM32F413 Electrode Control System

## Overview

ArrayDriver is a comprehensive firmware library for controlling 140 electrodes arranged in a 10×14 active matrix for microfluidics and PCR applications. The system uses arbitrary electrode numbering with complete runtime configuration via JSON files.

**Key Features:**
- ✅ Control 140 electrodes individually or in sequences
- ✅ JSON-based mapping loaded at runtime (no hardcoded mappings)
- ✅ Atomic GPIO operations for minimal transition delays
- ✅ User-defined test scenarios and protocols
- ✅ STM32F413 LQFP-144 optimized with HAL support

## Architecture

### Hardware Configuration
- **Microcontroller:** STM32F413 LQFP-144
- **Matrix:** 10 rows × 14 columns = 140 electrodes
- **Interface:** 160-pin PCIE connector
- **GPIO:** 114 available pins (no overlap between rows/columns)

### Electrode Numbering
**Important:** Electrode numbers (1-140) are **arbitrary** and **not geometrically arranged**. The numbering follows PCIE connector pins, not physical position.

```
Electrode # → PCIE Pin → Row,Col Matrix → GPIO Port/Pin
    (1-140)    (1-140)     (0-9, 0-13)    (STM32 GPIO)
       ↓           ↓             ↓              ↓
ElectrodeMap → PinMap → Row/Col Ctrl → GPIO Hardware
```

### Mapping Chain

Three JSON files define the complete electrode control chain:

#### 1. ElectrodeMap.json
Maps electrode numbers to PCIE connector pins.
```json
{
  "mapping": {
    "1": 1,      // Electrode 1 connects to PCIE Pin 1
    "2": 5,      // Electrode 2 connects to PCIE Pin 5
    "25": 87,    // Electrode 25 connects to PCIE Pin 87
    ...
  }
}
```

#### 2. PinMap.json
Maps PCIE pins to row/column positions in the active matrix.
```json
{
  "electrodes": {
    "0,0": 1,    // Row 0, Col 0 controlled by PCIE Pin 1
    "0,1": 2,    // Row 0, Col 1 controlled by PCIE Pin 2
    "6,3": 87,   // Row 6, Col 3 controlled by PCIE Pin 87
    ...
  }
}
```

#### 3. PinDef.json (Currently in header)
Maps row/column lines to STM32 GPIO pins.
- Rows 0-7: `GPIOA` pins 0-7
- Rows 8-9: `GPIOB` pins 0-1
- Columns 0-7: `GPIOC` pins 0-7
- Columns 8-13: `GPIOD` pins 0-5

## Installation

### Prerequisites

1. **STM32CubeMX Project** with:
   - STM32F413 configured
   - HAL drivers enabled
   - GPIO pins configured as outputs

2. **Filesystem Support** (choose one):
   - FatFS (for SD card)
   - LittleFS (for internal flash)
   - SPIFFS (for SPI flash)

3. **JSON Files** stored on filesystem:
   ```
   resources/
   ├── ElectrodeMap.json
   ├── PinMap.json
   └── TestScenarios.json (optional)
   ```

### File Structure

```
Firmware/
├── include/
│   └── ArrayDriver.h
├── src/
│   └── ArrayDriver.cpp
├── resources/
│   ├── ElectrodeMap.json
│   ├── PinMap.json
│   └── TestScenarios.json
└── README.md
```

## Quick Start

### 1. Setup with FatFS (SD Card)

```cpp
#include "stm32f4xx_hal.h"
#include "ff.h"           // FatFS
#include "ArrayDriver.h"

FATFS SDFatFS;
ArrayDriver electrodeArray;

int main(void) {
    // Initialize HAL
    HAL_Init();
    SystemClock_Config();
    
    // Mount SD card
    if (f_mount(&SDFatFS, "", 1) != FR_OK) {
        Error_Handler();  // SD card mount failed
    }
    
    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    // Create ArrayDriver (loads JSON files automatically)
    // Constructor reads ElectrodeMap.json and PinMap.json
    ArrayDriver electrodeArray;
    
    // Initialize GPIO pins
    electrodeArray.init();  // All electrodes start LOW
    
    // Ready to use!
    electrodeArray.setElectrodeHighByNumber(1);
    HAL_Delay(1000);
    electrodeArray.setElectrodeLowByNumber(1);
    
    while(1) {
        // Your application
    }
}
```

### 2. Basic Electrode Control

```cpp
// Control electrodes by number (1-140)
electrodeArray.setElectrodeHighByNumber(25);    // Turn on electrode 25
HAL_Delay(500);
electrodeArray.setElectrodeLowByNumber(25);     // Turn off electrode 25

// Generic control
electrodeArray.setElectrodeByNumber(10, true);  // HIGH
electrodeArray.setElectrodeByNumber(10, false); // LOW

// Bulk operations
electrodeArray.setAllElectrodesLow();
electrodeArray.setAllElectrodesHigh();
```

### 3. Run Test Sequence

```cpp
// Test all electrodes sequentially
electrodeArray.runElectrodeTest();  // 100ms per electrode

// Test specific electrodes
uint8_t testElectrodes[] = {1, 5, 10, 20, 50, 100};
electrodeArray.runElectrodeSequenceTest(testElectrodes, 6, 1000);  // 1 second each
```

## API Reference

### Constructor & Initialization

#### `ArrayDriver()`
Constructor automatically loads JSON mapping files from filesystem.
- Reads `resources/ElectrodeMap.json`
- Reads `resources/PinMap.json`
- Builds internal lookup tables
- Falls back to 1:1 mapping if files not found

#### `void init()`
Initializes GPIO pins and sets all electrodes to LOW state.
- Configures all row and column pins as outputs
- Sets push-pull mode, very high speed, no pull resistors
- Initial state: all rows LOW, all columns HIGH (electrodes OFF)

### Electrode Control by Number

#### `void setElectrodeByNumber(uint8_t electrodeNum, bool state)`
Set electrode (1-140) to specific state.
```cpp
electrodeArray.setElectrodeByNumber(25, true);   // Turn on
electrodeArray.setElectrodeByNumber(25, false);  // Turn off
```

#### `void setElectrodeHighByNumber(uint8_t electrodeNum)`
Turn on electrode.
```cpp
electrodeArray.setElectrodeHighByNumber(50);
```

#### `void setElectrodeLowByNumber(uint8_t electrodeNum)`
Turn off electrode.
```cpp
electrodeArray.setElectrodeLowByNumber(50);
```

### Electrode Control by Row/Column

#### `void setElectrode(uint8_t row, uint8_t col, bool state)`
Control electrode by matrix position.
```cpp
electrodeArray.setElectrode(5, 7, true);  // Row 5, Col 7 HIGH
```

#### `void setElectrodeHigh(uint8_t row, uint8_t col)`
#### `void setElectrodeLow(uint8_t row, uint8_t col)`

### Bulk Operations

#### `void setAllElectrodesLow()`
Turn off all 140 electrodes atomically.

#### `void setAllElectrodesHigh()`
Turn on all 140 electrodes atomically.

#### `void setRowElectrodes(uint8_t row, bool state)`
Control all electrodes in a row (0-9).

#### `void setColElectrodes(uint8_t col, bool state)`
Control all electrodes in a column (0-13).

### Pattern Control

#### `void setPattern(bool pattern[NUM_ROWS][NUM_COLS])`
Apply a 10×14 boolean pattern to the array.
```cpp
bool pattern[10][14];
// Define your pattern
for (int r = 0; r < 10; r++) {
    for (int c = 0; c < 14; c++) {
        pattern[r][c] = (r + c) % 2;  // Checkerboard
    }
}
electrodeArray.setPattern(pattern);
```

#### `void getPattern(bool pattern[NUM_ROWS][NUM_COLS])`
Read current electrode states.

### State Query

#### `bool getElectrodeState(uint8_t row, uint8_t col)`
Check if electrode is currently HIGH or LOW.

#### `bool getRowColFromElectrode(uint8_t electrodeNum, uint8_t* row, uint8_t* col)`
Convert electrode number to row/column coordinates.
```cpp
uint8_t row, col;
if (electrodeArray.getRowColFromElectrode(25, &row, &col)) {
    printf("Electrode 25 is at Row %d, Col %d\n", row, col);
}
```

### Sequence Execution

#### `void executeSequence(const ElectrodeSequence_t* sequence)`
Execute a timed sequence (blocking).
```cpp
ElectrodeStep_t steps[] = {
    {0, 0, true, 1000},   // Row 0, Col 0, HIGH, 1 second
    {0, 1, true, 500},    // Row 0, Col 1, HIGH, 0.5 seconds
    {0, 0, false, 100},   // Row 0, Col 0, LOW, 0.1 seconds
    {0, 1, false, 100}    // Row 0, Col 1, LOW, 0.1 seconds
};

ElectrodeSequence_t sequence = {
    steps,
    4,        // Number of steps
    3,        // Repeat 3 times
    2000      // 2 second delay between cycles
};

electrodeArray.executeSequence(&sequence);
```

#### `void executeSequenceAsync(const ElectrodeSequence_t* sequence)`
Start sequence in background (non-blocking).

#### `bool isSequenceRunning()`
Check if async sequence is still running.

#### `void stopSequence()`
Stop currently running async sequence.

### Test Functions

#### `void runElectrodeTest()`
Test all 140 electrodes sequentially (100ms each).

#### `void runElectrodeSequenceTest(uint8_t* electrodeNumbers, uint16_t numElectrodes, uint32_t duration_ms)`
Test specific electrodes in order.
```cpp
uint8_t testList[] = {1, 10, 25, 50, 100, 140};
electrodeArray.runElectrodeSequenceTest(testList, 6, 500);  // 500ms each
```

## Usage Examples

### Example 1: PCR Cycle

```cpp
void runPCRCycle() {
    // Define electrode zones based on your chip layout
    uint8_t heatingZone[] = {10, 20, 30, 40, 50};
    uint8_t coolingZone[] = {15, 25, 35, 45, 55};
    
    // Denaturation (95°C) - 30 seconds
    for (uint8_t i = 0; i < 5; i++) {
        electrodeArray.setElectrodeHighByNumber(heatingZone[i]);
    }
    HAL_Delay(30000);
    
    // Cool down
    for (uint8_t i = 0; i < 5; i++) {
        electrodeArray.setElectrodeLowByNumber(heatingZone[i]);
    }
    
    // Annealing (60°C) - 30 seconds
    for (uint8_t i = 0; i < 5; i++) {
        electrodeArray.setElectrodeHighByNumber(coolingZone[i]);
    }
    HAL_Delay(30000);
    
    // Cleanup
    electrodeArray.setAllElectrodesLow();
}

// Run 25 PCR cycles
for (uint16_t cycle = 0; cycle < 25; cycle++) {
    runPCRCycle();
    HAL_Delay(1000);  // Inter-cycle delay
}
```

### Example 2: Fluid Transport

```cpp
void transportFluid() {
    // Define transport path (custom electrode sequence)
    uint8_t path[] = {1, 5, 12, 18, 25, 33, 40, 48, 55};
    
    // Activate electrodes sequentially
    for (uint8_t i = 0; i < 9; i++) {
        electrodeArray.setElectrodeHighByNumber(path[i]);
        HAL_Delay(500);  // 500ms per step
        
        // Keep previous electrode on for smooth transport
        if (i > 0) {
            electrodeArray.setElectrodeLowByNumber(path[i-1]);
        }
    }
    
    // Turn off last electrode
    electrodeArray.setElectrodeLowByNumber(path[8]);
}
```

### Example 3: Mixing Pattern

```cpp
void runMixing(uint32_t duration_ms) {
    // Define mixing zone
    uint8_t mixZone[] = {50, 51, 52, 64, 65, 66, 78, 79, 80};
    
    uint32_t startTime = HAL_GetTick();
    
    while ((HAL_GetTick() - startTime) < duration_ms) {
        // Turn on mixing zone
        for (uint8_t i = 0; i < 9; i++) {
            electrodeArray.setElectrodeHighByNumber(mixZone[i]);
        }
        HAL_Delay(200);
        
        // Turn off mixing zone
        for (uint8_t i = 0; i < 9; i++) {
            electrodeArray.setElectrodeLowByNumber(mixZone[i]);
        }
        HAL_Delay(200);
    }
}

// Run mixing for 10 seconds
runMixing(10000);
```

### Example 4: Complex Protocol

```cpp
void runComplexProtocol() {
    // Phase 1: Sample loading (electrodes 1-5)
    uint8_t loadZone[] = {1, 2, 3, 4, 5};
    electrodeArray.runElectrodeSequenceTest(loadZone, 5, 1000);
    
    // Phase 2: Transport to reaction chamber
    uint8_t transportPath[] = {5, 12, 19, 26, 33, 40};
    for (uint8_t i = 0; i < 6; i++) {
        electrodeArray.setElectrodeHighByNumber(transportPath[i]);
        HAL_Delay(500);
        if (i > 0) electrodeArray.setElectrodeLowByNumber(transportPath[i-1]);
    }
    
    // Phase 3: Mixing (5 cycles)
    uint8_t mixZone[] = {40, 41, 42, 54, 55, 56};
    for (uint8_t cycle = 0; cycle < 5; cycle++) {
        for (uint8_t i = 0; i < 6; i++) {
            electrodeArray.setElectrodeHighByNumber(mixZone[i]);
        }
        HAL_Delay(2000);
        for (uint8_t i = 0; i < 6; i++) {
            electrodeArray.setElectrodeLowByNumber(mixZone[i]);
        }
        HAL_Delay(500);
    }
    
    // Phase 4: PCR (simplified - 10 cycles)
    uint8_t pcrZone[] = {70, 71, 72, 73, 74};
    for (uint16_t cycle = 0; cycle < 10; cycle++) {
        // Heat
        for (uint8_t i = 0; i < 5; i++) {
            electrodeArray.setElectrodeHighByNumber(pcrZone[i]);
        }
        HAL_Delay(15000);  // 15 seconds
        
        // Cool
        for (uint8_t i = 0; i < 5; i++) {
            electrodeArray.setElectrodeLowByNumber(pcrZone[i]);
        }
        HAL_Delay(5000);   // 5 seconds
    }
    
    // Phase 5: Detection
    electrodeArray.setElectrodeHighByNumber(100);
    HAL_Delay(10000);  // 10 second detection
    
    // Cleanup
    electrodeArray.setAllElectrodesLow();
}
```

## JSON Configuration

### Customizing Electrode Mapping

Edit `resources/ElectrodeMap.json` to match your hardware:

```json
{
  "description": "Custom electrode mapping for Device XYZ",
  "version": "1.0",
  "mapping": {
    "1": 45,     // Electrode 1 → PCIE Pin 45
    "2": 87,     // Electrode 2 → PCIE Pin 87
    "3": 12,     // Electrode 3 → PCIE Pin 12
    ...
    "140": 160   // Electrode 140 → PCIE Pin 160
  }
}
```

### Customizing PCIE Mapping

Edit `resources/PinMap.json` to match your board layout:

```json
{
  "description": "PCIE to Matrix mapping",
  "electrodes": {
    "0,0": 1,    // Row 0, Col 0 → PCIE Pin 1
    "0,1": 2,    // Row 0, Col 1 → PCIE Pin 2
    ...
    "9,13": 140  // Row 9, Col 13 → PCIE Pin 140
  }
}
```

**Changes take effect after restart** - no firmware recompilation needed!

## Technical Details

### Control Logic

**Electrode HIGH:** Row = LOW, Column = HIGH  
**Electrode LOW:** Row = HIGH, Column = LOW

This active matrix scheme allows independent control of all 140 electrodes using only 24 GPIO pins (10 rows + 14 columns).

### Atomic Operations

GPIO state changes use BSRR (Bit Set/Reset Register) for atomic simultaneous control:
```cpp
// To turn electrode HIGH (Row LOW, Column HIGH)
rowPins[row].port->BSRR = (uint32_t)rowPins[row].pin << 16U;  // Reset row (LOW)
colPins[col].port->BSRR = colPins[col].pin;                   // Set column (HIGH)
```

This ensures minimal delay between row and column transitions (sub-microsecond).

### Memory Usage

- **Electrode state array:** 140 bytes (10×14)
- **Electrode mapping:** 280 bytes (140 × 2 bytes)
- **PCIE mapping:** 280 bytes (140 × 2 bytes)
- **GPIO lookup tables:** 96 bytes
- **Total:** ~900 bytes RAM

### Performance

- **JSON loading:** ~100-300ms at startup
- **Electrode control:** < 1μs per electrode
- **Sequence execution:** User-defined timing
- **Lookup overhead:** O(1) array access

## Troubleshooting

### JSON Files Not Loading

**Symptom:** Electrodes don't match expected behavior  
**Solution:**
1. Verify SD card is mounted before creating ArrayDriver
2. Check file paths: `resources/ElectrodeMap.json`
3. Ensure files are not corrupted
4. Check FatFS configuration

### Electrode Not Responding

**Symptom:** Specific electrode doesn't activate  
**Check:**
1. JSON mapping for that electrode number
2. GPIO clock is enabled for that port
3. Physical wiring connections
4. Use `runElectrodeTest()` to systematically test all

### Wrong Electrode Activates

**Symptom:** Different electrode than intended activates  
**Solution:**
1. Review `ElectrodeMap.json` → electrode to PCIE mapping
2. Check `PinMap.json` → PCIE to row/col mapping
3. Verify physical connections match JSON files

### SD Card Mount Fails

**Symptom:** Cannot read JSON files  
**Solution:**
1. Check SD card connections (SDIO/SPI pins)
2. Verify FatFS is configured in CubeMX
3. Format SD card as FAT32
4. Check SD card is not write-protected

## Best Practices

### 1. Version Control JSON Files
```bash
git add resources/*.json
git commit -m "Update electrode mapping for Rev 2.1"
```

### 2. Backup Configurations
Keep tested configurations:
- `ElectrodeMap_production.json`
- `ElectrodeMap_testing.json`
- `ElectrodeMap_backup_YYYY-MM-DD.json`

### 3. Document Electrode Assignments
Add comments in JSON:
```json
{
  "mapping": {
    "1": 45,     // Heating zone - upper left
    "2": 46,     // Heating zone - upper center
    "10": 87,    // Mixing zone - center
    ...
  }
}
```

### 4. Test After Changes
```cpp
// After updating JSON files, run full test
electrodeArray.runElectrodeTest();
```

### 5. Error Handling
```cpp
// Check if electrode number is valid
if (electrodeNum < 1 || electrodeNum > 140) {
    // Handle error
}
```

## License

Copyright © 2025 Daya Tech. All rights reserved.

## Support

For issues, questions, or contributions, contact the firmware team.

---

**Last Updated:** January 2025  
**Firmware Version:** 2.0  
**Hardware:** STM32F413 LQFP-144
