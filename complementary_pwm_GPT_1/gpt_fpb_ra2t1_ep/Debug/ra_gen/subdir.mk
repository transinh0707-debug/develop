################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra_gen/common_data.c \
../ra_gen/hal_data.c \
../ra_gen/main.c \
../ra_gen/pin_data.c \
../ra_gen/vector_data.c 

C_DEPS += \
./ra_gen/common_data.d \
./ra_gen/hal_data.d \
./ra_gen/main.d \
./ra_gen/pin_data.d \
./ra_gen/vector_data.d 

OBJS += \
./ra_gen/common_data.o \
./ra_gen/hal_data.o \
./ra_gen/main.o \
./ra_gen/pin_data.o \
./ra_gen/vector_data.o 

SREC += \
gpt_fpb_ra2t1_ep.srec 

MAP += \
gpt_fpb_ra2t1_ep.map 


# Each subdirectory must supply rules for building sources it contributes
ra_gen/%.o: ../ra_gen/%.c
	$(file > $@.in,-mcpu=cortex-m23 -mthumb -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM23 -D_RA_ORDINAL=1 -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/src" -I"." -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/fsp/inc" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/fsp/inc/api" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/fsp/inc/instances" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra/arm/CMSIS_6/CMSIS/Core/Include" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra_gen" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra_cfg/fsp_cfg/bsp" -I"C:/Users/tminhnguyen/Downloads/fspra-5725/fspra-5725/r_gpt_Support_for_Complementary_PWM/develop-master/e2studio/ra_cfg/fsp_cfg" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

