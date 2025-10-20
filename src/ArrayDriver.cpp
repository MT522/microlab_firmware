#include "ArrayDriver.h"
#include <ctype.h>

// Constructor
ArrayDriver::ArrayDriver() {
    // Initialize row GPIO lookup table
    rowPins[0] = {ROW0_PORT, ROW0_PIN};
    rowPins[1] = {ROW1_PORT, ROW1_PIN};
    rowPins[2] = {ROW2_PORT, ROW2_PIN};
    rowPins[3] = {ROW3_PORT, ROW3_PIN};
    rowPins[4] = {ROW4_PORT, ROW4_PIN};
    rowPins[5] = {ROW5_PORT, ROW5_PIN};
    rowPins[6] = {ROW6_PORT, ROW6_PIN};
    rowPins[7] = {ROW7_PORT, ROW7_PIN};
    rowPins[8] = {ROW8_PORT, ROW8_PIN};
    rowPins[9] = {ROW9_PORT, ROW9_PIN};
    
    // Initialize column GPIO lookup table
    colPins[0] = {COL0_PORT, COL0_PIN};
    colPins[1] = {COL1_PORT, COL1_PIN};
    colPins[2] = {COL2_PORT, COL2_PIN};
    colPins[3] = {COL3_PORT, COL3_PIN};
    colPins[4] = {COL4_PORT, COL4_PIN};
    colPins[5] = {COL5_PORT, COL5_PIN};
    colPins[6] = {COL6_PORT, COL6_PIN};
    colPins[7] = {COL7_PORT, COL7_PIN};
    colPins[8] = {COL8_PORT, COL8_PIN};
    colPins[9] = {COL9_PORT, COL9_PIN};
    colPins[10] = {COL10_PORT, COL10_PIN};
    colPins[11] = {COL11_PORT, COL11_PIN};
    colPins[12] = {COL12_PORT, COL12_PIN};
    colPins[13] = {COL13_PORT, COL13_PIN};
    
    // Initialize all electrode states to low
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        for (uint8_t col = 0; col < NUM_COLS; col++) {
            electrodeState[row][col] = false;
        }
    }
    
    // Initialize sequence control variables
    sequenceRunning = false;
    currentStep = 0;
    stepStartTime = 0;
    currentSequence = nullptr;
    
    // Load electrode mappings from JSON files
    // This reads ElectrodeMap.json, PinMap.json, and PinDef.json at runtime
    bool success = true;
    success &= loadElectrodeMap(ELECTRODE_MAP_PATH);
    success &= loadPinMap(PIN_MAP_PATH);
    // PinDef is already hardcoded in the GPIO pin definitions above
    
    if (!success) {
        // Handle error - mapping files couldn't be loaded
        // For now, use default 1:1 mapping as fallback
        for (uint16_t i = 0; i < NUM_ELECTRODES; i++) {
            electrodeMap[i].row = i / NUM_COLS;
            electrodeMap[i].col = i % NUM_COLS;
        }
    }
}

// Initialization function
void ArrayDriver::init() {
    // Configure all row pins as outputs and set them LOW initially
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    
    // Initialize rows
    for (uint8_t i = 0; i < NUM_ROWS; i++) {
        GPIO_InitStruct.Pin = rowPins[i].pin;
        HAL_GPIO_Init(rowPins[i].port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(rowPins[i].port, rowPins[i].pin, GPIO_PIN_RESET);
    }
    
    // Initialize columns
    for (uint8_t i = 0; i < NUM_COLS; i++) {
        GPIO_InitStruct.Pin = colPins[i].pin;
        HAL_GPIO_Init(colPins[i].port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(colPins[i].port, colPins[i].pin, GPIO_PIN_SET);
    }
    
    // Set all electrodes to low state
    setAllElectrodesLow();
}

// Inline helper functions for GPIO control
inline void ArrayDriver::setRowHigh(uint8_t row) {
    HAL_GPIO_WritePin(rowPins[row].port, rowPins[row].pin, GPIO_PIN_SET);
}

inline void ArrayDriver::setRowLow(uint8_t row) {
    HAL_GPIO_WritePin(rowPins[row].port, rowPins[row].pin, GPIO_PIN_RESET);
}

inline void ArrayDriver::setColHigh(uint8_t col) {
    HAL_GPIO_WritePin(colPins[col].port, colPins[col].pin, GPIO_PIN_SET);
}

inline void ArrayDriver::setColLow(uint8_t col) {
    HAL_GPIO_WritePin(colPins[col].port, colPins[col].pin, GPIO_PIN_RESET);
}

// Atomic set operation for minimal delay between row and column transitions
inline void ArrayDriver::setRowColAtomic(uint8_t row, uint8_t col, bool state) {
    // Disable interrupts for atomic operation
    __disable_irq();
    
    if (state) {
        // To drive electrode HIGH: Row HIGH, Column LOW
        // Use BSRR register for atomic simultaneous operation
        rowPins[row].port->BSRR = rowPins[row].pin;  // Set row high
        colPins[col].port->BSRR = (uint32_t)colPins[col].pin << 16U;  // Reset column (set low)
    } else {
        // To drive electrode LOW: Row LOW, Column HIGH
        // Use BSRR register for atomic simultaneous operation
        rowPins[row].port->BSRR = (uint32_t)rowPins[row].pin << 16U;  // Reset row (set low)
        colPins[col].port->BSRR = colPins[col].pin;  // Set column high
    }
    
    // Re-enable interrupts
    __enable_irq();
}

// Set electrode to specific state
void ArrayDriver::setElectrode(uint8_t row, uint8_t col, bool state) {
    if (row >= NUM_ROWS || col >= NUM_COLS) {
        return;  // Invalid indices
    }
    
    setRowColAtomic(row, col, state);
    electrodeState[row][col] = state;
}

// Set electrode HIGH
void ArrayDriver::setElectrodeHigh(uint8_t row, uint8_t col) {
    setElectrode(row, col, true);
}

// Set electrode LOW
void ArrayDriver::setElectrodeLow(uint8_t row, uint8_t col) {
    setElectrode(row, col, false);
}

// Convert electrode number (1-140) to row/col
bool ArrayDriver::getRowColFromElectrode(uint8_t electrodeNum, uint8_t* row, uint8_t* col) {
    if (electrodeNum < 1 || electrodeNum > NUM_ELECTRODES) {
        return false;  // Invalid electrode number
    }
    
    // Use the mapping table loaded from JSON files at runtime
    *row = electrodeMap[electrodeNum - 1].row;
    *col = electrodeMap[electrodeNum - 1].col;
    return true;
}

// Set electrode by number (1-140)
void ArrayDriver::setElectrodeByNumber(uint8_t electrodeNum, bool state) {
    uint8_t row, col;
    if (getRowColFromElectrode(electrodeNum, &row, &col)) {
        setElectrode(row, col, state);
    }
}

// Set electrode HIGH by number
void ArrayDriver::setElectrodeHighByNumber(uint8_t electrodeNum) {
    setElectrodeByNumber(electrodeNum, true);
}

// Set electrode LOW by number
void ArrayDriver::setElectrodeLowByNumber(uint8_t electrodeNum) {
    setElectrodeByNumber(electrodeNum, false);
}

// Set all electrodes LOW
void ArrayDriver::setAllElectrodesLow() {
    // Disable interrupts for bulk operation
    __disable_irq();
    
    // Set all rows LOW
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        rowPins[row].port->BSRR = (uint32_t)rowPins[row].pin << 16U;
    }
    
    // Set all columns HIGH
    for (uint8_t col = 0; col < NUM_COLS; col++) {
        colPins[col].port->BSRR = colPins[col].pin;
    }
    
    __enable_irq();
    
    // Update state array
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        for (uint8_t col = 0; col < NUM_COLS; col++) {
            electrodeState[row][col] = false;
        }
    }
}

// Set all electrodes HIGH
void ArrayDriver::setAllElectrodesHigh() {
    // Disable interrupts for bulk operation
    __disable_irq();
    
    // Set all rows HIGH
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        rowPins[row].port->BSRR = rowPins[row].pin;
    }
    
    // Set all columns LOW
    for (uint8_t col = 0; col < NUM_COLS; col++) {
        colPins[col].port->BSRR = (uint32_t)colPins[col].pin << 16U;
    }
    
    __enable_irq();
    
    // Update state array
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        for (uint8_t col = 0; col < NUM_COLS; col++) {
            electrodeState[row][col] = true;
        }
    }
}

// Set all electrodes in a row to specific state
void ArrayDriver::setRowElectrodes(uint8_t row, bool state) {
    if (row >= NUM_ROWS) {
        return;
    }
    
    for (uint8_t col = 0; col < NUM_COLS; col++) {
        setElectrode(row, col, state);
    }
}

// Set all electrodes in a column to specific state
void ArrayDriver::setColElectrodes(uint8_t col, bool state) {
    if (col >= NUM_COLS) {
        return;
    }
    
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        setElectrode(row, col, state);
    }
}

// Get electrode state
bool ArrayDriver::getElectrodeState(uint8_t row, uint8_t col) {
    if (row >= NUM_ROWS || col >= NUM_COLS) {
        return false;
    }
    return electrodeState[row][col];
}

// Set pattern from array
void ArrayDriver::setPattern(bool pattern[NUM_ROWS][NUM_COLS]) {
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        for (uint8_t col = 0; col < NUM_COLS; col++) {
            setElectrode(row, col, pattern[row][col]);
        }
    }
}

// Get current pattern
void ArrayDriver::getPattern(bool pattern[NUM_ROWS][NUM_COLS]) {
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        for (uint8_t col = 0; col < NUM_COLS; col++) {
            pattern[row][col] = electrodeState[row][col];
        }
    }
}

// ============================================================================
// MICROFLUIDICS/PCR TEST SCENARIOS
// ============================================================================

// Execute sequence synchronously (blocking)
void ArrayDriver::executeSequence(const ElectrodeSequence_t* sequence) {
    if (!sequence || !sequence->steps) {
        return;
    }
    
    for (uint32_t cycle = 0; cycle < sequence->cycleCount; cycle++) {
        for (uint16_t step = 0; step < sequence->numSteps; step++) {
            const ElectrodeStep_t* currentStep = &sequence->steps[step];
            
            // Set electrode state
            setElectrode(currentStep->row, currentStep->col, currentStep->state);
            
            // Wait for duration
            HAL_Delay(currentStep->duration_ms);
        }
        
        // Delay between cycles (except for last cycle)
        if (cycle < sequence->cycleCount - 1) {
            HAL_Delay(sequence->cycleDelay_ms);
        }
    }
}

// Execute sequence asynchronously (non-blocking)
void ArrayDriver::executeSequenceAsync(const ElectrodeSequence_t* sequence) {
    if (!sequence || !sequence->steps) {
        return;
    }
    
    currentSequence = const_cast<ElectrodeSequence_t*>(sequence);
    sequenceRunning = true;
    currentStep = 0;
    stepStartTime = HAL_GetTick();
}

// Check if sequence is running
bool ArrayDriver::isSequenceRunning() {
    return sequenceRunning;
}

// Stop current sequence
void ArrayDriver::stopSequence() {
    sequenceRunning = false;
    currentSequence = nullptr;
    currentStep = 0;
}

// ============================================================================
// TEST SCENARIOS - User provides custom electrode sequences
// ============================================================================

// Run a test sequence on specified electrodes (by number 1-140)
void ArrayDriver::runElectrodeSequenceTest(uint8_t* electrodeNumbers, uint16_t numElectrodes, uint32_t duration_ms) {
    for (uint16_t i = 0; i < numElectrodes; i++) {
        setElectrodeHighByNumber(electrodeNumbers[i]);
        HAL_Delay(duration_ms);
        setElectrodeLowByNumber(electrodeNumbers[i]);
    }
}

// Sequential test of all electrodes (1-140)
void ArrayDriver::runElectrodeTest() {
    for (uint8_t electrodeNum = 1; electrodeNum <= NUM_ELECTRODES; electrodeNum++) {
        setElectrodeHighByNumber(electrodeNum);
        HAL_Delay(100);  // 100ms per electrode
        setElectrodeLowByNumber(electrodeNum);
    }
}

// ============================================================================
// JSON FILE READING AND PARSING
// ============================================================================

// Read file from filesystem (implement based on your platform)
char* ArrayDriver::readFile(const char* filepath, size_t* fileSize) {
    // For STM32 with FatFS or LittleFS
    FILE* file = fopen(filepath, "r");
    if (!file) {
        return nullptr;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    *fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer
    char* buffer = (char*)malloc(*fileSize + 1);
    if (!buffer) {
        fclose(file);
        return nullptr;
    }
    
    // Read file
    size_t bytesRead = fread(buffer, 1, *fileSize, file);
    buffer[bytesRead] = '\0';
    
    fclose(file);
    return buffer;
}

// Find JSON value for a given key
const char* ArrayDriver::findJSONValue(const char* json, const char* key) {
    char searchStr[64];
    snprintf(searchStr, sizeof(searchStr), "\"%s\"", key);
    
    const char* keyPos = strstr(json, searchStr);
    if (!keyPos) return nullptr;
    
    // Find the colon after the key
    const char* colon = strchr(keyPos, ':');
    if (!colon) return nullptr;
    
    // Skip whitespace after colon
    colon++;
    while (*colon && isspace(*colon)) colon++;
    
    return colon;
}

// Parse integer from JSON
int ArrayDriver::parseJSONInt(const char* str) {
    if (!str) return 0;
    
    // Skip whitespace
    while (*str && isspace(*str)) str++;
    
    // Parse number
    int value = 0;
    bool negative = false;
    
    if (*str == '-') {
        negative = true;
        str++;
    }
    
    while (*str && isdigit(*str)) {
        value = value * 10 + (*str - '0');
        str++;
    }
    
    return negative ? -value : value;
}

// Load ElectrodeMap.json - electrode number to PCIE pin
bool ArrayDriver::loadElectrodeMap(const char* filepath) {
    size_t fileSize;
    char* jsonData = readFile(filepath, &fileSize);
    if (!jsonData) {
        return false;
    }
    
    bool success = parseElectrodeMapJSON(jsonData);
    free(jsonData);
    return success;
}

// Parse ElectrodeMap.json
bool ArrayDriver::parseElectrodeMapJSON(const char* jsonData) {
    // Find the "mapping" object
    const char* mappingStart = strstr(jsonData, "\"mapping\"");
    if (!mappingStart) return false;
    
    // Find the opening brace
    const char* openBrace = strchr(mappingStart, '{');
    if (!openBrace) return false;
    
    // Parse each electrode number (1-140)
    for (uint16_t electrode = 1; electrode <= NUM_ELECTRODES; electrode++) {
        char keyStr[16];
        snprintf(keyStr, sizeof(keyStr), "\"%d\"", electrode);
        
        const char* keyPos = strstr(openBrace, keyStr);
        if (keyPos) {
            const char* valuePos = findJSONValue(keyPos, keyStr + 1); // Remove leading quote
            if (valuePos) {
                int pciePin = parseJSONInt(valuePos);
                // Store the PCIE pin for this electrode
                // This will be used with PinMap to get row/col
                if (pciePin > 0 && pciePin <= NUM_ELECTRODES) {
                    // Temporarily store PCIE pin number
                    // Will be resolved to row/col when PinMap is loaded
                    electrodeMap[electrode - 1].row = (pciePin - 1) / NUM_COLS;
                    electrodeMap[electrode - 1].col = (pciePin - 1) % NUM_COLS;
                }
            }
        }
    }
    
    return true;
}

// Load PinMap.json - PCIE pin to row/column
bool ArrayDriver::loadPinMap(const char* filepath) {
    size_t fileSize;
    char* jsonData = readFile(filepath, &fileSize);
    if (!jsonData) {
        return false;
    }
    
    bool success = parsePinMapJSON(jsonData);
    free(jsonData);
    return success;
}

// Parse PinMap.json
bool ArrayDriver::parsePinMapJSON(const char* jsonData) {
    // Find the "electrodes" object
    const char* electrodesStart = strstr(jsonData, "\"electrodes\"");
    if (!electrodesStart) return false;
    
    // Find the opening brace
    const char* openBrace = strchr(electrodesStart, '{');
    if (!openBrace) return false;
    
    // Initialize PCIE to row/col mapping
    for (uint16_t i = 0; i < NUM_ELECTRODES; i++) {
        pcieToRow[i] = 0;
        pcieToCol[i] = 0;
    }
    
    // Parse each row,col pair from JSON
    for (uint8_t row = 0; row < NUM_ROWS; row++) {
        for (uint8_t col = 0; col < NUM_COLS; col++) {
            char keyStr[16];
            snprintf(keyStr, sizeof(keyStr), "\"%d,%d\"", row, col);
            
            const char* keyPos = strstr(openBrace, keyStr);
            if (keyPos) {
                // Find the value (PCIE pin number)
                const char* colon = strchr(keyPos, ':');
                if (colon) {
                    int pciePin = parseJSONInt(colon + 1);
                    if (pciePin > 0 && pciePin <= NUM_ELECTRODES) {
                        pcieToRow[pciePin - 1] = row;
                        pcieToCol[pciePin - 1] = col;
                    }
                }
            }
        }
    }
    
    // Now update electrodeMap with the final mapping
    // Electrode# → PCIE Pin (from ElectrodeMap.json) → Row/Col (from PinMap.json)
    for (uint16_t electrode = 1; electrode <= NUM_ELECTRODES; electrode++) {
        // Get PCIE pin for this electrode (currently stored as temp row/col)
        uint8_t tempRow = electrodeMap[electrode - 1].row;
        uint8_t tempCol = electrodeMap[electrode - 1].col;
        uint16_t pciePin = tempRow * NUM_COLS + tempCol + 1;
        
        // Get actual row/col from PCIE pin mapping
        if (pciePin > 0 && pciePin <= NUM_ELECTRODES) {
            electrodeMap[electrode - 1].row = pcieToRow[pciePin - 1];
            electrodeMap[electrode - 1].col = pcieToCol[pciePin - 1];
        }
    }
    
    return true;
}

// Load PinDef.json - GPIO definitions (currently hardcoded in header)
bool ArrayDriver::loadPinDef(const char* filepath) {
    // GPIO definitions are currently hardcoded in the header file
    // This function is a placeholder for future dynamic GPIO loading
    return true;
}
