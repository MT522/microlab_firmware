#include "UartCommandHandler.h"
#include <stdio.h>

// Constructor
UartCommandHandler::UartCommandHandler(ArrayDriver* driver, UART_HandleTypeDef* uart) {
    arrayDriver = driver;
    huart = uart;
    cmdBufferIndex = 0;
    cmdComplete = false;
}

// Initialization
void UartCommandHandler::init() {
    cmdBufferIndex = 0;
    cmdComplete = false;
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
    sendResponse("ArrayDriver UART Command Handler Ready\n");
    sendResponse("Type 'HELP' for command list\n");
}

// Process incoming byte
void UartCommandHandler::processByte(uint8_t byte) {
    // Check for buffer overflow
    if (cmdBufferIndex >= UART_CMD_BUFFER_SIZE - 1) {
        sendError("Buffer overflow");
        cmdBufferIndex = 0;
        return;
    }
    
    // Check for end of command (newline)
    if (byte == '\n' || byte == '\r') {
        if (cmdBufferIndex > 0) {
            cmdBuffer[cmdBufferIndex] = '\0';
            cmdComplete = true;
        }
        return;
    }
    
    // Add byte to buffer
    cmdBuffer[cmdBufferIndex++] = byte;
}

// Check if command is ready
bool UartCommandHandler::isCommandReady() {
    return cmdComplete;
}

// Process complete command
void UartCommandHandler::processCommands() {
    if (!cmdComplete) {
        return;
    }
    
    // Parse and execute command
    parseCommand(cmdBuffer);
    
    // Reset buffer
    cmdBufferIndex = 0;
    cmdComplete = false;
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
}

// Send response
void UartCommandHandler::sendResponse(const char* response) {
    HAL_UART_Transmit(huart, (uint8_t*)response, strlen(response), 1000);
}

// Send error message
void UartCommandHandler::sendError(const char* errorMsg) {
    snprintf(responseBuffer, sizeof(responseBuffer), "ERROR: %s\n", errorMsg);
    sendResponse(responseBuffer);
}

// Send OK response
void UartCommandHandler::sendOK() {
    sendResponse("OK\n");
}

// Main command parser
void UartCommandHandler::parseCommand(char* cmd) {
    // Skip leading whitespace
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    
    // Check for empty command
    if (*cmd == '\0') {
        return;
    }
    
    // Parse command type
    if (strncmp(cmd, "START|", 6) == 0) {
        parseElectrodeCommand(cmd);
    }
    else if (strncmp(cmd, "SET|", 4) == 0) {
        parseSingleElectrodeCommand(cmd);
    }
    else if (strncmp(cmd, "ALL|", 4) == 0) {
        parseAllElectrodesCommand(cmd);
    }
    else if (strncmp(cmd, "ROW|", 4) == 0) {
        parseRowCommand(cmd);
    }
    else if (strncmp(cmd, "COL|", 4) == 0) {
        parseColCommand(cmd);
    }
    else if (strncmp(cmd, "TEST", 4) == 0) {
        parseTestCommand(cmd);
    }
    else if (strncmp(cmd, "STATUS", 6) == 0) {
        parseStatusCommand(cmd);
    }
    else if (strncmp(cmd, "STOP", 4) == 0) {
        parseStopCommand(cmd);
    }
    else if (strncmp(cmd, "GET|", 4) == 0) {
        parseGetStateCommand(cmd);
    }
    else if (strncmp(cmd, "RELOAD", 6) == 0) {
        parseReloadMappingCommand(cmd);
    }
    else if (strncmp(cmd, "HELP", 4) == 0) {
        sendResponse("\n=== ArrayDriver Commands ===\n");
        sendResponse("START|REPS|DELAY|STEPS|ID1,DUR1|ID2,DUR2|...|END - Execute sequence\n");
        sendResponse("SET|ELECTRODE|STATE - Set single electrode (STATE: 0=LOW, 1=HIGH)\n");
        sendResponse("ALL|STATE - Set all electrodes\n");
        sendResponse("ROW|ROW_NUM|STATE - Set all electrodes in row\n");
        sendResponse("COL|COL_NUM|STATE - Set all electrodes in column\n");
        sendResponse("TEST - Run full electrode test\n");
        sendResponse("STATUS - Get system status\n");
        sendResponse("STOP - Stop current sequence\n");
        sendResponse("GET|ELECTRODE - Get electrode state\n");
        sendResponse("RELOAD - Reload JSON mappings\n");
        sendResponse("HELP - Show this help\n\n");
    }
    else {
        sendError("Unknown command. Type 'HELP' for command list");
    }
}

// Parse electrode sequence command
// Format: START|REPS|DELAY|STEPS|ID1,DUR1|ID2,DUR2|...|END
void UartCommandHandler::parseElectrodeCommand(char* cmd) {
    // Check start marker
    if (strncmp(cmd, "START|", 6) != 0) {
        sendError("Invalid start");
        return;
    }
    
    int cycleReps, cycleDelay, numSteps;
    
    // Parse header: START|REPS|DELAY|STEPS|
    char* ptr = cmd + 6; // Skip "START|"
    
    // Parse cycle repetitions
    cycleReps = atoi(ptr);
    if (cycleReps < 1 || cycleReps > 1000) {
        sendError("Invalid cycle repetitions (1-1000)");
        return;
    }
    
    ptr = strchr(ptr, '|');
    if (!ptr) {
        sendError("Missing delimiter after REPS");
        return;
    }
    ptr++; // Skip '|'
    
    // Parse cycle delay
    cycleDelay = atoi(ptr);
    if (cycleDelay < 0) {
        sendError("Invalid cycle delay");
        return;
    }
    
    ptr = strchr(ptr, '|');
    if (!ptr) {
        sendError("Missing delimiter after DELAY");
        return;
    }
    ptr++; // Skip '|'
    
    // Parse number of steps
    numSteps = atoi(ptr);
    if (numSteps < 1 || numSteps > MAX_STEPS) {
        snprintf(responseBuffer, sizeof(responseBuffer), 
                "Invalid steps count (1-%d)", MAX_STEPS);
        sendError(responseBuffer);
        return;
    }
    
    ptr = strchr(ptr, '|');
    if (!ptr) {
        sendError("Missing delimiter after STEPS");
        return;
    }
    ptr++; // Skip '|'
    
    // Parse electrode steps
    int electrodeIds[MAX_STEPS];
    int durations[MAX_STEPS];
    
    for (int i = 0; i < numSteps; i++) {
        // Parse electrode ID
        electrodeIds[i] = atoi(ptr);
        
        if (electrodeIds[i] < 1 || electrodeIds[i] > 140) {
            snprintf(responseBuffer, sizeof(responseBuffer), 
                    "Invalid electrode ID at step %d (1-140)", i);
            sendError(responseBuffer);
            return;
        }
        
        // Find comma
        ptr = strchr(ptr, ',');
        if (!ptr) {
            sendError("Missing comma in step");
            return;
        }
        ptr++; // Skip ','
        
        // Parse duration
        durations[i] = atoi(ptr);
        
        if (durations[i] < 0) {
            snprintf(responseBuffer, sizeof(responseBuffer), 
                    "Invalid duration at step %d", i);
            sendError(responseBuffer);
            return;
        }
        
        // Find next pipe or END
        ptr = strchr(ptr, '|');
        if (!ptr) {
            sendError("Missing delimiter");
            return;
        }
        ptr++; // Skip '|'
        
        // Check for END marker
        if (strncmp(ptr, "END", 3) == 0) {
            if (i + 1 == numSteps) {
                // Successfully parsed all steps
                executeSequence(cycleReps, cycleDelay, numSteps, 
                               electrodeIds, durations);
                sendOK();
                return;
            } else {
                sendError("Early END marker");
                return;
            }
        }
    }
    
    sendError("Missing END marker");
}

// Execute sequence from parsed data
void UartCommandHandler::executeSequence(int cycleReps, int cycleDelay, 
                                        int numSteps, int* electrodeIds, int* durations) {
    // Build sequence steps
    for (int i = 0; i < numSteps; i++) {
        uint8_t row, col;
        if (arrayDriver->getRowColFromElectrode(electrodeIds[i], &row, &col)) {
            sequenceSteps[i].row = row;
            sequenceSteps[i].col = col;
            sequenceSteps[i].state = true;  // Turn on
            sequenceSteps[i].duration_ms = durations[i];
        } else {
            sendError("Invalid electrode number");
            return;
        }
    }
    
    // Configure sequence
    currentSequence.steps = sequenceSteps;
    currentSequence.numSteps = numSteps;
    currentSequence.cycleCount = cycleReps;
    currentSequence.cycleDelay_ms = cycleDelay;
    
    // Execute sequence
    sendResponse("Executing sequence...\n");
    arrayDriver->executeSequence(&currentSequence);
    sendResponse("Sequence complete\n");
}

// Parse single electrode command
// Format: SET|ELECTRODE|STATE
void UartCommandHandler::parseSingleElectrodeCommand(char* cmd) {
    char* ptr = cmd + 4; // Skip "SET|"
    
    int electrode = atoi(ptr);
    if (electrode < 1 || electrode > 140) {
        sendError("Invalid electrode (1-140)");
        return;
    }
    
    ptr = strchr(ptr, '|');
    if (!ptr) {
        sendError("Missing delimiter");
        return;
    }
    ptr++;
    
    int state = atoi(ptr);
    if (state != 0 && state != 1) {
        sendError("Invalid state (0=LOW, 1=HIGH)");
        return;
    }
    
    arrayDriver->setElectrodeByNumber(electrode, state == 1);
    
    snprintf(responseBuffer, sizeof(responseBuffer), 
            "Electrode %d set to %s\n", electrode, state ? "HIGH" : "LOW");
    sendResponse(responseBuffer);
    sendOK();
}

// Parse all electrodes command
// Format: ALL|STATE
void UartCommandHandler::parseAllElectrodesCommand(char* cmd) {
    char* ptr = cmd + 4; // Skip "ALL|"
    
    int state = atoi(ptr);
    if (state != 0 && state != 1) {
        sendError("Invalid state (0=LOW, 1=HIGH)");
        return;
    }
    
    if (state == 1) {
        arrayDriver->setAllElectrodesHigh();
        sendResponse("All electrodes set to HIGH\n");
    } else {
        arrayDriver->setAllElectrodesLow();
        sendResponse("All electrodes set to LOW\n");
    }
    
    sendOK();
}

// Parse row command
// Format: ROW|ROW_NUM|STATE
void UartCommandHandler::parseRowCommand(char* cmd) {
    char* ptr = cmd + 4; // Skip "ROW|"
    
    int row = atoi(ptr);
    if (row < 0 || row > 9) {
        sendError("Invalid row (0-9)");
        return;
    }
    
    ptr = strchr(ptr, '|');
    if (!ptr) {
        sendError("Missing delimiter");
        return;
    }
    ptr++;
    
    int state = atoi(ptr);
    if (state != 0 && state != 1) {
        sendError("Invalid state (0=LOW, 1=HIGH)");
        return;
    }
    
    arrayDriver->setRowElectrodes(row, state == 1);
    
    snprintf(responseBuffer, sizeof(responseBuffer), 
            "Row %d set to %s\n", row, state ? "HIGH" : "LOW");
    sendResponse(responseBuffer);
    sendOK();
}

// Parse column command
// Format: COL|COL_NUM|STATE
void UartCommandHandler::parseColCommand(char* cmd) {
    char* ptr = cmd + 4; // Skip "COL|"
    
    int col = atoi(ptr);
    if (col < 0 || col > 13) {
        sendError("Invalid column (0-13)");
        return;
    }
    
    ptr = strchr(ptr, '|');
    if (!ptr) {
        sendError("Missing delimiter");
        return;
    }
    ptr++;
    
    int state = atoi(ptr);
    if (state != 0 && state != 1) {
        sendError("Invalid state (0=LOW, 1=HIGH)");
        return;
    }
    
    arrayDriver->setColElectrodes(col, state == 1);
    
    snprintf(responseBuffer, sizeof(responseBuffer), 
            "Column %d set to %s\n", col, state ? "HIGH" : "LOW");
    sendResponse(responseBuffer);
    sendOK();
}

// Parse test command
void UartCommandHandler::parseTestCommand(char* cmd) {
    sendResponse("Running electrode test (140 electrodes x 100ms)...\n");
    arrayDriver->runElectrodeTest();
    sendResponse("Test complete\n");
    sendOK();
}

// Parse status command
void UartCommandHandler::parseStatusCommand(char* cmd) {
    sendResponse("\n=== System Status ===\n");
    
    if (arrayDriver->isSequenceRunning()) {
        sendResponse("Sequence: RUNNING\n");
    } else {
        sendResponse("Sequence: IDLE\n");
    }
    
    snprintf(responseBuffer, sizeof(responseBuffer), 
            "Electrodes: 140 (10 rows x 14 columns)\n");
    sendResponse(responseBuffer);
    
    sendResponse("Status: OK\n\n");
}

// Parse stop command
void UartCommandHandler::parseStopCommand(char* cmd) {
    if (arrayDriver->isSequenceRunning()) {
        arrayDriver->stopSequence();
        sendResponse("Sequence stopped\n");
    } else {
        sendResponse("No sequence running\n");
    }
    sendOK();
}

// Parse get state command
// Format: GET|ELECTRODE
void UartCommandHandler::parseGetStateCommand(char* cmd) {
    char* ptr = cmd + 4; // Skip "GET|"
    
    int electrode = atoi(ptr);
    if (electrode < 1 || electrode > 140) {
        sendError("Invalid electrode (1-140)");
        return;
    }
    
    uint8_t row, col;
    if (arrayDriver->getRowColFromElectrode(electrode, &row, &col)) {
        bool state = arrayDriver->getElectrodeState(row, col);
        snprintf(responseBuffer, sizeof(responseBuffer), 
                "Electrode %d (Row %d, Col %d): %s\n", 
                electrode, row, col, state ? "HIGH" : "LOW");
        sendResponse(responseBuffer);
        sendOK();
    } else {
        sendError("Failed to get electrode state");
    }
}

// Parse reload mapping command
void UartCommandHandler::parseReloadMappingCommand(char* cmd) {
    sendResponse("Reload mapping not implemented (requires re-initialization)\n");
    sendError("Not implemented");
}

