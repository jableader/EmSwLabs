/*! @file
 *
 *  @brief Flash Hardware Abstraction Layer
 *
 *  This contains the functions needed for accessing the internal Flash.
 *
 *  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-29
 */

#include "Flash.h"
#include "MK70F12.h"

// Macro for selecting the nth byte.
#define GET_BYTE(a, n) (uint8_t)((a >> (n * 8)) & 0xFF)

// Macro for setting data - encapsulates casting logic into single place.
#define SET_DATA(type, dataPtr, data) *(type *)dataPtr = *(type *)data

// Enum for Flash FTFE Command opcodes
typedef enum
{
  PROGRAM_PHRASE = 0x07, /*!< Program 8 bytes into flash block */
  ERASE_SECTOR = 0x09 /*!< Erase all the bytes in the flash sector */
} FlashCommand;

// Struct to encapsulate what is put in the FCCOB registers
typedef struct
{
  FlashCommand command; /*!< The command to be executed */
  uint32_t address; /*!< The address the command should operate on - The Most Significant Byte is ignored */
  uint64_t data; /*!< The data to be used in the command */
} TFCCOB;

/* @brief Launch FTFE Command
 *
 * Executes a flash command
 * See 30.4.10.1.3 of K70P256M150SF3RM.pdf for Command Execution flow chart
 *
 * @param command - The command to be executed
 * @return bool - TRUE if the command was completed with no errors
 */
static bool LaunchCommand(TFCCOB* command)
{
  // Write to the FCCOB registers

  // Set command code
  FTFE_FCCOB0 = command->command;

  // Set Flash Address
  FTFE_FCCOB1 = GET_BYTE(command->address, 2); // [23:16]
  FTFE_FCCOB2 = GET_BYTE(command->address, 1); // [15:8]
  FTFE_FCCOB3 = GET_BYTE(command->address, 0); // [7:0]

  // Set Data registers
  // FCCOB7 has the lowest address in memory
  // using it we can set the whole set of registers at once
  *((uint64_t *)&FTFE_FCCOB7) = command->data;

  // Clear CCIF to launch the command
  FTFE_FSTAT = FTFE_FSTAT_CCIF_MASK;

  // Wait for command to finish
  while (!(FTFE_FSTAT & FTFE_FSTAT_CCIF_MASK)) { }

  // Check for errors
  const uint8_t errorMask = FTFE_FSTAT_FPVIOL_MASK | FTFE_FSTAT_ACCERR_MASK;
  if (FTFE_FSTAT & errorMask)
  {
    // Clear errors and return false to signal an error
    FTFE_FSTAT = errorMask;
    return false;
  }

  // No errors
  return true;
}

/* @brief Write Phrase To Flash memory
 *
 * @param address - The address of the to be written to
 * @param phrase - The data to write to flash
 * @return bool - TRUE if the memory was succesfully written to
 */
static bool WritePhrase(const uint32_t address, const uint64_t phrase)
{
  // Build command
  TFCCOB command;
  command.command = PROGRAM_PHRASE;
  command.address = address;
  command.data = phrase;

  // Launch command
  return LaunchCommand(&command);
}

/* @brief Erase the sector in Flash memory
 *
 * @param address - The address of the sector to be erased
 * @return bool - TRUE if the sector was successfully erased
 */
static bool EraseSector(const uint32_t address)
{
  // Build command
  TFCCOB command;
  command.command = ERASE_SECTOR;
  command.address = address;

  // Launch command
  return LaunchCommand(&command);
}

/* @brief Modify the sector in Flash memory
 *
 * Bulk erases the sector and then writes a new phrase into memory
 *
 * @param address - The address of the sector to be modified
 * @param phrase - The phrase to write to flash
 * @return bool - TRUE if the phrase was successfully written
 */
static bool ModifySector(const uint32_t address, const uint64_t phrase)
{
  // Bulk erase the sector and then write the passed in phrase to memory
  return EraseSector(address) && WritePhrase(address, phrase);
}

/* @brief Writes data into the Flash memory
 *
 * Helper method for writing data of size N to Flash memory
 *
 * @param address - The flash address to write to
 * @param data - A pointer to the data to write
 * @param size - The size of the data (1, 2 or 4)
 * @return bool - TRUE if the data was successfully written to flash
 */
static bool Flash_WriteN(uint32_t const address, void const *data, uint8_t size)
{
  // Check address bounds. Should be in first phrase of sector 0.
  if (address < FLASH_DATA_START || (address + size) > FLASH_DATA_END || ((address % size) != 0))
    return false;

  // Make a copy (on the stack) of first phrase at sector 0 for modification in memory
  uint64_t phrase = _FP(FLASH_DATA_START);

  // Get the starting address for the portion of the copied phrase that we wish to modify
  uint32_t dataPtr = (uint32_t)&phrase + (address - FLASH_DATA_START);

  // Mutate a certain part of the (copied) phrase dependent on the passed in size
  switch (size)
  {
  case sizeof(uint8_t): // Modify Byte
      SET_DATA(uint8_t, dataPtr, data);
      break;
  case sizeof(uint16_t): // Modify Half-Word
      SET_DATA(uint16_t, dataPtr, data);
      break;
  case sizeof(uint32_t): // Modify Word
      SET_DATA(uint32_t, dataPtr, data);
      break;
  default:
      // Only writing 1, 2 or 4 bytes is supported
      return false;
  }

  // Write our mutated copy of phrase into flash memory
  return ModifySector(FLASH_DATA_START, phrase);
}

/*!
 * @addtogroup FLASH_module Flash module documentation.
 * @{
 */
/* MODULE FLASH */

bool Flash_Init(void)
{
  // Enable NAND Flash Clock
  SIM_SCGC3 |= SIM_SCGC3_NFC_MASK;

  return true;
}

bool Flash_AllocateVar(volatile void** variable, const uint8_t size)
{
  // A bitmask representing which of the 8 available bytes have been allocated
  static uint8_t availableBytes = 0x00;

  // Bounds check. Must be word sized or smaller
  if (size != sizeof(uint8_t) && size != sizeof(uint16_t) && size != sizeof(uint32_t))
    return false;

  /* Set the N LSBs to '1'
   * 2 => 0000 0011
   * 4 => 0000 1111 */
  uint8_t allocMask = (1 << size) - 1;

  // Search the phrase for an available block of the required size in memory
  for (uint8_t offset = 0; offset < 8; offset += size, allocMask <<= size)
  {
    // Check if this block of memory is available for being allocated to
    if ((allocMask & availableBytes) == 0)
    {
      // Claim these bytes
      availableBytes |= allocMask;

      // Set the address to the claimed chunk
      *variable = (void*)(FLASH_DATA_START + offset);
      return true;
    }
  }

  // We could not find a contiguous block
  return false;
}

bool Flash_Write32(volatile uint32_t* const address, const uint32_t data)
{
  // Write a word to memory
  Flash_WriteN((uint32_t)address, &data, sizeof(uint32_t));
}

bool Flash_Write16(volatile uint16_t* const address, const uint16_t data)
{
  // Write a half word to memory
  Flash_WriteN((uint32_t)address, &data, sizeof(uint16_t));
}

bool Flash_Write8(volatile uint8_t* const address, const uint8_t data)
{
  // Write a Byte to memory
  Flash_WriteN((uint32_t)address, &data, sizeof(uint8_t));
}

bool Flash_Erase(void)
{
  // Erase the flash sector
  return EraseSector(FLASH_DATA_START);
}

/*!
 * @}
 */
