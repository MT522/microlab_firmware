# UART Command Handler - Serial Control Interface

## Overview

The UART Command Handler provides a serial interface for controlling the ArrayDriver remotely via UART/USB from a PC or other controller. All electrode operations can be performed through simple text-based commands.

## Hardware Setup

### STM32 Configuration
- **UART**: USART1, USART2, or USART6 (configurable)
- **Baud Rate**: 115200 (recommended)
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None

### Connections
- **TX**: STM32 TX → PC/USB RX
- **RX**: STM32 RX → PC/USB TX
- **GND**: Common ground

## Integration

### 1. Include Headers

```cpp
#include "ArrayDriver.h"
#include "UartCommandHandler.h"
```

### 2. Setup in main.c

```cpp
// Global objects
ArrayDriver electrodeArray;
UartCommandHandler cmdHandler(&electrodeArray, &huart1);  // Use your UART handle

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();  // Or your UART
    
    // Mount filesystem (for JSON files)
    f_mount(&SDFatFS, "", 1);
    
    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    // Initialize ArrayDriver
    electrodeArray.init();
    
    // Initialize UART command handler
    cmdHandler.init();
    
    while(1) {
        // Process commands
        if (cmdHandler.isCommandReady()) {
            cmdHandler.processCommands();
        }
        
        // Your other tasks
    }
}
```

### 3. UART Interrupt Handler

**Option A: Interrupt Mode (Recommended)**

```cpp
uint8_t rxByte;

// In your main function
HAL_UART_Receive_IT(&huart1, &rxByte, 1);

// UART callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart1) {
        cmdHandler.processByte(rxByte);
        HAL_UART_Receive_IT(&huart1, &rxByte, 1);  // Re-enable
    }
}
```

**Option B: Polling Mode**

```cpp
while(1) {
    uint8_t rxByte;
    if (HAL_UART_Receive(&huart1, &rxByte, 1, 10) == HAL_OK) {
        cmdHandler.processByte(rxByte);
    }
    
    if (cmdHandler.isCommandReady()) {
        cmdHandler.processCommands();
    }
}
```

## Command Protocol

### Command Format

Commands are text-based, terminated by newline (`\n` or `\r`). Fields are separated by pipe `|` delimiter.

### General Syntax
```
COMMAND|PARAM1|PARAM2|...|END
```

## Command Reference

### 1. Execute Sequence

**Format:**
```
START|REPS|DELAY|STEPS|ID1,DUR1|ID2,DUR2|...|IDN,DURN|END
```

**Parameters:**
- `REPS`: Number of cycle repetitions (1-1000)
- `DELAY`: Delay between cycles in milliseconds
- `STEPS`: Number of electrode steps
- `IDx`: Electrode number (1-140)
- `DURx`: Duration in milliseconds

**Example:**
```
START|5|1000|3|10,2000|25,1500|50,3000|END
```
This executes 5 cycles with 1 second delay between cycles:
- Electrode 10 ON for 2 seconds
- Electrode 25 ON for 1.5 seconds
- Electrode 50 ON for 3 seconds

**Response:**
```
Executing sequence...
OK
Sequence complete
```

### 2. Set Single Electrode

**Format:**
```
SET|ELECTRODE|STATE
```

**Parameters:**
- `ELECTRODE`: Electrode number (1-140)
- `STATE`: 0=LOW, 1=HIGH

**Examples:**
```
SET|25|1          # Turn on electrode 25
SET|25|0          # Turn off electrode 25
```

**Response:**
```
Electrode 25 set to HIGH
OK
```

### 3. Set All Electrodes

**Format:**
```
ALL|STATE
```

**Parameters:**
- `STATE`: 0=LOW (all off), 1=HIGH (all on)

**Examples:**
```
ALL|1             # Turn on all 140 electrodes
ALL|0             # Turn off all electrodes
```

**Response:**
```
All electrodes set to HIGH
OK
```

### 4. Set Row

**Format:**
```
ROW|ROW_NUM|STATE
```

**Parameters:**
- `ROW_NUM`: Row number (0-9)
- `STATE`: 0=LOW, 1=HIGH

**Example:**
```
ROW|5|1           # Turn on all electrodes in row 5
```

**Response:**
```
Row 5 set to HIGH
OK
```

### 5. Set Column

**Format:**
```
COL|COL_NUM|STATE
```

**Parameters:**
- `COL_NUM`: Column number (0-13)
- `STATE`: 0=LOW, 1=HIGH

**Example:**
```
COL|7|1           # Turn on all electrodes in column 7
```

**Response:**
```
Column 7 set to HIGH
OK
```

### 6. Run Test

**Format:**
```
TEST
```

Tests all 140 electrodes sequentially (100ms each).

**Response:**
```
Running electrode test (140 electrodes x 100ms)...
Test complete
OK
```

### 7. Get Status

**Format:**
```
STATUS
```

Returns system status information.

**Response:**
```
=== System Status ===
Sequence: IDLE
Electrodes: 140 (10 rows x 14 columns)
Status: OK
```

### 8. Stop Sequence

**Format:**
```
STOP
```

Stops currently running sequence.

**Response:**
```
Sequence stopped
OK
```

### 9. Get Electrode State

**Format:**
```
GET|ELECTRODE
```

**Parameters:**
- `ELECTRODE`: Electrode number (1-140)

**Example:**
```
GET|25
```

**Response:**
```
Electrode 25 (Row 1, Col 10): HIGH
OK
```

### 10. Help

**Format:**
```
HELP
```

Displays list of available commands.

**Response:**
```
=== ArrayDriver Commands ===
START|REPS|DELAY|STEPS|ID1,DUR1|ID2,DUR2|...|END - Execute sequence
SET|ELECTRODE|STATE - Set single electrode (STATE: 0=LOW, 1=HIGH)
ALL|STATE - Set all electrodes
ROW|ROW_NUM|STATE - Set all electrodes in row
COL|COL_NUM|STATE - Set all electrodes in column
TEST - Run full electrode test
STATUS - Get system status
STOP - Stop current sequence
GET|ELECTRODE - Get electrode state
RELOAD - Reload JSON mappings
HELP - Show this help
```

## Usage Examples

### Example 1: PCR Cycle via UART

**Python Script:**
```python
import serial
import time

ser = serial.Serial('COM3', 115200, timeout=1)  # Adjust COM port

def send_command(cmd):
    ser.write((cmd + '\n').encode())
    time.sleep(0.1)
    response = ser.read_all().decode()
    print(response)
    return response

# PCR cycle: 3 phases, 25 cycles
pcr_cmd = "START|25|1000|3|10,30000|15,30000|20,60000|END"
send_command(pcr_cmd)

ser.close()
```

### Example 2: Fluid Transport

**Python Script:**
```python
import serial
import time

ser = serial.Serial('COM3', 115200)

# Define transport path
path = [1, 5, 12, 18, 25, 33, 40, 48]
duration = 500  # 500ms per step

# Build command
steps = len(path)
cmd = f"START|1|0|{steps}|"
for electrode in path:
    cmd += f"{electrode},{duration}|"
cmd += "END"

ser.write((cmd + '\n').encode())
print(ser.read_all().decode())

ser.close()
```

### Example 3: Interactive Control

**Python Script:**
```python
import serial

ser = serial.Serial('COM3', 115200, timeout=1)

while True:
    cmd = input("Enter command: ")
    if cmd == 'quit':
        break
    
    ser.write((cmd + '\n').encode())
    time.sleep(0.1)
    print(ser.read_all().decode())

ser.close()
```

### Example 4: Mixing Pattern

**MATLAB Script:**
```matlab
% Open serial port
s = serial('COM3', 'BaudRate', 115200);
fopen(s);

% Define mixing zone
mixZone = [50, 51, 52, 64, 65, 66];
duration = 2000;  % 2 seconds

% Build command
cmd = sprintf('START|5|500|%d|', length(mixZone));
for i = 1:length(mixZone)
    cmd = [cmd sprintf('%d,%d|', mixZone(i), duration)];
end
cmd = [cmd 'END'];

% Send command
fprintf(s, '%s\n', cmd);
pause(0.1);
response = fscanf(s);
disp(response);

fclose(s);
delete(s);
```

## Error Handling

### Error Responses

All errors return:
```
ERROR: <error message>
```

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `Invalid start` | Missing START marker | Begin with `START\|` |
| `Invalid electrode (1-140)` | Electrode out of range | Use electrode 1-140 |
| `Missing delimiter` | Incorrect format | Check pipe `\|` separators |
| `Invalid state` | State not 0 or 1 | Use 0 (LOW) or 1 (HIGH) |
| `Early END marker` | Too few steps | Match STEPS count |
| `Missing END marker` | No END in command | End with `\|END` |
| `Buffer overflow` | Command too long | Max 2048 characters |

## Protocol Details

### Command Termination
- Commands must end with `\n` or `\r`
- Maximum command length: 2048 bytes
- Maximum steps per sequence: 256

### Response Format
- Success: `OK\n`
- Error: `ERROR: <message>\n`
- Data: Multi-line response followed by `OK\n`

### Timing
- Command processing: < 1ms (except sequence execution)
- UART timeout: 1000ms
- Sequence execution: User-defined

## Debugging

### Enable Verbose Mode

```cpp
// In your main loop, echo received commands
if (cmdHandler.isCommandReady()) {
    printf("Received: %s\n", cmdBuffer);  // Add debug output
    cmdHandler.processCommands();
}
```

### Test Connection

```
# Send test command
HELP

# Expected response
=== ArrayDriver Commands ===
...
```

### Monitor with Serial Terminal

Use any serial terminal:
- **PuTTY** (Windows)
- **screen** (Linux/Mac)
- **Arduino Serial Monitor**
- **Tera Term**
- **CoolTerm**

Settings:
- Baud: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None
- Local echo: ON (to see typed commands)

## Advanced Usage

### Scripting Automated Tests

**Bash Script:**
```bash
#!/bin/bash

DEVICE="/dev/ttyUSB0"
BAUD=115200

# Function to send command
send_cmd() {
    echo "$1" > $DEVICE
    sleep 0.2
}

# Test all electrodes
for i in {1..140}; do
    send_cmd "SET|$i|1"
    sleep 0.1
    send_cmd "SET|$i|0"
done

send_cmd "ALL|0"
```

### Creating Custom Protocols

```python
class ElectrodeController:
    def __init__(self, port, baud=115200):
        self.ser = serial.Serial(port, baud, timeout=1)
        time.sleep(2)  # Wait for init
    
    def execute_sequence(self, electrodes, durations, cycles=1, delay=0):
        """
        electrodes: list of electrode numbers
        durations: list of durations in ms
        cycles: number of repetitions
        delay: delay between cycles in ms
        """
        steps = len(electrodes)
        cmd = f"START|{cycles}|{delay}|{steps}|"
        
        for e, d in zip(electrodes, durations):
            cmd += f"{e},{d}|"
        cmd += "END"
        
        return self.send_command(cmd)
    
    def send_command(self, cmd):
        self.ser.write((cmd + '\n').encode())
        time.sleep(0.1)
        return self.ser.read_all().decode()
    
    def set_electrode(self, num, state):
        cmd = f"SET|{num}|{1 if state else 0}"
        return self.send_command(cmd)
    
    def close(self):
        self.ser.close()

# Usage
ctrl = ElectrodeController('COM3')
ctrl.execute_sequence([10, 20, 30], [1000, 1500, 2000], cycles=5)
ctrl.close()
```

## Performance

- **Command parsing:** < 1ms
- **Single electrode control:** < 100μs
- **Sequence execution:** User-defined timing
- **UART throughput:** Up to 11.5 KB/s @ 115200 baud

## Troubleshooting

### No Response
1. Check UART connections (TX/RX crossed)
2. Verify baud rate matches
3. Ensure common ground
4. Check UART initialization in STM32

### Garbled Output
1. Incorrect baud rate
2. Check data bits/parity settings
3. EMI/noise on UART lines

### Commands Not Executing
1. Check command format (use HELP)
2. Verify electrode numbers (1-140)
3. Check for missing delimiters
4. Ensure command ends with newline

## License

Copyright © 2025 Daya Tech. All rights reserved.

