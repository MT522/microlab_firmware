#ifndef ARRAYDRIVER_H
#define ARRAYDRIVER_H

#if defined(__has_include)
#  if __has_include("stm32f4xx_hal.h")
#    include "stm32f4xx_hal.h"  // STM32F413 (F4 series) CubeMX HAL
#  else
#    include <stdint.h>
#    include <stdbool.h>
// Minimal fallback so this file can be linted without HAL present.
// In your STM32 project, the real HAL will be used instead.
typedef struct {
	uint32_t Pin;
	uint32_t Mode;
	uint32_t Pull;
	uint32_t Speed;
	uint32_t Alternate;
} GPIO_InitTypeDef;

typedef struct {
	volatile uint32_t MODER;
	volatile uint32_t OTYPER;
	volatile uint32_t OSPEEDR;
	volatile uint32_t PUPDR;
	volatile uint32_t IDR;
	volatile uint32_t ODR;
	volatile uint32_t BSRR;
	volatile uint32_t LCKR;
	volatile uint32_t AFR[2];
} GPIO_TypeDef;

#define GPIO_MODE_OUTPUT_PP 0x01U
#define GPIO_NOPULL 0x00U
#define GPIO_SPEED_FREQ_VERY_HIGH 0x03U
#ifndef GPIO_PIN_RESET
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
#endif

void HAL_GPIO_Init(GPIO_TypeDef* GPIOx, GPIO_InitTypeDef* GPIO_Init);
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t PinState);

extern GPIO_TypeDef* GPIOA;
extern GPIO_TypeDef* GPIOB;
extern GPIO_TypeDef* GPIOC;
extern GPIO_TypeDef* GPIOD;
#  endif
#else
#  include "stm32f4xx_hal.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Matrix dimensions
#define NUM_ROWS 10
#define NUM_COLS 14
#define NUM_ELECTRODES 140

// GPIO Pin definitions based on PinDef.json mapping
// Rows (10 pins): GPIOA[0-7], GPIOB[0-1]
#define ROW0_PORT GPIOA
#define ROW0_PIN GPIO_PIN_0
#define ROW1_PORT GPIOA
#define ROW1_PIN GPIO_PIN_1
#define ROW2_PORT GPIOA
#define ROW2_PIN GPIO_PIN_2
#define ROW3_PORT GPIOA
#define ROW3_PIN GPIO_PIN_3
#define ROW4_PORT GPIOA
#define ROW4_PIN GPIO_PIN_4
#define ROW5_PORT GPIOA
#define ROW5_PIN GPIO_PIN_5
#define ROW6_PORT GPIOA
#define ROW6_PIN GPIO_PIN_6
#define ROW7_PORT GPIOA
#define ROW7_PIN GPIO_PIN_7
#define ROW8_PORT GPIOB
#define ROW8_PIN GPIO_PIN_0
#define ROW9_PORT GPIOB
#define ROW9_PIN GPIO_PIN_1

// Columns (14 pins): GPIOC[0-7], GPIOD[0-5]
#define COL0_PORT GPIOC
#define COL0_PIN GPIO_PIN_0
#define COL1_PORT GPIOC
#define COL1_PIN GPIO_PIN_1
#define COL2_PORT GPIOC
#define COL2_PIN GPIO_PIN_2
#define COL3_PORT GPIOC
#define COL3_PIN GPIO_PIN_3
#define COL4_PORT GPIOC
#define COL4_PIN GPIO_PIN_4
#define COL5_PORT GPIOC
#define COL5_PIN GPIO_PIN_5
#define COL6_PORT GPIOC
#define COL6_PIN GPIO_PIN_6
#define COL7_PORT GPIOC
#define COL7_PIN GPIO_PIN_7
#define COL8_PORT GPIOD
#define COL8_PIN GPIO_PIN_0
#define COL9_PORT GPIOD
#define COL9_PIN GPIO_PIN_1
#define COL10_PORT GPIOD
#define COL10_PIN GPIO_PIN_2
#define COL11_PORT GPIOD
#define COL11_PIN GPIO_PIN_3
#define COL12_PORT GPIOD
#define COL12_PIN GPIO_PIN_4
#define COL13_PORT GPIOD
#define COL13_PIN GPIO_PIN_5

// GPIO pin structure for efficient access
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} GPIO_Pin_t;

// Microfluidics/PCR Test Scenarios - Forward declarations
typedef struct {
    uint8_t row;
    uint8_t col;
    bool state;
    uint32_t duration_ms;  // Duration to hold this state
} ElectrodeStep_t;

typedef struct {
    ElectrodeStep_t* steps;
    uint16_t numSteps;
    uint32_t cycleCount;  // Number of times to repeat the sequence
    uint32_t cycleDelay_ms;  // Delay between cycles
} ElectrodeSequence_t;

class ArrayDriver {
private:
    // Row and column GPIO lookup tables
    GPIO_Pin_t rowPins[NUM_ROWS];
    GPIO_Pin_t colPins[NUM_COLS];
    
    // Current state of electrodes (true = high, false = low)
    bool electrodeState[NUM_ROWS][NUM_COLS];
    
    // Sequence control variables
    volatile bool sequenceRunning;
    volatile uint16_t currentStep;
    volatile uint32_t stepStartTime;
    ElectrodeSequence_t* currentSequence;
    
    // Electrode number to row/col mapping (loaded from JSON files at runtime)
    typedef struct {
        uint8_t row;
        uint8_t col;
    } ElectrodeMapping_t;
    ElectrodeMapping_t electrodeMap[NUM_ELECTRODES];
    
    // PCIE pin to row/col mapping
    uint8_t pcieToRow[NUM_ELECTRODES];
    uint8_t pcieToCol[NUM_ELECTRODES];
    
    // JSON file paths
    static constexpr const char* ELECTRODE_MAP_PATH = "resources/ElectrodeMap.json";
    static constexpr const char* PIN_MAP_PATH = "resources/PinMap.json";
    static constexpr const char* PIN_DEF_PATH = "resources/PinDef.json";
    
    // JSON parsing functions
    bool loadElectrodeMap(const char* filepath);
    bool loadPinMap(const char* filepath);
    bool loadPinDef(const char* filepath);
    char* readFile(const char* filepath, size_t* fileSize);
    bool parseElectrodeMapJSON(const char* jsonData);
    bool parsePinMapJSON(const char* jsonData);
    bool parsePinDefJSON(const char* jsonData);
    const char* findJSONValue(const char* json, const char* key);
    int parseJSONInt(const char* str);
    
    // Helper functions for GPIO control
    inline void setRowHigh(uint8_t row);
    inline void setRowLow(uint8_t row);
    inline void setColHigh(uint8_t col);
    inline void setColLow(uint8_t col);
    
    // Atomic set operations for minimal delay
    inline void setRowColAtomic(uint8_t row, uint8_t col, bool state);
    
public:
    // Constructor
    ArrayDriver();
    
    // Initialization
    void init();
    
    // Electrode control by row/column
    void setElectrode(uint8_t row, uint8_t col, bool state);
    void setElectrodeHigh(uint8_t row, uint8_t col);
    void setElectrodeLow(uint8_t row, uint8_t col);
    
    // Electrode control by electrode number (1-140)
    void setElectrodeByNumber(uint8_t electrodeNum, bool state);
    void setElectrodeHighByNumber(uint8_t electrodeNum);
    void setElectrodeLowByNumber(uint8_t electrodeNum);
    
    // Convert electrode number to row/col
    bool getRowColFromElectrode(uint8_t electrodeNum, uint8_t* row, uint8_t* col);
    
    // Bulk operations
    void setAllElectrodesLow();
    void setAllElectrodesHigh();
    void setRowElectrodes(uint8_t row, bool state);
    void setColElectrodes(uint8_t col, bool state);
    
    // State query
    bool getElectrodeState(uint8_t row, uint8_t col);
    
    // Advanced control
    void setPattern(bool pattern[NUM_ROWS][NUM_COLS]);
    void getPattern(bool pattern[NUM_ROWS][NUM_COLS]);
    
    // Sequence execution functions
    void executeSequence(const ElectrodeSequence_t* sequence);
    void executeSequenceAsync(const ElectrodeSequence_t* sequence);
    bool isSequenceRunning();
    void stopSequence();
    
    // Test scenarios - user provides custom electrode lists
    void runElectrodeSequenceTest(uint8_t* electrodeNumbers, uint16_t numElectrodes, uint32_t duration_ms);
    void runElectrodeTest();  // Sequential test of all electrodes (1-140)
};

#endif // ARRAYDRIVER_H
