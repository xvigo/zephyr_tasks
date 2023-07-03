/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 32

/* error enum */
enum {
	ERR_NONE = 0,
	ERR_INVALID_EXPR = -1,
	ERR_ZERO_DIV = -2,
};

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards, parse and
 * evaluate the mathematical expression and send the result back to the user.
 *
 * @param dev        UART device
 * @param user_data  unused
 *
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {

		if ((c == '\n' || c == '\r')) {
			/* ignore line feed */
			if (rx_buf_pos == 0) {
				continue;
			}

			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';
			uart_poll_out(uart_dev, '\r');
			uart_poll_out(uart_dev, '\n');

			k_msgq_put(&uart_msgq, rx_buf, K_NO_WAIT);
			rx_buf_pos = 0;

		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
			uart_poll_out(uart_dev, c);
		}
	}
}

/*
 * Print a null-terminated string character by character to the UART interface.
 * If endl is set to 1, a newline character is added at the end.
 *
 * @param buf   pointer to the string to be printed
 * @param endl  1 to add a newline character at the end, 0 otherwise
 */
void print_uart(char *buf, char endl)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}

	if (endl) {
		uart_poll_out(uart_dev, '\r');
		uart_poll_out(uart_dev, '\n');
	}
}

/*
 * Perform the specified arithmetic operation
 *
 * @param operand1  first operand
 * @param operand2  second operand
 * @param operator  arithmetic operator
 * @param result    pointer to the result variable
 *
 * @return          0 on success, negative value on error
 */
int perform_operation(int operand1, int operand2, char operator, int * result)
{
	switch (operator) {
	case '+':
		*result = operand1 + operand2;
		break;
	case '-':
		*result = operand1 - operand2;
		break;
	case '*':
		*result = operand1 * operand2;
		break;
	case '/':
		if (operand2 != 0) {
			*result = operand1 / operand2;
		} else {
			return ERR_ZERO_DIV;
		}
		break;
	case '%':
		if (operand2 != 0) {
			*result = operand1 % operand2;
		} else {
			return ERR_ZERO_DIV;
		}
		break;
	default:
		return ERR_INVALID_EXPR; // invalid operator, return the first operand
	}

	return ERR_NONE;
}

/*
 * Evaluate a mathematical expression and return the result
 *
 * @param expr      string containing the expression
 * @param result    pointer to the result variable
 *
 * @return          0 on success, negative value on error
 */
int eval_expression(char *expr, int *result)
{
	int operand1, operand2;
	char operator= '\0';
	char *endptr;

	/* parse first operand */
	operand1 = strtol(expr, &endptr, 10);
	if (strlen(endptr) == strlen(expr)) {
		return ERR_INVALID_EXPR; // invalid character
	}
	expr = endptr;

	/*parse operator*/
	while (isspace(*expr)) {
		expr++;
	}
	operator= * expr;
	expr++;

	/* parse second operand */
	operand2 = strtol(expr, &endptr, 10);
	if (strlen(endptr) == strlen(expr)) {
		return ERR_INVALID_EXPR; // invalid character
	}
	expr = endptr;

	/* check if there are any trailing characters */
	while (isspace(*expr)) {
		expr++;
	}
	if (*expr != '\0') {
		return ERR_INVALID_EXPR;
	}

	/* perform the operation */
	return perform_operation(operand1, operand2, operator, result);
}

int main(void)
{
	char tx_buf[MSG_SIZE];

	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return ERR_NONE;
	}

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return ERR_NONE;
	}
	uart_irq_rx_enable(uart_dev);

	print_uart("Simple UART Calculator", 1);
	print_uart("Enter a mathematical expression with 2 operands (e.g., 2 + 3):", 1);

	/* infinitely wait for input from the user */
	while (k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER) == ERR_NONE) {
		int result = 0;

		if (eval_expression(tx_buf, &result) == ERR_NONE) {
			sprintf(tx_buf, "Result: %d", result);
			print_uart(tx_buf, 1);

		} else if (eval_expression(tx_buf, &result) == ERR_ZERO_DIV) {
			print_uart("Division by zero!", 1);

		} else {
			print_uart("Invalid expression!", 1);
		}
	}
	return ERR_NONE;
}
