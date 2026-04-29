################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hal_entry.c \
../src/hal_warmstart.c \
../src/r_gpt_test_tg3_comp_pwm.c 

C_DEPS += \
./src/hal_entry.d \
./src/hal_warmstart.d \
./src/r_gpt_test_tg3_comp_pwm.d 

OBJS += \
./src/hal_entry.o \
./src/hal_warmstart.o \
./src/r_gpt_test_tg3_comp_pwm.o 

SREC += \
compl_pwm.srec 

MAP += \
compl_pwm.map 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	$(file > $@.in,-mcpu=cortex-m23 -mthumb -Oz -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM23 -D_RA_ORDINAL=1 -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/ra_gen" -I"." -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/ra_cfg/fsp_cfg/bsp" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/ra_cfg/fsp_cfg" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/src" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/ra/fsp/inc" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/ra/fsp/inc/api" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/ra/fsp/inc/instances" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/develop/compl_pwm_support_RTT_Viewer/ra/arm/CMSIS_6/CMSIS/Core/Include" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

