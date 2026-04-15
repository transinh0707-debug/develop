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
gpt_fpb_ra2t1_ep.srec 

MAP += \
gpt_fpb_ra2t1_ep.map 


# Each subdirectory must supply rules for building sources it contributes
src/SEGGER_RTT/%.o: ../src/SEGGER_RTT/%.c
	$(file > $@.in,-mcpu=cortex-m23 -mthumb -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM23 -D_RA_ORDINAL=1 -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/src" -I"." -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/fsp/inc" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/fsp/inc/api" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/fsp/inc/instances" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/arm/CMSIS_6/CMSIS/Core/Include" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra_gen" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra_cfg/fsp_cfg/bsp" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra_cfg/fsp_cfg" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

