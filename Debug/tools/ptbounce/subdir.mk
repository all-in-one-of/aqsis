################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../tools/ptbounce/ptbounce.cpp 

OBJS += \
./tools/ptbounce/ptbounce.o 

CPP_DEPS += \
./tools/ptbounce/ptbounce.d 


# Each subdirectory must supply rules for building sources it contributes
tools/ptbounce/%.o: ../tools/ptbounce/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


