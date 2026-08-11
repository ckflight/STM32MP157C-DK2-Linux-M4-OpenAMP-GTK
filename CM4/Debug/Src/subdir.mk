################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/main.c \
../Src/mbox_ipcc.c \
../Src/openamp.c \
../Src/openamp_log.c \
../Src/rsc_table.c \
../Src/stm32mp1xx_hal_msp.c \
../Src/stm32mp1xx_it.c \
../Src/syscalls.c \
../Src/sysmem.c 

OBJS += \
./Src/main.o \
./Src/mbox_ipcc.o \
./Src/openamp.o \
./Src/openamp_log.o \
./Src/rsc_table.o \
./Src/stm32mp1xx_hal_msp.o \
./Src/stm32mp1xx_it.o \
./Src/syscalls.o \
./Src/sysmem.o 

C_DEPS += \
./Src/main.d \
./Src/mbox_ipcc.d \
./Src/openamp.d \
./Src/openamp_log.d \
./Src/rsc_table.d \
./Src/stm32mp1xx_hal_msp.d \
./Src/stm32mp1xx_it.d \
./Src/syscalls.d \
./Src/sysmem.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DNO_ATOMIC_64_SUPPORT -DMETAL_INTERNAL -DMETAL_MAX_DEVICE_REGIONS=2 -DVIRTIO_SLAVE_ONLY -DUSE_HAL_DRIVER -DSTM32MP157Cxx -c -I../Inc -I../../Middlewares/Third_Party/OpenAMP/open-amp/lib/include -I../../Middlewares/Third_Party/OpenAMP/libmetal/lib/include -I../../Drivers/STM32MP1xx_HAL_Driver/Inc -I../../Drivers/STM32MP1xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32MP1xx/Include -I../../Middlewares/Third_Party/OpenAMP/virtual_driver -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/mbox_ipcc.cyclo ./Src/mbox_ipcc.d ./Src/mbox_ipcc.o ./Src/mbox_ipcc.su ./Src/openamp.cyclo ./Src/openamp.d ./Src/openamp.o ./Src/openamp.su ./Src/openamp_log.cyclo ./Src/openamp_log.d ./Src/openamp_log.o ./Src/openamp_log.su ./Src/rsc_table.cyclo ./Src/rsc_table.d ./Src/rsc_table.o ./Src/rsc_table.su ./Src/stm32mp1xx_hal_msp.cyclo ./Src/stm32mp1xx_hal_msp.d ./Src/stm32mp1xx_hal_msp.o ./Src/stm32mp1xx_hal_msp.su ./Src/stm32mp1xx_it.cyclo ./Src/stm32mp1xx_it.d ./Src/stm32mp1xx_it.o ./Src/stm32mp1xx_it.su ./Src/syscalls.cyclo ./Src/syscalls.d ./Src/syscalls.o ./Src/syscalls.su ./Src/sysmem.cyclo ./Src/sysmem.d ./Src/sysmem.o ./Src/sysmem.su

.PHONY: clean-Src

