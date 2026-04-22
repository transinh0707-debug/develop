################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/board/ra2t1_fpb/board_init.c \
../ra/board/ra2t1_fpb/board_leds.c 

C_DEPS += \
./ra/board/ra2t1_fpb/board_init.d \
./ra/board/ra2t1_fpb/board_leds.d 

OBJS += \
./ra/board/ra2t1_fpb/board_init.o \
./ra/board/ra2t1_fpb/board_leds.o 

SREC += \
compl_pwm.srec 

MAP += \
compl_pwm.map 


# Each subdirectory must supply rules for building sources it contributes
ra/board/ra2t1_fpb/%.o: ../ra/board/ra2t1_fpb/%.c
	$(file > $@.in,-mcpu=cortex-m23 -mthumb -Oz -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM23 -D_RA_ORDINAL=1 -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/ra_gen" -I"." -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/ra_cfg/fsp_cfg/bsp" -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/ra_cfg/fsp_cfg" -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/src" -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/ra/fsp/inc" -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/ra/fsp/inc/api" -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/ra/fsp/inc/instances" -I"C:/Users/admin.wins/e2_studio/workspace/User/5725/develop-master/compl_pwm/ra/arm/CMSIS_6/CMSIS/Core/Include" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

