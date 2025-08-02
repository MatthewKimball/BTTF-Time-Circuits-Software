################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libraries/Time_Circuits/datetime_display.c \
../Libraries/Time_Circuits/imu.c \
../Libraries/Time_Circuits/sound_effects.c \
../Libraries/Time_Circuits/storagedevice_control.c \
../Libraries/Time_Circuits/timecircuit_control.c 

OBJS += \
./Libraries/Time_Circuits/datetime_display.o \
./Libraries/Time_Circuits/imu.o \
./Libraries/Time_Circuits/sound_effects.o \
./Libraries/Time_Circuits/storagedevice_control.o \
./Libraries/Time_Circuits/timecircuit_control.o 

C_DEPS += \
./Libraries/Time_Circuits/datetime_display.d \
./Libraries/Time_Circuits/imu.d \
./Libraries/Time_Circuits/sound_effects.d \
./Libraries/Time_Circuits/storagedevice_control.d \
./Libraries/Time_Circuits/timecircuit_control.d 


# Each subdirectory must supply rules for building sources it contributes
Libraries/Time_Circuits/%.o Libraries/Time_Circuits/%.su Libraries/Time_Circuits/%.cyclo: ../Libraries/Time_Circuits/%.c Libraries/Time_Circuits/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F405xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Peripherals/ht16k33" -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Peripherals/keypad3x4w" -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Time_Circuits" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Peripherals/bno055" -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Peripherals/ds3231" -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/CanOpenSTM32/CANopenNode" -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/CanOpenSTM32/CANopenNode_STM32" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Libraries-2f-Time_Circuits

clean-Libraries-2f-Time_Circuits:
	-$(RM) ./Libraries/Time_Circuits/datetime_display.cyclo ./Libraries/Time_Circuits/datetime_display.d ./Libraries/Time_Circuits/datetime_display.o ./Libraries/Time_Circuits/datetime_display.su ./Libraries/Time_Circuits/imu.cyclo ./Libraries/Time_Circuits/imu.d ./Libraries/Time_Circuits/imu.o ./Libraries/Time_Circuits/imu.su ./Libraries/Time_Circuits/sound_effects.cyclo ./Libraries/Time_Circuits/sound_effects.d ./Libraries/Time_Circuits/sound_effects.o ./Libraries/Time_Circuits/sound_effects.su ./Libraries/Time_Circuits/storagedevice_control.cyclo ./Libraries/Time_Circuits/storagedevice_control.d ./Libraries/Time_Circuits/storagedevice_control.o ./Libraries/Time_Circuits/storagedevice_control.su ./Libraries/Time_Circuits/timecircuit_control.cyclo ./Libraries/Time_Circuits/timecircuit_control.d ./Libraries/Time_Circuits/timecircuit_control.o ./Libraries/Time_Circuits/timecircuit_control.su

.PHONY: clean-Libraries-2f-Time_Circuits

