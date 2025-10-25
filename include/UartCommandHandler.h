#ifndef UARTCOMMANDHANDLER_H
#define UARTCOMMANDHANDLER_H

#include "stm32f4xx_hal.h"
#include "ArrayDriver.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// Command buffer size
#define UART_CMD_BUFFER_SIZE 2048
#define MAX_STEPS 256
#define UART_RESPONSE_BUFFER_SIZE 256

class UartCommandHandler {
private:
    ArrayDriver* arrayDriver;
    UART_HandleTypeDef* huart;
    
    // Command buffer
    char cmdBuffer[UART_CMD_BUFFER_SIZE];
    uint16_t cmdBufferIndex;
    bool cmdComplete;
    
    // Response buffer
    char responseBuffer[UART_RESPONSE_BUFFER_SIZE];
    
    // Sequence storage
    ElectrodeStep_t sequenceSteps[MAX_STEPS];
    ElectrodeSequence_t currentSequence;
    
    // Command parsing functions
    void parseCommand(char* cmd);
    void parseElectrodeCommand(char* cmd);
    void parseSingleElectrodeCommand(char* cmd);
    void parseAllElectrodesCommand(char* cmd);
    void parseRowCommand(char* cmd);
    void parseColCommand(char* cmd);
    void parseTestCommand(char* cmd);
    void parseStatusCommand(char* cmd);
    void parseStopCommand(char* cmd);
    void parseGetStateCommand(char* cmd);
    void parseReloadMappingCommand(char* cmd);
    
    // Helper functions
    void sendResponse(const char* response);
    void sendError(const char* errorMsg);
    void sendOK();
    
    // Execute sequence from parsed data
    void executeSequence(int cycleReps, int cycleDelay, int numSteps, 
                        int* electrodeIds, int* durations);

public:
    // Constructor
    UartCommandHandler(ArrayDriver* driver, UART_HandleTypeDef* uart);
    
    // Initialization
    void init();
    
    // Process incoming byte (call from UART interrupt or polling)
    void processByte(uint8_t byte);
    
    // Process complete command (call from main loop)
    void processCommands();
    
    // Check if command is ready
    bool isCommandReady();
};

#endif // UARTCOMMANDHANDLER_H

