/**
* CopyCat 3000
*
* Copies all the cats
*/

#pragma warning disable 520         // Suppress "function [...] not called" warning

#include "mcc_generated_files/mcc.h"

void ISR_Timer0();          // Timer0 ISR
void ISR_Timer2();          // Timer1 ISR
void RecordSequence();      // Record sequence
void PlaySequence();        // Play sequence

static volatile bool t0Rollover = false;    // Timer0 rollover flag

static volatile bool swProgram = false;     // Debounced program switch (true if pressed)
static volatile bool swAction = false;      // Debounced action switch (true if pressed)
static volatile bool inTrigger = false;     // Debounced trigger input (true if active)

#define MAX_SEQ_LEN     255         // Max sequence length

void main(void)
{
    uint16_t progHoldCount = 0;
    uint8_t ledToggleCount = 0;
    
    // Initialize system
    SYSTEM_Initialize();
    
    // Set Timer0 ISR handler
    TMR0_SetInterruptHandler(ISR_Timer0);
    
    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();
    
    IO_RA4_SetLow();
    IO_RA5_SetLow();
    
    while (1)
    {
        CLRWDT();
        
        if(t0Rollover)
        {
            t0Rollover = false;
            
            // Rapid LED flash
            ++ledToggleCount;
            if(ledToggleCount > 10)
            {
                IO_RA5_Toggle();
                ledToggleCount = 0;
            }
            
            if(swProgram)
            {
                // Count how many cycles the program button is held
                ++progHoldCount;
                if(progHoldCount >= 1000)
                {
                    // Program button held for a while, record sequence
                    RecordSequence();
                    progHoldCount = 0;
                }
            }
            else
            {
                // Program button released, reset counter
                progHoldCount = 0;
            }
        }
        
        if(swAction || inTrigger)
        {
            // Action button pressed or trigger active, play sequence
            PlaySequence();
        }
    }
}

void ISR_Timer0()
{
    static bool swProgram_prev = false;
    static bool swAction_prev = false;
    static bool inTrigger_prev = false;
    
    // Debounce program switch
    swProgram = !(IO_RA1_GetValue() || swProgram_prev);
    swProgram_prev = IO_RA1_GetValue();
    
    // Debounce action switch
    swAction = !(IO_RA2_GetValue() || swAction_prev);
    swAction_prev = IO_RA2_GetValue();
    
    // Debounce trigger input
    inTrigger = !(IO_RA0_GetValue() || inTrigger_prev);
    inTrigger_prev = IO_RA0_GetValue();
    
    // Set rollover flag
    t0Rollover = true;
}

void RecordSequence()
{
    uint8_t nBit = 0;           // Current bit number
    uint8_t seqValue = 0x00;    // Current 8-bit sequence value
    uint8_t len = 0;            // Sequence length
    uint8_t addr = 0x01;        // EEPROM address
    
    IO_RA4_SetLow();
    IO_RA5_SetHigh();
    
    // Wait while held
    while(swProgram)
    {
        CLRWDT();
    }
    
    // Wait while released
    while(!swProgram)
    {
        CLRWDT();
    }
    
    IO_RA5_SetLow();
    
    // Reset Timer0
    TMR0_Reload();
    
    // Record while program button is held
    while(swProgram)
    {
        // Reset bit number and sequence value
        nBit = 0;
        seqValue = 0x00;
        
        // Capture 8 bits
        while(nBit < 8)
        {
            // Shift sequence value and OR in new bit
            seqValue <<= 1;
            if(swAction)
            {
                IO_RA4_SetHigh();
                IO_RA5_SetHigh();
                seqValue |= 1;
            }
            else
            {
                IO_RA4_SetLow();
                IO_RA5_SetLow();
            }

            // Increment bit number, handle completed byte if needed
            ++nBit;
            if(nBit >= 8)
            {
                // Write value to EEPROM
                DATAEE_WriteByte(addr, seqValue);
                ++addr;

                // Increment length
                ++len;
            }
            
            // Wait for timer
            while(!t0Rollover)
            {
                CLRWDT();
            }
            t0Rollover = false;
        }
        
        // Check if sequence is full
        if(len >= MAX_SEQ_LEN)
        {
            break;
        }
    }
    
    // Write sequence length to EEPROM
    DATAEE_WriteByte(0x00, len);
    
    IO_RA4_SetLow();
    IO_RA5_SetLow();
}

void PlaySequence()
{
    uint8_t nBit = 0;           // Current bit number
    uint8_t seqValue = 0x00;    // Current 8-bit sequence value
    uint8_t len = 0;            // Sequence length
    uint8_t addr = 0x01;        // EEPROM address
    
    IO_RA4_SetLow();
    IO_RA5_SetLow();
    
    // Read sequence length
    len = DATAEE_ReadByte(0x00);
    
    // Zero sequence length, do nothing
    if(len == 0)
    {
        return;
    }
    
    // Reset Timer0
    TMR0_Reload();
    
    while(len--)
    {
        // Read sequence value
        seqValue = DATAEE_ReadByte(addr);
        ++addr;
        
        // Play all bits
        for(nBit = 0; nBit < 8; ++nBit)
        {
            if(seqValue & 0x80)
            {
                IO_RA4_SetHigh();
                IO_RA5_SetHigh();
            }
            else
            {
                IO_RA4_SetLow();
                IO_RA5_SetLow();
            }

            // Shift sequence value
            seqValue <<= 1;
            
            // Wait for timer
            while(!t0Rollover)
            {
                CLRWDT();
            }
            t0Rollover = false;
        }
    }
    
    // Clear outputs
    IO_RA4_SetLow();
    IO_RA5_SetLow();
}
