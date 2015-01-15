/*
 * A. Jaworowski
 * Object: Measure temperature with the internal temp-sensor.
 * Inspired from
 * http://www.embedds.com/introducing-to-stm32-adc-programming-part2/
 * Example files for STM32L1xx_StdPeriph_Examples
 *
 * 2015-01-15 Added conversion from ADC output to Celcius (with copensation).
 * Typical values for room temperature is 25 C.
 *
 * Observe that the best solution to the problem below is to skip
 * HSI. The simple answer is that its already started when new
 * Embedded project is initiated, see stm32l1xx.h and HSI
 *
 * 2015-01-15 Solution: Either comment out HSI or (better? - NO!) add:
 * ADC_CommonInitTypeDef ADC_CommonInitStruct;
 * ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div1;
 * ADC_CommonInit(&ADC_CommonInitStruct);
 *
 * 2015-01-15 Problem: I do only get two measurements with contiuous
 * measurements. EOC flag in SR of ADC1 newer gets high after second
 * conversion.
 *
 * 2015-01-10 Initial version
 * I have made some changes..
 * - ADC init is quite different
 * - ADC calibration removed
 * - just focusing on the ADC-value
 *
 * Test results: ...values between 6000 - 14000...
 */
/* **
  ******************************************************************************
  * @file    main.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    11-February-2014
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>

//Temp sensor calibration data, see datasheet and
//https://my.st.com/public/STe2ecommunities/mcu/Lists/cortex_mx_stm32/Flat.aspx?RootFolder=%2Fpublic%2FSTe2ecommunities%2Fmcu%2FLists%2Fcortex_mx_stm32%2FInternal%20temperatur%20sensor&FolderCTID=0x01200200770978C69A1141439FE559EB459D7580009C4E14902C3CDE46A77F0FFD06506F5B&currentviews=1508
//Observe that these value must be corrected for 3.3V
const uint16_t VDD = 33;   //3.3 V but avoiding float
const uint16_t VTREF = 30; //3.0 V used for calibration of temperature sensor
const uint16_t TS_CAL1 = 674; //*((uint16_t*)0x1ff800fa) - Calibration ADC value at 30 C
const uint16_t TS_CAL2 = 857; //*((uint16_t*)0x1ff800fe) - Calibration ADC value at 110 C

static __IO uint32_t TimingDelay;
RCC_ClocksTypeDef RCC_Clocks;
uint16_t AD_value;
int16_t TemperatureC;
uint8_t BlinkSpeed = 0;

int main(void)
{
	ADC_InitTypeDef ADC_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStruct;

	/* SysTick end of count event each 1ms */
	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);

	/* HSI is actually turned on when new Embedded project is started
	 * so this should not be necessary!
	 * See stm32l1xx.h and HSI (Malin 2015-01-15)
	 */
//	/* Enable the HSI - the ADC internal clock running at 16 MHz
//	 * Was unsure if needed - it is!*/
//	RCC_HSICmd(ENABLE);
//	/* Wait until HSI oscillator is ready */
//	while(RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);
//
//	ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div1;
//	ADC_CommonInit(&ADC_CommonInitStruct);

	/* Enable ADC1 clock. ADC is connected to the APB2 bus.
	 * For some reason there is no ADC0 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	//ADC1 configuration
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	//We will convert single channel only
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	//we will convert continuously
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	//select no external triggering
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	//select no external triggering
	//ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	//right 12-bit data alignment in ADC data register
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	//single channel conversion
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	//load structure values to control and status registers
	ADC_Init(ADC1, &ADC_InitStructure);
	//Enable ADC1
	ADC_Cmd(ADC1, ENABLE);

	//Wake up temperature sensor
	//AJ this function exists in stm32l1xx_adc.c from
	//the StdPeriph_Driver lib
	//There you will find information about setting up
	//temperature measurements
	ADC_TempSensorVrefintCmd(ENABLE);
	//ADC1 channel16 configuration
	//Datasheet says 4 us sample time for T-sensor
	//With 32MHz that should be 128 cycles...but there is no such choice
	//and rank=1 which doesn't matter in single mode
	ADC_RegularChannelConfig(ADC1, ADC_Channel_16, 1,  ADC_SampleTime_384Cycles); //Should be possible to run with fewer cycles
	/* Wait until the ADC1 is ready */
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_ADONS) == RESET) {};
	//Start ADC1 Software Conversion
	ADC_SoftwareStartConv(ADC1);

	while (1)
	{
		//wait for conversion complete
		while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC)){}
		//Get AD raw value and compensate for VDD
		AD_value = ADC_GetConversionValue(ADC1);
		AD_value = (uint16_t)(VDD * AD_value / VTREF);
		//ADC_ClearFlag of ADC_FLAG_EOC is not needed since we read from DR, see reference manual
		TemperatureC = (int16_t)((80*100/(TS_CAL2 - TS_CAL1)*(AD_value - TS_CAL1))/100 + 30);
		printf("ADC value and temperature: %d\t\t%dC\n", AD_value, TemperatureC);
		Delay(1000);
	}
}

/**
* @brief  Inserts a delay time.
* @param  nTime: specifies the delay time length, in 1 ms.
* @retval None
*/
void Delay(__IO uint32_t nTime)
{
  TimingDelay = nTime;

  while(TimingDelay != 0);
}

/**
* @brief  Decrements the TimingDelay variable.
* @param  None
* @retval None
*/
/* This function is controlled by an interrupt handler for the timer SysTick.
 * Find the handler in stm32l1xx_it.c, just as for the button.
 * */
void TimingDelay_Decrement(void)
{
  if (TimingDelay != 0x00)
  {
    TimingDelay--;
  }
}

#ifdef  USE_FULL_ASSERT

/**
* @brief  Reports the name of the source file and the source line number
*         where the assert_param error has occurred.
* @param  file: pointer to the source file name
* @param  line: assert_param error line source number
* @retval None
*/
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
  ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
* @}
*/


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
