################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../SerialDaemon.c \
../SerialLib8051.c \
../SerialMsgUtils.c \
../alt_functions.c \
../become_daemon.c \
../error_functions.c \
../get_num.c \
../signal.c \
../tty_functions.c 

OBJS += \
./SerialDaemon.o \
./SerialLib8051.o \
./SerialMsgUtils.o \
./alt_functions.o \
./become_daemon.o \
./error_functions.o \
./get_num.o \
./signal.o \
./tty_functions.o 

C_DEPS += \
./SerialDaemon.d \
./SerialLib8051.d \
./SerialMsgUtils.d \
./alt_functions.d \
./become_daemon.d \
./error_functions.d \
./get_num.d \
./signal.d \
./tty_functions.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	arm-linux-gnueabihf-gcc -I/usr/arm-linux-gnueabihf/include -I"/home/mbezold/workspace/SerialLib8051" -include"/home/mbezold/workspace/SerialDaemon8051/SerialPacket.h" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


