################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
D:/DZF/speech_recognition/STM32CubeU5/Middlewares/ST/threadx/utility/low_power/tx_low_power.c 

OBJS += \
./Middlewares/ThreadX/Low_Power_Support/tx_low_power.o 

C_DEPS += \
./Middlewares/ThreadX/Low_Power_Support/tx_low_power.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/ThreadX/Low_Power_Support/tx_low_power.o: D:/DZF/speech_recognition/STM32CubeU5/Middlewares/ST/threadx/utility/low_power/tx_low_power.c Middlewares/ThreadX/Low_Power_Support/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DTX_INCLUDE_USER_DEFINE_FILE -DTX_SINGLE_MODE_NON_SECURE=1 -DUSE_HAL_DRIVER -DSTM32U575xx -c -I../../Inc -I../../../../../../../Drivers/STM32U5xx_HAL_Driver/Inc -I../../../../../../../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../../../../../../../Middlewares/ST/threadx/common/inc -I../../../../../../../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../../../../../../../Middlewares/ST/threadx/ports/cortex_m33/gnu/inc -I../../../../../../../Middlewares/ST/threadx/utility/low_power -I../../../../../../../Drivers/CMSIS/Include -I../../../../../../../Drivers/CMSIS/DSP/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-ThreadX-2f-Low_Power_Support

clean-Middlewares-2f-ThreadX-2f-Low_Power_Support:
	-$(RM) ./Middlewares/ThreadX/Low_Power_Support/tx_low_power.cyclo ./Middlewares/ThreadX/Low_Power_Support/tx_low_power.d ./Middlewares/ThreadX/Low_Power_Support/tx_low_power.o ./Middlewares/ThreadX/Low_Power_Support/tx_low_power.su

.PHONY: clean-Middlewares-2f-ThreadX-2f-Low_Power_Support

