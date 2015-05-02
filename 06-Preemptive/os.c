#include <stddef.h>
#include <stdint.h>
#include "reg.h"
#include "asm.h"
#include "host.h"

/* Size of our user task stacks in words */
#define STACK_SIZE	256

/* Number of user task */
#define TASK_LIMIT	10 // task_count limit

/* USART TXE Flag
 * This flag is cleared when data is written to USARTx_DR and
 * set when that data is transferred to the TDR
 */
#define USART_FLAG_TXE	((uint16_t) 0x0080)

void usart_init(void)
{
	*(RCC_APB2ENR) |= (uint32_t) (0x00000001 | 0x00000004);
	*(RCC_APB1ENR) |= (uint32_t) (0x00020000);

	/* USART2 Configuration, Rx->PA3, Tx->PA2 */
	*(GPIOA_CRL) = 0x00004B00;
	*(GPIOA_CRH) = 0x44444444;
	*(GPIOA_ODR) = 0x00000000;
	*(GPIOA_BSRR) = 0x00000000;
	*(GPIOA_BRR) = 0x00000000;

	*(USART2_CR1) = 0x0000000C;
	*(USART2_CR2) = 0x00000000;
	*(USART2_CR3) = 0x00000000;
	*(USART2_CR1) |= 0x2000;
}

void print_str(const char *str)
{
	while (*str) {
		while (!(*(USART2_SR) & USART_FLAG_TXE));
		*(USART2_DR) = (*str & 0xFF);
		str++;
	}
}

void delay(int count)
{
	count *= 50000;
	while (count--);
}

/* Exception return behavior */
#define HANDLER_MSP	0xFFFFFFF1
#define THREAD_MSP	0xFFFFFFF9
#define THREAD_PSP	0xFFFFFFFD

/* Initilize user task stack and execute it one time */
/* XXX: Implementation of task creation is a little bit tricky. In fact,
 * after the second time we called `activate()` which is returning from
 * exception. But the first time we called `activate()` which is not returning
 * from exception. Thus, we have to set different `lr` value.
 * First time, we should set function address to `lr` directly. And after the
 * second time, we should set `THREAD_PSP` to `lr` so that exception return
 * works correctly.
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0552a/Babefdjc.html
 */
unsigned int *create_task(unsigned int *stack, void (*start)(void), int priority)
{
	static int first = 1;

	stack += STACK_SIZE - 34; /* End of stack, minus what we are about to push */
	if (first) {
		stack[8] = (unsigned int) start;
		stack[17] = priority;
		first = 0;
	} else {
		stack[8] = (unsigned int) THREAD_PSP;
		stack[15] = (unsigned int) start;
		stack[16] = (unsigned int) 0x01000000; /* PSR Thumb bit */
		stack[17] = priority;
	}
	stack = activate(stack);

	return stack;
}

void task1_func(void)
{
	print_str("task1: Created!\n");
	print_str("task1: Now, return to kernel mode\n");
	syscall();
	while (1) {
		print_str("task1: Running...\n");
		delay(1000);
	}
}

void task2_func(void)
{
	print_str("task2: Created!\n");
	print_str("task2: Now, return to kernel mode\n");
	syscall();
	while (1) {
		print_str("task2: Running...\n");
		delay(1000);
	}
}

void task3_func(void)
{
	print_str("task3: Created!\n");
	print_str("task3: Now, return to kernel mode\n");
	syscall();
	while (1) {
		print_str("task3: Running...\n");
		delay(1000);
	}
}
int main(void)
{
	unsigned int user_stacks[TASK_LIMIT][STACK_SIZE];
	unsigned int *usertasks[TASK_LIMIT];
	size_t task_count = 0;
	size_t current_task;

	usart_init();

	print_str("OS: Starting...\n");
	print_str("OS: First create task 1 with priority = 2\n");
	usertasks[0] = create_task(user_stacks[0], &task1_func, 2);
	task_count += 1;
	print_str("OS: Back to OS, create task 2 with priority = 1\n");
	usertasks[1] = create_task(user_stacks[1], &task2_func, 1);
	task_count += 1;
	print_str("OS: New task, create task 3 with priority = 3\n");
	usertasks[2] = create_task(user_stacks[2], &task3_func, 3);
	task_count += 1;

	//print_str("\nOS: Start round-robin scheduler!\n");
	print_str("\nOS: Start priority-based scheduler!\n");

	/* SysTick configuration */
	*SYSTICK_LOAD = 7200000;
	*SYSTICK_VAL = 0;
	*SYSTICK_CTRL = 0x07;
	current_task = 0;

	size_t task, highest = 0, i;
	while (1) {
		for(i = 0; i < task_count; i++){
			task = *(usertasks[i] + 19);
			if(task > highest){ // if equal we ignore, first serve
				print_str("task > highest\n");
				highest =  task;
				current_task = i;
			}
		}
		print_str("OS: Activate the next high priority task\n");
		usertasks[current_task] = activate(usertasks[current_task]);
		
	}

	return 0;
}
