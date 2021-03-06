/*
 *
 * RepRap, The Replicating Rapid Prototyper Project
 *
 * http://reprap.org/
 *
 * RepRap is copyright (C) 2005-6 University of Bath, the RepRap
 * researchers (see the project's People webpage), and other contributors.
 *
 * RepRap is free; you can redistribute it and/or modify it under the
 * terms of the GNU Library General Public Licence as published by the
 * Free Software Foundation; either version 2 of the Licence, or (at your
 * option) any later version.
 *
 * RepRap is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * Licence for more details.
 *
 * For this purpose the words "software" and "library" in the GNU Library
 * General Public Licence are taken to mean any and all computer programs
 * computer files data results documents and other copyright information
 * available from the RepRap project.
 *
 * You should have received a copy of the GNU Library General Public
 * Licence along with RepRap (in reports, it will be one of the
 * appendices, for example); if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or see
 *
 * http://www.gnu.org/
 *
 */

#include "extruder.h"
#include "serial.h"

typedef unsigned int config;
config at 0x2007 __CONFIG = _CP_OFF &
 _WDT_OFF &
 _BODEN_OFF &
 _PWRTE_ON &
 _INTRC_OSC_NOCLKOUT &
 _MCLRE_OFF &
 _LVP_OFF;

byte deviceAddress = PORT;
volatile byte interruptTemp;  //will be set, if an interrupt occures


static void isr() interrupt 0 {
  interruptTemp = 1; //set flag, to check if the measurement is finishing during our isr

  serialInterruptHandler();

  if (RBIF) 
    motorTick();  //should not occur in UNIVERSAL_PCB

  if (TMR1IF) {
    timerTick();
    TMR1IF = 0;
  }
  if (C2OUT) {
    interruptTemp = 0; //measurement is not finished
  }
}

void init1()
{
  INTCON = BIN(00000000);     // Interrupts disabled
  PIR1 = 0;                   // Clear peripheral interrupt flags
  PIE1 = BIN(00000000);       // All peripheral interrupts initially disabled

  OPTION_REG = BIN(01010111); // Disable TMR0 on RA4, 1:256 Prescaler for TMR0, pullups on

  //serial communication
  SPBRG = 12;                 // 12 = ~19200 baud @ 4MHz
  
  TXSTA = BIN(00000100);      // 8 bit high speed 
  RCSTA = BIN(10000000);      // Enable port for 8 bit receive
  CREN = 1;  // Start reception
  TXEN = 1;  // Enable transmit
  RCIE = 1;  // Enable receive interrupts

// RB0 is Extrude speed sensor
// RB1 is Rx
// RB2 is Tx 
// RB3 is PWM output to L298                                                                                       
// RB4 is L298 2 (Extrude)
// RB5 is L298 1 (Extrude)
// RB6 is the cooling fan
// RB7 is not used
  RBIE = 1;  // Enable RB port change interrupt 
             //(should not occur in UNIVERSAL_PCB, except if connector 11 is used)
  PEIE = 1;  // Peripheral interrupts on
#ifdef UNIVERSAL_PCB
  TRISA = BIN(11000010) | PORTATRIS;  // Turn off A/D lines,
                                      // but set others as required  
  //TRISB = BIN(11000111);
  TRISB = BIN(10000111);
  PORTA = 0;
  PORTB = BIN(10000001);   // Fan off; pullup on RB7
#else
  TRISA = BIN(11000010) | PORTATRIS;  // Turn off A/D lines,
                                      // but set others as required  
  TRISB = BIN(11000110);      // Port B outputs, except 1/2 for serial and
                              // RB7 for optointerrupter input
                              // RB6 for material out detector
                              // RB0 for heater controller output
  PORTA = 0;
  PORTB = BIN(11000000);  // Pullup on RB6,RB7 for opto-inputs
#endif

  CMCON = BIN(00000010);   // Comparator: compare RA0 to int. ref.

  TMR1IE = 0;
  T1CON = BIN(00000000);   // Timer 1 in clock mode with 1:1 scale
  TMR1IE = 1;              // Enable timer interrupt
  TMR1ON = 1;

  PR2 = PWMPeriod;          // Initial PWM period
  CCP1CON = BIN(00001100);  // Enable PWM mode
  CCPR1L = 0;               // Start turned off
  
  T2CON = BIN(00000100);    // Enable timer 2 and set prescale to 1
  GIE = 1;   // Now turn on interrupts
}

/* for debugging
void waitSec() {
  int i;
  for (i=30000; i > 0; i--) {
    delay_10us();
    delay_10us();
    delay_10us();
  }
}
*/

void main() {
  init2();  // Order is important here, otherwise interrupts will occur
            // before initialisation.  Once sdcc bugs are fixed, this
            // will not matter.
  init1();
  serial_init();

  // Clear up any boot noise from the TSR
  GIE=0;
  uartTransmit(0);
  uartTransmit(0);
  GIE=1;

  for(;;) {
    if (packetReady()) {
      processCommand();
      releaseLock();
    }  

    checkTemperature();

    delay_10us();
//not used:
//    clearwdt();
  }
}
