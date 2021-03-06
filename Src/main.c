/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"
#include "adc.h"
#include "dma.h"
#include "fatfs.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "stm32_adafruit_lcd.h"
#include "stm32_adafruit_sd.h"
#include "usbd_cdc_if.h"
#include "fatfs_storage.h"
#include "unistd.h"
#include "mpu9250.h"
#include "math.h"
#include "rc.h"
#include "servo.h"
#include "controller.h"
#include "flash.h"
#include "config.h"
#include "led.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

uint8_t counter = 0;
uint32_t failsafe_counter = 100;
uint8_t led_blink_counter = 0;
char* pDirectoryFiles[MAX_BMP_FILES];
uint8_t res;
uint16_t usb_res;
FRESULT fres;
DIR directory;
FATFS SD_FatFs; /* File system object for SD card logical drive */
float volt1 = 0.0f;
uint32_t free_ram;
char buf[50] = { 0 };
char buf2[100] = { 0 };
const uint8_t flash_top = 255;
uint32_t free_flash;
uint32_t tick, prev_tick, dt;
float vx = 0.0f, vy = 0.0f;
HAL_StatusTypeDef hal_res;
uint32_t idle_counter;
float cp_pid_vars[9];
uint8_t i, armed = 0, back_armed = 0;
uint8_t indexer = 0;
uint32_t millis[2];
uint32_t micros[2];
uint8_t low_volt = 0;
uint8_t warning = 0;
uint8_t average_counter = 0;
uint8_t acc_cal_counter = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USB_DEVICE_Init();
  MX_SPI2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();

  /* USER CODE BEGIN 2 */

#ifdef HAVE_SD_CARD
    MX_FATFS_Init();
#endif

    led_set_rainbow(0, NR_COLORS, 128);

    // Reset USB on maple mine clone to let the PC host enumerate the device
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

    // read settings into flash_buf and validate data
    check_settings_page();
    // feed variables from settings
    analyze_settings();

    if (esc_mode == ONES)
    {
        MX_OneShot_TIM2_Init();
    }
    else
    {
        MX_TIM2_Init();
    }

    if (p_settings->receiver == SBUS)
    {
        // alternative SBUS (Futaba) aka DBUS (Jeti)
        MX_SBUS_USART1_UART_Init();
        UART_RxCpltCallback = &HAL_UART_RxCpltCallback_SBUS;
        UART_ErrorCallback = &HAL_UART_ErrorCallback_SBUS;
        // Request first 25 bytes s-bus frame from uart, uart_data becomes filled per interrupts
        // Get callback if ready. Callback function starts next request
        HAL_UART_Receive_IT(&huart1, (uint8_t*) uart_data, 25);
    }
    else if (p_settings->receiver == SRXL)
    {
        // alternative SRXL16 (Multiplex) aka UDI16 (Jeti)
        MX_USART1_UART_Init();
        UART_RxCpltCallback = &HAL_UART_RxCpltCallback_SRXL;
        UART_ErrorCallback = &HAL_UART_ErrorCallback_SRXL;
        // Request first 35 bytes srxl frame from uart, uart_data becomes filled per interrupts
        // Get callback if ready. Callback function starts next request
        HAL_UART_Receive_IT(&huart1, (uint8_t*) uart_data, 35);
    }

    // BSP_MPU_Init() Parameter:
    // no sample rate divider (0+1, 1. parameter), gyro and accel lowpass filters see below
    // gyro dlpf, 2. parameter:
    // 0  250 Hz
    // 1  184 Hz
    // 2   92 Hz
    // 3   41 Hz
    // 4   20 Hz
    // 5   10 Hz
    // 6    5 Hz

    // acc dlpf, 3. parameter
    // 0  460 Hz
    // 1  184 Hz
    // 2   92 Hz
    // 3   41 Hz
    // 4   20 Hz
    // 5   10 Hz
    // 6    5 Hz

    BSP_MPU_Init(0, 2, 2);
    HAL_Delay(4000); // wait for silence after batteries plugged in
    BSP_MPU_GyroCalibration();
    BSP_MPU_AccCalibration(p_settings->acc_offset);

#ifdef HAVE_DISPLAY
    BSP_LCD_Init();
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_SetRotation(0);
    BSP_LCD_SetFont(&Font12);
    BSP_LCD_SetTextColor(LCD_COLOR_YELLOW);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
#endif

#ifdef HAVE_SD_CARD
    //############ init SD-card, signal errors by LED ######################
    res = BSP_SD_Init();

    if (res != BSP_SD_OK)
    {
        Error_Handler();
    }
    else
    {
        fres = f_mount(&SD_FatFs, (TCHAR const*) "/", 0);
        sprintf(buf, "f_mount: %d", fres);

        if (fres != FR_OK)
        {
            Error_Handler();
        }
        else
        {
            for (i = 0; i < MAX_BMP_FILES; i++)
            {
                pDirectoryFiles[i] = malloc(11);
            }
        }
    }

    if (res == BSP_SD_OK)
    {
        TFT_DisplayImages(0, 0, "PICT1.BMP", buf);
    }
#endif

    HAL_TIM_Base_Start(&htim4);

    // start servo pulse generation no pulse finished interrupt generation
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

    // not to be enabled until BSP_MPU_GyroCalibration
    // Period elapsed callback sets flag PeriodElapsed
    // timer 2 generates servo pulses while timer 4 is synchronizing the main loop
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);

    // timer 4 is auto started by and synchronized to timer 2 and has n times the frequency of timer 2
    // started by software now, as auto started by enable trigger from timer 2 has no fixed phase. why?
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);

    // start DMA transferring circular aCCValue_Buffer values to timer3 CCR
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, (uint32_t *) aCCValue_Buffer, NR_LEDS * 24 + 8);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

        // reference probe
        //millis[0] = HAL_GetTick();
        //micros[0] = SysTick->VAL;
        if (PeriodElapsed == 1) // 1 kHz of Timer 4, update events (this flag) are synchronized from IMU Int trigger pin
        {
            PeriodElapsed = 0;

            counter++;
            failsafe_counter++;

            if (RC_RECEIVED == 1)
            {
                RC_RECEIVED = 0;
                failsafe_counter = 0;
            }

            BSP_MPU_read_rot();
            BSP_MPU_read_acc();

            // use int values se_roll, se_nick, se_gier as index to map different orientations of the sensor
            BSP_MPU_updateIMU(ac[se_roll] * se_roll_sign, ac[se_nick] * se_nick_sign, ac[se_gier] * se_gier_sign,
                              gy[se_roll] * se_roll_sign, gy[se_nick] * se_nick_sign, gy[se_gier] * se_gier_sign, 1.0f); // dt 1ms

            // then it comes out here properly mapped because Quaternions already changed axes
            BSP_MPU_getEuler(&ang[roll], &ang[nick], &ang[gier]);

            // armed only if arm switch on + not failsafe + not usb connected
            //if (channels[rc_arm] > H_TRSH && failsafe_counter < 40) // use for test performance (armed) while usb connected
            if (channels[rc_arm] > H_TRSH && failsafe_counter < 40 && hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) // armed, flight mode
            {
                armed = 1;

                average_counter++;

                if (channels[rc_mode] < L_TRSH)
                {
                    // attitude hold mode (acro, rate controlled only)
                    // full stick equals 250 degrees per second with rate[x] of 8.192 (2048 / 250)
                    // gy range goes from 0 to +-1000 [DPS]
                    diff_roll_rate = gy[se_roll] * se_roll_sign * rate[se_roll] - (float) channels[rc_roll] + MIDDLE_POS; // native middle positions
                    diff_nick_rate = gy[se_nick] * se_nick_sign * rate[se_nick] - (float) channels[rc_nick] + MIDDLE_POS; // rc from -2048 to +2048
                    diff_gier_rate = gy[se_gier] * se_gier_sign * rate[se_gier] + (float) channels[rc_gier] - MIDDLE_POS; // control reversed, gy right direction

                    roll_set += pid(x, scale_roll, diff_roll_rate, pid_vars[RKp], pid_vars[RKi], pid_vars[RKd], 1.0f);
                    nick_set += pid(y, scale_nick, diff_nick_rate, pid_vars[NKp], pid_vars[NKi], pid_vars[NKd], 1.0f);
                    gier_set += pid(z, 1.0f, diff_gier_rate, pid_vars[GKp], pid_vars[GKi], pid_vars[GKd], 1.0f);

                }
                else // mode 2 and mode 3 are the same currently
                {
                    // level hold mode (level controlled)
                    // full stick translates to 45 degrees shifted
                    // ang[x] in radiant
                    diff_roll_ang = ang[roll] * 2048.0f / (M_PI / 4.0f) - (float) channels[rc_roll] + MIDDLE_POS; // native middle positions
                    diff_nick_ang = ang[nick] * 2048.0f / (M_PI / 4.0f) - (float) channels[rc_nick] + MIDDLE_POS; // rc from -2048 to +2048

                    // calculate errors of roll rate and nick rate from errors of roll angle and nick angle scaled by 1.0f (P only control)
                    // i.e. using angle error as setpoint for rate instead of input from RC
                    // gier is always rate controlled
                    // full stick equals 250 degrees per second with rate of 8.192 (2048 / 250)
                    diff_roll_rate = gy[se_roll] * se_roll_sign * rate[se_roll] + diff_roll_ang;
                    diff_nick_rate = gy[se_nick] * se_nick_sign * rate[se_nick] + diff_nick_ang;
                    diff_gier_rate = gy[se_gier] * se_gier_sign * rate[se_gier] + (float) channels[rc_gier] - MIDDLE_POS; // control reversed, gy right direction

                    roll_set += pid(x, scale_roll, diff_roll_rate, l_pid_vars[RKp], l_pid_vars[RKi], l_pid_vars[RKd], 1.0f);
                    nick_set += pid(y, scale_nick, diff_nick_rate, l_pid_vars[NKp], l_pid_vars[NKi], l_pid_vars[NKd], 1.0);
                    gier_set += pid(z, 1.0f, diff_gier_rate, l_pid_vars[GKp], l_pid_vars[GKi], l_pid_vars[GKd], 1.0f);
                }

                // scale thrust channel to have space for governor if max thrust is set
                thrust_set = rintf((float) channels[rc_thrust] * 0.8f) + LOW_OFFS; // native middle position and 134 % are set, rc from 0 to 4095

                if (ServoPeriodElapsed == 1) // In OneShot mode Update Events of timer2 (this Flag) is synchronized from timer 4
                {
                    ServoPeriodElapsed = 0;

                    if (esc_mode == STD)  // average control values from n Periods of control calculation
                    {
                        roll_set /= average_counter; // average_counter is 3 almost always
                        nick_set /= average_counter;
                        gier_set /= average_counter;

                        average_counter = 0;
                    }

                    control(thrust_set, roll_set, nick_set, gier_set, esc_mode);

                    // Preload is on. Values taken at next update event
                    TIM2->CCR1 = servos[0];
                    TIM2->CCR2 = servos[1];
                    TIM2->CCR3 = servos[2];
                    TIM2->CCR4 = servos[3];

                    // reset sum of control values
                    roll_set = 0;
                    nick_set = 0;
                    gier_set = 0;
                }
            }
            else // not armed or fail save or USB connected, motor stop except if motor test running
            {
                armed = 0;

                // stop motors even with motor test running if USB cable gets disconnected
                if (rcv_motors == 0 || hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
                {
                    halt_reset();
                }

                // prevent overflow after many hours
                failsafe_counter = 100;

                // message over VCP arrived. From config tool?
                if (cdc_received == 1)
                {
                    // receiving 1k data for settings flash page in one or more iterations
                    if (rcv_settings == 1)
                    {
                        receive_settings();
                    }
                    else
                    {   // read commands from config tool
                        config_state_switch((const char *) received_data);
                    }
                }

                // sending 1k data of settings flash page
                if (snd_settings == 1)
                {
                    HAL_Delay(300); // wait for live data on wire gets swallowed before send

                    send_settings((uint8_t*) flash_buf);
                }
                else if (snd_channels == 1 && channels_receipt == 1)  // sending channels
                {
                    send_channels();
                }
                else if (snd_live == 1 && live_receipt == 1)
                {
                    send_live();
                }

                if (snd_live == 0)
                {
                    //free_ram = (0x20000000 + 1024 * 20) - (uint32_t) sbrk((int)0);
                    //sprintf(buf, "free: %ld", free_ram);
                    //free_flash = (0x8000000 + 1024 * 128) - (uint32_t) &flash_top;
                    //sprintf(buf, "free: %ld", free_flash);
                    //BSP_LCD_DisplayCLRStringAtLine(12, (uint8_t *) buf, LEFT_MODE, LCD_COLOR_YELLOW);
                }

            } // not armed

            // do it in time pieces
            if (counter >= 8) // Program With Display, Flight Display and LED State Change Slot
            {
                counter = 0;

                if (armed == 0)
                {
                    // transition to motor stop clear screen
                    if (back_armed - armed > 0)
                    {
                        led_set_rainbow(0, NR_COLORS, 128);
#ifdef HAVE_DISPLAY
                        BSP_LCD_SetRotation(0);
                        BSP_LCD_Clear(LCD_COLOR_BLACK);
#endif
                        back_armed = armed;
                    }
#ifdef HAVE_DISPLAY
                    // transition of beeper momentary switch (channels[rc_beep]) detect
                    if (channels[rc_beep] - back_channels[rc_beep] > TRANS_OFFS)
                    {
                        // if program switch (channels[rc_prog]) is off
                        // increment indexer
                        if (channels[rc_prog] < L_TRSH)
                        {
                            if (indexer < 8)
                            {
                                indexer++;
                            }
                            else
                            {
                                indexer = 0;
                            }

                        }
                        // else if program switch (channels[rc_prog]) full on
                        // copy pid_vars from ram to upper flash page
                        else if (channels[rc_prog] > H_TRSH)
                        {
                            p_settings = (settings *) flash_buf;
                            read_flash_vars((uint32_t *) flash_buf, 256, 0);

                            for (i = 0; i < 9; i++)
                            {
                                // write ram vars to flash buffer
                                p_settings->pidvars[i] = pid_vars[i];
                                p_settings->l_pidvars[i] = l_pid_vars[i];
                            }

                            if (erase_flash_page() != HAL_OK)
                            {
                                Error_Handler();
                            }
                            else
                            {
                                if (write_flash_vars((uint32_t*) flash_buf, 256, 0) != HAL_OK)
                                {
                                    Error_Handler();
                                }
                            }
                        }
                    }

                    back_channels[rc_beep] = channels[rc_beep];

                    // do not disturb data flows
                    if (snd_live == 0 && rcv_motors == 0 && snd_channels == 0 && rcv_settings == 0 && snd_settings == 0)
                    {
                        if (channels[rc_mode] < L_TRSH)
                        {
                            // show and program by RC the current PID values
                            draw_program_pid_values(1, pid_vars[RKp], "Roll   Kp: %3.5f", indexer, 1);
                            draw_program_pid_values(2, pid_vars[RKi], "Roll   Ki: %3.5f", indexer, 1);
                            draw_program_pid_values(3, pid_vars[RKd], "Roll   Kd: %3.5f", indexer, 1);
                            draw_program_pid_values(4, pid_vars[NKp], "Nick   Kp: %3.5f", indexer, 1);
                            draw_program_pid_values(5, pid_vars[NKi], "Nick   Ki: %3.5f", indexer, 1);
                            draw_program_pid_values(6, pid_vars[NKd], "Nick   Kd: %3.5f", indexer, 1);
                            draw_program_pid_values(7, pid_vars[GKp], "Gier   Kp: %3.5f", indexer, 1);
                            draw_program_pid_values(8, pid_vars[GKi], "Gier   Ki: %3.5f", indexer, 1);
                            draw_program_pid_values(9, pid_vars[GKd], "Gier   Kd: %3.5f", indexer, 1);
                        }
                        else
                        {
                            // show and program by RC the current level flight PID values
                            draw_program_pid_values(1, l_pid_vars[RKp], "Roll l_Kp: %3.5f", indexer, 1);
                            draw_program_pid_values(2, l_pid_vars[RKi], "Roll l_Ki: %3.5f", indexer, 1);
                            draw_program_pid_values(3, l_pid_vars[RKd], "Roll l_Kd: %3.5f", indexer, 1);
                            draw_program_pid_values(4, l_pid_vars[NKp], "Nick l_Kp: %3.5f", indexer, 1);
                            draw_program_pid_values(5, l_pid_vars[NKi], "Nick l_Ki: %3.5f", indexer, 1);
                            draw_program_pid_values(6, l_pid_vars[NKd], "Nick l_Kd: %3.5f", indexer, 1);
                            draw_program_pid_values(7, l_pid_vars[GKp], "Gier l_Kp: %3.5f", indexer, 1);
                            draw_program_pid_values(8, l_pid_vars[GKi], "Gier l_Ki: %3.5f", indexer, 1);
                            draw_program_pid_values(9, l_pid_vars[GKd], "Gier l_Kd: %3.5f", indexer, 1);
                        }
                    }
#endif
                }
                else // armed, flight mode screen
                {
                    // transition to armed
                    if (armed - back_armed > 0)
                    {
                        if (channels[rc_mode] < L_TRSH)
                        {
                            led_set_armed_acro(255);
                        }
                        else
                        {
                            led_set_armed_level_hold(255);
                        }
#ifdef HAVE_DISPLAY
                        BSP_LCD_SetRotation(3);
                        BSP_LCD_Clear(LCD_COLOR_GREEN);
#endif
                        back_armed = armed;
                    }
                }
            }

            if (counter == 7) // Batt Voltage Slot
            {
                HAL_ADCEx_Calibration_Start(&hadc1);
                if (HAL_ADC_Start(&hadc1) == HAL_OK)
                {
                    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
                    {
                        volt1 = (HAL_ADC_GetValue(&hadc1)) / 219.84f; // calibrate, resistors 8.2k 1.8k
                    }
                    HAL_ADC_Stop(&hadc1);
                }

                if (volt1 < low_voltage)
                {
                    low_volt = 1;
                }
                else if (volt1 > (low_voltage + 1.0f))
                {
                    low_volt = 0;
                }
            }

            if (counter == 6) // Board LED Slot
            {
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);    // maple mini clone board
                //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // minimum development system board
            }

            if (counter == 5) // LEDs Show State Slot
            {
                // rotate led colors unless armed light
                if (armed == 0)
                {
                    // rotate colors, right is moving inner to outer
                    led_rotate_right(0, NR_COLORS - 1);
                    //led_rotate_left(0, NR_COLORS - 1);

                    if (warning == 0) // to not disturb warning LED off period
                    {
                        led_trans_vals();
                    }
                }
                else
                {
                    // transition to level hold mode while armed
                    if (channels[rc_mode] - back_channels[rc_mode] > TRANS_OFFS)
                    {
                        led_set_armed_level_hold(255);
                        back_channels[rc_mode] = channels[rc_mode];
                    }
                    // transition to acro mode while armed, only low position of 3 step switch is acro
                    if (back_channels[rc_mode] - channels[rc_mode] > TRANS_OFFS && channels[rc_mode] < L_TRSH)
                    {
                        led_set_armed_acro(255);
                        back_channels[rc_mode] = channels[rc_mode];
                    }
                }
            }

            if (counter == 4)  // Warning Slot
            {

                if (led_blink_counter++ > 10)
                {
                    led_blink_counter = 0;
                }

                if ((low_volt == 1 || channels[rc_beep] > L_TRSH) && hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
                //if ((low_volt == 1 || channels[rc_beep] > L_TRSH)) // for testing armed while USB connected
                {
                    // beep
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
                    warning = 1;
                }

                if (warning == 1)
                {
                    if (led_blink_counter <= 5)
                    {
                        // blink OFF
                        led_set_off(0, NR_LEDS);
                    }
                    else
                    {
                        // blink ON
                        led_trans_vals();

                        // reset warning state after blink period finished if warning condition vanished
                        if ((low_volt == 0 && channels[rc_beep] < H_TRSH) || hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED)
                        {
                            // unbeep
                            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
                            warning = 0;
                        }
                    }
                }
            }

            if (counter == 3)
            {
                // Baro
            }

            if (counter == 2)
            {
                // GPS
            }

            if (counter == 1) // cal acc while flying acro slot
            {
                if ( armed && channels[rc_thrust] > L_TRSH && channels[rc_mode] < L_TRSH) // device is flying, acro mode
                {
                    // collect errors if counter is running (1.6 s)
                    if ( acc_cal_counter > 0 )
                    {
                        acc_cal_counter--;

                        if ( BSP_Acc_Collect_Error_cal(p_settings->acc_offset, acc_cal_counter) == 0 )
                        {
                            // 57 ms no control
                            erase_flash_page();
                            write_flash_vars((uint32_t*) flash_buf, 256, 0);
                        }
                    }
                    else
                    {
                        // detect transition to beep if counter not running
                        // prepare ACC error collecting
                        if (channels[rc_beep] - back_channels[rc_beep] > TRANS_OFFS)
                        {
                            acc_cal_counter = ACC_ERR_COLLECT_COUNT;
                            BSP_Revert_Acc_Cancel();
                            back_channels[rc_beep] = 2000; // restore default
                        }
                    }
                }
            }

            // For visualization by Processing Software
            //sprintf(buf2, "%3.3f,%3.3f,%3.3f\n", ang[gier], ang[nick], ang[roll]);

            //sprintf(buf2, "%ld\n", idle_counter);
            //CDC_Transmit_FS((uint8_t*) buf2, strlen(buf2));

            // max 541 us without screen updates when armed, plenty of time for idle_counter
            //millis[1] = HAL_GetTick();
            //micros[1] = SysTick->VAL;

            /*
             if ( micros[0] > micros[1] )
             {
             sprintf(buf2, "diff: %ld\n", ( micros[0] - micros[1] ) / 72 + 1000 * ( millis[1] - millis[0] ) );
             //sprintf(buf2, "millis0: %ld micros0: %ld millis1: %ld micros1: %ld\n", millis[0], micros[0]/72, millis[1], micros[1]/72 );
             }
             else // systick counter rounded between probes
             {
             sprintf(buf2, "diff_r: %ld\n", micros[0] / 72 + 1000 - micros[1] / 72 + 1000 * ( millis[1] - millis[0] -1 )  );
             //sprintf(buf2, "millis0: %ld micros0: %ld millis1: %ld micros1: %ld\n", millis[0], micros[0]/72, millis[1], micros[1]/72 );
             }

             CDC_Transmit_FS((uint8_t*) buf2, strlen(buf2));

             // read on PC:
             // stty "1:0:18b2:0:3:1c:7f:15:4:5:1:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0" -F /dev/ttyACM0
             // cat /dev/ttyACM0
             */

            idle_counter = 0;

        }
        else
        {
            idle_counter++;
        }

    } //while(1)

  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC
                              |RCC_PERIPHCLK_USB;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
    /* User can add his own implementation to report the HAL error return state */
    uint8_t i;
    for (i = 0; i < 6; i++)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(200);
    }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
