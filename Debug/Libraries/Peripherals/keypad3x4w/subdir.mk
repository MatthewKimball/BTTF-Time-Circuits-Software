################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libraries/Peripherals/keypad3x4w/keypad3x4w.c 

OBJS += \
./Libraries/Peripherals/keypad3x4w/keypad3x4w.o 

C_DEPS += \
./Libraries/Peripherals/keypad3x4w/keypad3x4w.d 


# Each subdirectory must supply rules for building sources it contributes
Libraries/Peripherals/keypad3x4w/%.o Libraries/Peripherals/keypad3x4w/%.su Libraries/Peripherals/keypad3x4w/%.cyclo: ../Libraries/Peripherals/keypad3x4w/%.c Libraries/Peripherals/keypad3x4w/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F405xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Peripherals/ht16k33" -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Peripherals/keypad3x4w" -I"C:/Users/User/STM32CubeIDE/workspace_1.7.0/Time Circuit Control/Libraries/Time_Circuits" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Libraries-2f-Peripherals-2f-keypad3x4w

clean-Libraries-2f-Peripherals-2f-keypad3x4w:
	-$(RM) ./Libraries/Peripherals/keypad3x4w/keypad3x4w.cyclo ./Libraries/Peripherals/keypad3x4w/keypad3x4w.d ./Libraries/Peripherals/keypad3x4w/keypad3x4w.o ./Libraries/Peripherals/keypad3x4w/keypad3x4w.su

.PHONY: clean-Libraries-2f-Peripherals-2f-keypad3x4w

