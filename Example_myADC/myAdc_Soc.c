//###########################################################################
// Description
//! \addtogroup f2833x_example_list
//! <h1> ADC Start of Conversion (adc_soc)</h1>
//!
//! This ADC example uses ePWM1 to generate a periodic ADC SOC on SEQ1.
//! Two channels are converted, ADCINA3 and ADCINA2.
//!
//! \b Watch \b Variables \n
//! - Voltage1[10]	- Last 10 ADCRESULT0 values
//! - Voltage2[10]	- Last 10 ADCRESULT1 values
//! - ConversionCount	- Current result number 0-9
//! - LoopCount		- Idle loop counter
//
//
//###########################################################################
// $TI Release: F2833x/F2823x Header Files and Peripheral Examples V142 $
// $Release Date: November  1, 2016 $
// $Copyright: Copyright (C) 2007-2016 Texas Instruments Incorporated -
//             http://www.ti.com/ ALL RIGHTS RESERVED $
//###########################################################################

#include "DSP28x_Project.h"     // Device Headerfile and Examples Include File
#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File
#include "math.h"

// math constants
#define pi (3.14159)
// PWM period and CMPA parameters
#define EPWM_PERIOD (1875)			// TBCLK = 18750kHz, EPWMCLK = 10kHz
#define EPWM_CMPA_MAX (EPWM_PERIOD-1)// fully on over a PWM period
#define EPWM_CMPA_MIN (0)			// fully off over a PWM period
#define EPWM_OFF	(EPWM_PERIOD-1) // uncontrolled rectifier: all IGBTs off
// Maximum Dead Band values
#define EPWM_MAX_DB   0x005F
#define EPWM_MIN_DB   0x005F
// voltage external loop values
#define STAND_VD	(100)			// output DC voltage reference
#define UNC_VD		(40)	// <= UNC_VD, use uncontrolled rectifier
#define Kp_vl	10
#define Ki_vl 	0.001
// PLL values
#define PHASE_SHIFT (0)
#define Kp_pll 0.1
#define Ki_pll 0.007
#define OMG (100*pi)			// angular frequency of line voltage
// Loop control values
#define cntMAX (200)

// Loop control variables
Uint16 LoopCnt;

// PLL global variables
float theta, omega, Uq_p, Uq, delta;
float phaA, phaB, phaC;
float viewAB, viewBC, viewCA,AB90,BC90,CA90;

// ADC global variables
Uint16 UAB;									// AC line-line voltage Uab
Uint16 UBC;									// AC line-line voltage Ubc
Uint16 UCA;									// AC line-line voltage Uca
Uint16 UD;									// DC output voltage
Uint16 IA;									// AC phase A current
Uint16 IB;									// AC phase B current
Uint16 IC;									// AC phase C current


// voltage external loop global variables
Uint16 epwm_flag;
float Ia,Ib,Ic,Ud,Uab,Ubc,Uca;
// for GRAPH watching
float Vab[cntMAX],Vbc[cntMAX],Vca[cntMAX];
float iA[cntMAX],iB[cntMAX],iC[cntMAX];

// voltage external loop global variables
float Verr, Verr_p, Istand;

// current internal loop global variables
float Ia1, Ib1, Ic1, Aerr, Berr, Cerr;


// Prototype statements for functions found within this file.
void InitEPwm1();
void InitEPwm2();
void InitEPwm3();
void InitEPwm1Gpio(void);
void InitEPwm2Gpio(void);
void InitEPwm3Gpio(void);
void PLL(void);
float calcRMS(float U[cntMAX]);
__interrupt void adc_isr(void);


main()
{
// Step 1. Initialize System Control:
// PLL, WatchDog, enable Peripheral Clocks
// This example function is found in the DSP2833x_SysCtrl.c file.

	InitSysCtrl();
   EALLOW;
  #if (CPU_FRQ_150MHZ)     // Default - 150 MHz SYSCLKOUT
	#define ADC_MODCLK 0x3 // HSPCLK = SYSCLKOUT/2*ADC_MODCLK2 = 150/(2*3)   = 25.0 MHz
  #endif
  #if (CPU_FRQ_100MHZ)
	#define ADC_MODCLK 0x2 // HSPCLK = SYSCLKOUT/2*ADC_MODCLK2 = 100/(2*2)   = 25.0 MHz
  #endif
  EDIS;

   // Define ADCCLK clock frequency ( less than or equal to 25 MHz )
   // Assuming InitSysCtrl() has set SYSCLKOUT to 150 MHz
   EALLOW;
   SysCtrlRegs.HISPCP.all = ADC_MODCLK;
   EDIS;

// Step 2. Initialize GPIO:
// This example function is found in the DSP2833x_Gpio.c file and
// illustrates how to set the GPIO to it's default state.
// InitGpio();  // Skipped for this example

// For this case just init GPIO pins for ePWM1, ePWM2, ePWM3
// These functions are in the DSP2833x_EPwm.c file
  InitEPwm1Gpio();
  InitEPwm2Gpio();
  InitEPwm3Gpio();

// Step 3. Clear all interrupts and initialize PIE vector table:
// Disable CPU interrupts
   DINT;

// Initialize the PIE control registers to their default state.
// The default state is all PIE interrupts disabled and flags
// are cleared.
// This function is found in the DSP2833x_PieCtrl.c file.
   InitPieCtrl();

// Disable CPU interrupts and clear all CPU interrupt flags:
   IER = 0x0000;
   IFR = 0x0000;

// Initialize the PIE vector table with pointers to the shell Interrupt
// Service Routines (ISR).
// This will populate the entire table, even if the interrupt
// is not used in this example.  This is useful for debug purposes.
// The shell ISR routines are found in DSP2833x_DefaultIsr.c.
// This function is found in DSP2833x_PieVect.c.
   InitPieVectTable();

// Interrupts that are used in this example are re-mapped to
// ISR functions found within this file.
   EALLOW;  // This is needed to write to EALLOW protected register
   PieVectTable.ADCINT = &adc_isr;
   EDIS;    // This is needed to disable write to EALLOW protected registers

// Step 4. Initialize all the Device Peripherals:
// This function is found in DSP2833x_InitPeripherals.c
// InitPeripherals(); // Not required for this example
   InitAdc();  // For this example, init the ADC

  EALLOW;
  SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
  EDIS;
  InitEPwm1();
  InitEPwm2();
  InitEPwm3();
  EALLOW;

   SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
   EDIS;

// Step 5. User specific code, enable interrupts:

// Enable ADCINT in PIE
   PieCtrlRegs.PIEIER1.bit.INTx6 = 1;
   IER |= M_INT1; // Enable CPU Interrupt 1
   EINT;          // Enable Global interrupt INTM
   ERTM;          // Enable Global realtime interrupt DBGM

// Configure ADC
   AdcRegs.ADCMAXCONV.all = 0x6;       // Setup 7 conv's
   AdcRegs.ADCCHSELSEQ1.bit.CONV00 = 0x0; // Setup ADCINA1 as 1st SEQ1 conv.
   AdcRegs.ADCCHSELSEQ1.bit.CONV01 = 0x1; // Setup ADCINA2 as 2nd SEQ1 conv.
   AdcRegs.ADCCHSELSEQ1.bit.CONV02 = 0x2; // Setup ADCINA3 as 3rd SEQ1 conv.
   AdcRegs.ADCCHSELSEQ1.bit.CONV03 = 0x3; // Setup ADCINA4 as 4th SEQ1 conv.
   AdcRegs.ADCCHSELSEQ2.bit.CONV04 = 0x4; // Setup ADCINA5 as 5th SEQ1 conv.
   AdcRegs.ADCCHSELSEQ2.bit.CONV05 = 0x5; // Setup ADCINA6 as 6th SEQ1 conv.
   AdcRegs.ADCCHSELSEQ2.bit.CONV06 = 0x6; // Setup ADCINA7 as 7th SEQ1 conv.
   AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1;// Enable SOCA from ePWM to start SEQ1
   AdcRegs.ADCTRL2.bit.INT_ENA_SEQ1 = 1;  // Enable SEQ1 interrupt (every EOS)

// Assumes ePWM1 clock is already enabled in InitSysCtrl();
   EPwm1Regs.ETSEL.bit.SOCAEN = 1;        // Enable SOC on A group
   EPwm1Regs.ETSEL.bit.SOCASEL = 4;       // Select SOC from from CPMA on upcount
   EPwm1Regs.ETPS.bit.SOCAPRD = 1;        // Generate pulse on 1st event


// Wait for ADC interrupt
// Step 6. Customlize initialization
   Verr = 0; Verr_p = 0; Istand = 0;
   Ia1 = 0; Ib1 = 0; Ic1 = 0; Aerr = 0; Berr = 0; Cerr = 0;
   LoopCnt = 0;
   delta = 0;
   for(;;){}
}

__interrupt void  adc_isr(void)
{
		if(LoopCnt == cntMAX-1)
	   {
		   LoopCnt = 0;
	   }
	   else
	   {
		   LoopCnt++;
	   }

  // Read ADC register
  UAB = AdcRegs.ADCRESULT0 >>4;
  UBC = AdcRegs.ADCRESULT1 >>4;
  UCA = AdcRegs.ADCRESULT2 >>4;
  UD  = AdcRegs.ADCRESULT3 >>4;
  IA  = AdcRegs.ADCRESULT4 >>4;
  IB  = AdcRegs.ADCRESULT5 >>4;
  IC  = AdcRegs.ADCRESULT6 >>4;

  // ADC Uint16 -> float -> Actual parameter
  Ia  = (float) IA/4095*3;							// 12 bit accuracy, 0~3V input
  Ib  = (float) IB/4095*3;
  Ic  = (float) IC/4095*3;
  Uab = (float) UAB/4095*3;
  Ubc = (float) UBC/4095*3;
  Uca = (float) UCA/4095*3;
  Ud  = (float) UD/4095*3;

  Ia = -5.22*Ia+7.99;
  Ib = -5.16*Ib+8;
  Ic = -5.12*Ic+7.81;
  Uab = 244*Uab-376;
  Ubc = 240*Ubc-371;
  Uca = 252*Uca-394.8;
  Ud = 62.5*Ud+0.184;

  // 20180116 adjustion
  Ia = -0.9937*Ia+0.078;
  Ib = -0.9982*Ib+0.0492;
  Ic = -1.02*Ic-0.1608;
  Uab = 1.0237*Uab+3.486;
  Ubc = 1.1448*Ubc+18.311;
  Uca = 0.9481*Uca+11.842;
  Ud = 2.0048*Ud-3.6605;

  // Three phase PLL
  PLL();

  // for GRAPH watch
  Vab[LoopCnt] = Uab;
  Vbc[LoopCnt] = Ubc;
  Vca[LoopCnt] = Uca;
  iA[LoopCnt] = Ia;
  iB[LoopCnt] = Ib;
  iC[LoopCnt] = Ic;


  // Initial uncontrolled rectifier
  if(Ud<UNC_VD)						// uncontrolled rectifier: all IGBTs off
  {
	  epwm_flag = 0;

	  EPwm1Regs.CMPA.half.CMPA = EPWM_OFF;
	  EPwm1Regs.AQCTLA.bit.CAU = AQ_SET;
	  EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;
	  EPwm1Regs.AQCTLB.bit.CAU = AQ_SET;
	  EPwm1Regs.AQCTLB.bit.CAD = AQ_SET;
	  EPwm1Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;

	  EPwm2Regs.CMPA.half.CMPA = EPWM_OFF;
	  EPwm2Regs.AQCTLA.bit.CAU = AQ_SET;
	  EPwm2Regs.AQCTLA.bit.CAD = AQ_SET;
	  EPwm2Regs.AQCTLB.bit.CAU = AQ_SET;
	  EPwm2Regs.AQCTLB.bit.CAD = AQ_SET;
	  EPwm2Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;

	  EPwm3Regs.CMPA.half.CMPA = EPWM_OFF;
	  EPwm3Regs.AQCTLA.bit.CAU = AQ_SET;
	  EPwm3Regs.AQCTLA.bit.CAD = AQ_SET;
	  EPwm3Regs.AQCTLB.bit.CAU = AQ_SET;
	  EPwm3Regs.AQCTLB.bit.CAD = AQ_SET;
	  EPwm3Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;
  }
  else								// PWM rectifier
  {
	epwm_flag = 1;

	// Set actions
	EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;
	EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;
	EPwm1Regs.AQCTLB.bit.CAU = AQ_SET;
	EPwm1Regs.AQCTLB.bit.CAD = AQ_CLEAR;
	EPwm1Regs.DBCTL.bit.POLSEL = DB_ACTV_LOC;

	EPwm2Regs.AQCTLA.bit.CAU = AQ_CLEAR;
	EPwm2Regs.AQCTLA.bit.CAD = AQ_SET;
	EPwm2Regs.AQCTLB.bit.CAU = AQ_SET;
	EPwm2Regs.AQCTLB.bit.CAD = AQ_CLEAR;
	EPwm2Regs.DBCTL.bit.POLSEL = DB_ACTV_LOC;

	EPwm3Regs.AQCTLA.bit.CAU = AQ_CLEAR;
	EPwm3Regs.AQCTLA.bit.CAD = AQ_SET;
	EPwm3Regs.AQCTLB.bit.CAU = AQ_SET;
	EPwm3Regs.AQCTLB.bit.CAD = AQ_CLEAR;
	EPwm3Regs.DBCTL.bit.POLSEL = DB_ACTV_LOC;

	// control loop
	Verr = STAND_VD - Ud;
	Istand = Ki_vl*Verr + Kp_vl*(Verr-Verr_p) + Istand;
	// Saturation
	if(Istand>6)
	{
		Istand = 6;
	}
	else if(Istand<0)
	{
		Istand = 0;
	}
	Verr_p = Verr;

	Ia1 = Istand*cos(phaA);
	Ib1 = Istand*cos(phaB);
	Ic1 = Istand*cos(phaC);

	Aerr = Ia1 - Ia;
	Berr = Ib1 - Ib;
	Cerr = Ic1 - Ic;

	// relay control PWM output (IPM: low voltage level conduct)
	// leg A
	if(Aerr>0.1)
	{
	  EPwm1Regs.CMPA.half.CMPA = EPWM_CMPA_MAX;
	}
	else if(Aerr<-0.1)
	{
	  EPwm1Regs.CMPA.half.CMPA = EPWM_CMPA_MIN;
	}

	// leg B
	if(Berr>0.1)
	{
	  EPwm2Regs.CMPA.half.CMPA = EPWM_CMPA_MAX;
	}
	else if(Berr<-0.1)
	{
	  EPwm2Regs.CMPA.half.CMPA = EPWM_CMPA_MIN;
	}

	// leg C
	if(Cerr>0.1)
	{
	  EPwm3Regs.CMPA.half.CMPA = EPWM_CMPA_MAX;
	}
	else if(Cerr<-0.1)
	{
	  EPwm3Regs.CMPA.half.CMPA = EPWM_CMPA_MIN;
	}
  }




  // Reinitialize for next ADC sequence
  AdcRegs.ADCTRL2.bit.RST_SEQ1 = 1;         // Reset SEQ1
  AdcRegs.ADCST.bit.INT_SEQ1_CLR = 1;       // Clear INT SEQ1 bit
  PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;   // Acknowledge interrupt to PIE

  return;
}

void InitEPwm1()
{
	EPwm1Regs.TBPRD = EPWM_PERIOD;                         // Set timer period
	EPwm1Regs.TBPHS.half.TBPHS = 0x0000;            // Phase is 0
	EPwm1Regs.TBCTR = 0x0000;                       // Clear counter


	// Setup TBCLK
	EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up
	EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;        // Disable phase loading
	EPwm1Regs.TBCTL.bit.HSPCLKDIV = TB_DIV4;       // Clock ratio to SYSCLKOUT

	// Setup compare
	EPwm1Regs.CMPA.half.CMPA = EPWM_CMPA_MAX;

	// Set actions
	EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;
	EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;


	EPwm1Regs.AQCTLB.bit.CAU = AQ_SET;
	EPwm1Regs.AQCTLB.bit.CAD = AQ_CLEAR;

	// Active high complementary PWMs - Setup the deadband
	EPwm1Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
	EPwm1Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;
	EPwm1Regs.DBCTL.bit.IN_MODE = DBA_ALL;
	EPwm1Regs.DBRED = EPWM_MAX_DB;
	EPwm1Regs.DBFED = EPWM_MAX_DB;
}

void InitEPwm2()
{
   EPwm2Regs.TBPRD = EPWM_PERIOD;                        // Set timer period
   EPwm2Regs.TBPHS.half.TBPHS = 0x0000;           // Phase is 0
   EPwm2Regs.TBCTR = 0x0000;                      // Clear counter

   // Setup TBCLK
   EPwm2Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up
   EPwm2Regs.TBCTL.bit.PHSEN = TB_DISABLE;        // Disable phase loading
   EPwm2Regs.TBCTL.bit.HSPCLKDIV = TB_DIV4;       // Clock ratio to SYSCLKOUT

   // Setup compare
   EPwm2Regs.CMPA.half.CMPA = EPWM_CMPA_MAX;

   // Set actions
   EPwm2Regs.AQCTLA.bit.CAU = AQ_CLEAR;
   EPwm2Regs.AQCTLA.bit.CAD = AQ_SET;


   EPwm2Regs.AQCTLB.bit.CAU = AQ_SET;
   EPwm2Regs.AQCTLB.bit.CAD = AQ_CLEAR;

   // Active Low complementary PWMs - setup the deadband
   EPwm2Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
   EPwm2Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;
   EPwm2Regs.DBCTL.bit.IN_MODE = DBA_ALL;
   EPwm2Regs.DBRED = EPWM_MAX_DB;
   EPwm2Regs.DBFED = EPWM_MAX_DB;
}

void InitEPwm3()
{
   EPwm3Regs.TBPRD = EPWM_PERIOD;                  // Set timer period
   EPwm3Regs.TBPHS.half.TBPHS = 0x0000;            // Phase is 0
   EPwm3Regs.TBCTR = 0x0000;                       // Clear counter


   // Setup TBCLK
   EPwm3Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up
   EPwm3Regs.TBCTL.bit.PHSEN = TB_DISABLE;        // Disable phase loading
   EPwm3Regs.TBCTL.bit.HSPCLKDIV = TB_DIV4;       // Clock ratio to SYSCLKOUT

   // Setup compare
   EPwm3Regs.CMPA.half.CMPA = EPWM_CMPA_MAX;

   // Set actions
   EPwm3Regs.AQCTLA.bit.CAU = AQ_CLEAR;
   EPwm3Regs.AQCTLA.bit.CAD = AQ_SET;


   EPwm3Regs.AQCTLB.bit.CAU = AQ_SET;
   EPwm3Regs.AQCTLB.bit.CAD = AQ_CLEAR;

   // Active high complementary PWMs - Setup the deadband
   EPwm3Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
   EPwm3Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;
   EPwm3Regs.DBCTL.bit.IN_MODE = DBA_ALL;
   EPwm3Regs.DBRED = EPWM_MAX_DB;
   EPwm3Regs.DBFED = EPWM_MAX_DB;
}

void
InitEPwm1Gpio(void)
{
    EALLOW;

    //
    // Enable internal pull-up for the selected pins
    // Pull-ups can be enabled or disabled by the user.
    // This will enable the pullups for the specified pins.
    // Comment out other unwanted lines.
    //
    GpioCtrlRegs.GPAPUD.bit.GPIO0 = 0;    // Enable pull-up on GPIO0 (EPWM1A)
    GpioCtrlRegs.GPAPUD.bit.GPIO1 = 0;    // Enable pull-up on GPIO1 (EPWM1B)

    //
    // Configure ePWM-1 pins using GPIO regs
    // This specifies which of the possible GPIO pins will be ePWM1 functional
    // pins. Comment out other unwanted lines.
    //
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;   // Configure GPIO0 as EPWM1A
    GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1;   // Configure GPIO1 as EPWM1B

    EDIS;
}

//
// InitEPwm2Gpio - This function initializes GPIO pins to function as ePWM2
//
void
InitEPwm2Gpio(void)
{
    EALLOW;

    //
    // Enable internal pull-up for the selected pins
    // Pull-ups can be enabled or disabled by the user.
    // This will enable the pullups for the specified pins.
    // Comment out other unwanted lines.
    //
    GpioCtrlRegs.GPAPUD.bit.GPIO2 = 0;    // Enable pull-up on GPIO2 (EPWM2A)
    GpioCtrlRegs.GPAPUD.bit.GPIO3 = 0;    // Enable pull-up on GPIO3 (EPWM3B)

    //
    // Configure ePWM-2 pins using GPIO regs
    // This specifies which of the possible GPIO pins will be ePWM2 functional
    // pins. Comment out other unwanted lines.
    //
    GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1;   // Configure GPIO2 as EPWM2A
    GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1;   // Configure GPIO3 as EPWM2B

    EDIS;
}

//
// InitEPwm3Gpio - This function initializes GPIO pins to function as ePWM3
//
void
InitEPwm3Gpio(void)
{
    EALLOW;

    //
    // Enable internal pull-up for the selected pins
    // Pull-ups can be enabled or disabled by the user.
    // This will enable the pullups for the specified pins.
    // Comment out other unwanted lines.
    //
    GpioCtrlRegs.GPAPUD.bit.GPIO4 = 0;    // Enable pull-up on GPIO4 (EPWM3A)
    GpioCtrlRegs.GPAPUD.bit.GPIO5 = 0;    // Enable pull-up on GPIO5 (EPWM3B)

    //
    // Configure ePWM-3 pins using GPIO regs
    // This specifies which of the possible GPIO pins will be ePWM3 functional
    // pins. Comment out other unwanted lines.
    //
    GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 1;   // Configure GPIO4 as EPWM3A
    GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 1;   // Configure GPIO5 as EPWM3B

    EDIS;
}


void PLL(void)
{

	// ���� transformation & dq transformation
//	Ud = 2/3*(Uab-1/2*Ubc-1/2*Uca)*cos(theta)+sqrt(3)/3*(Ubc-Uca)*sin(theta);
	Uq = sqrt(3)/3*(Ubc-Uca)*cos(theta)-2/3*(Uab-1/2*Ubc-1/2*Uca)*sin(theta);

//	delta = Kp_pll*(Uq-Uq_p)+Ki_pll*Uq+delta;
//	omega = OMG + delta;

	omega += Kp_pll*(Uq-Uq_p)+Ki_pll*Uq;
	// Saturation
	if(omega<300)
	{
		omega = 300;
	}
	else if(omega>350)
	{
		omega = 350;
	}

	theta = theta + omega/10000;

	// theta must be in the range of [0,2pi)
	theta = fmod(theta,2*pi);

	Uq_p = Uq;

	phaA = theta - pi/6 -PHASE_SHIFT;
	if(phaA<0)
	{
		phaA += 2*pi;
	}
	else if(phaA > 2*pi)
	{
		phaA -= 2*pi;
	}
	phaB = theta - pi/6 - 2*pi/3 - PHASE_SHIFT;
	if(phaB<0)
	{
		phaB += 2*pi;
	}
	else if(phaB > 2*pi)
	{
		phaB -= 2*pi;
	}
	phaC = theta - pi/6 - 4*pi/3 - PHASE_SHIFT;
	if(phaC<0)
	{
		phaC += 2*pi;
	}
	else if(phaC > 2*pi)
	{
		phaC -= 2*pi;
	}

	if(theta <= omega/10000)
	{
		viewAB = Uab;
		viewBC = Ubc;
		viewCA = Uca;
	}
	if(fabs(theta-pi*2/3) <= omega/10000)
	{
		AB90 = Uab;
		BC90 = Ubc;
		CA90 = Uca;
	}

	return;

}
