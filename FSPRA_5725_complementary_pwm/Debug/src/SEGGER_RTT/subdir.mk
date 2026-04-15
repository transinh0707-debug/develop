################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/SEGGER_RTT/SEGGER_RTT.c \
../src/SEGGER_RTT/SEGGER_RTT_printf.c 

C_DEPS += \
./src/SEGGER_RTT/SEGGER_RTT.d \
./src/SEGGER_RTT/SEGGER_RTT_printf.d 

OBJS += \
./src/SEGGER_RTT/SEGGER_RTT.o \
./src/SEGGER_RTT/SEGGER_RTT_printf.o 

SREC += \
FSPRA_5725_complementary_pwm.srec 

MAP += \
FSPRA_5725_complementary_pwm.map 


# Each subdirectory must supply rules for building sources it contributes
src/SEGGER_RTT/%.o: ../src/SEGGER_RTT/%.c
	$(file > $@.in,-mcpu=cortex-m23 -mthumb -Oz -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM23 -D_RA_ORDINAL=1 -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/ra_gen" -I"." -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/ra_cfg/fsp_cfg/bsp" -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/ra_cfg/fsp_cfg" -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/src" -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/ra/fsp/inc" -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/ra/fsp/inc/api" -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/ra/fsp/inc/instances" -I"C:/Users/admin.wins/e2_studio/workspace/FSPRA_5725_complementary_pwm/ra/arm/CMSIS_6/CMSIS/Core/Include" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

