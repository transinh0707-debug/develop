################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/complementary_pwm.c \
../src/gpt_timer.c \
../src/hal_entry.c 

C_DEPS += \
./src/complementary_pwm.d \
./src/gpt_timer.d \
./src/hal_entry.d 

OBJS += \
./src/complementary_pwm.o \
./src/gpt_timer.o \
./src/hal_entry.o 

SREC += \
gpt_fpb_ra2t1_ep.srec 

MAP += \
gpt_fpb_ra2t1_ep.map 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	$(file > $@.in,-mcpu=cortex-m23 -mthumb -Oz -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM23 -D_RA_ORDINAL=1 -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/src" -I"." -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/ra/fsp/inc" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/ra/fsp/inc/api" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/ra/fsp/inc/instances" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/ra/arm/CMSIS_6/CMSIS/Core/Include" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/ra_gen" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/ra_cfg/fsp_cfg/bsp" -I"C:/Users/Admin/Desktop/gpt_fpb_ra2t1_ep/complementary_pwm_GPT_2/gpt_fpb_ra2t1_ep/ra_cfg/fsp_cfg" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

