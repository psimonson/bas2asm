#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

typedef enum {
	TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENT, TOKEN_PRINT, TOKEN_LET,
	TOKEN_GOTO, TOKEN_IF, TOKEN_COLON, TOKEN_EXIT, TOKEN_EQ, TOKEN_LT,
	TOKEN_GT, TOKEN_ADD, TOKEN_SUB, TOKEN_MUL, TOKEN_DIV, TOKEN_LP,
	TOKEN_RP, TOKEN_LINENO, TOKEN_COMMA, TOKEN_INPUT, TOKEN_EOL,
	TOKEN_DIM, TOKEN_EOF
} TokenType;

typedef struct {
	TokenType type;
	int value;
	char name[64];
	char str_val[64];
} Token;

typedef struct {
	int lineno;
	char value[64];
} PrintString;

typedef struct {
	TokenType type;
	unsigned char flags;
	int lineno;
	char name[64];
	union {
		int value;
		char string[256];
	};
} Variable;

const char *input;
Token current_token;
int current_value = 0;
int new_value = 0;
int output = 1;
Variable variables[1024];
int variable_counter = 0;
PrintString print_strings[1024];
int print_strings_counter = 0;
int if_statement = 0;
int label_counter = 0;
int lineno_start = 1;
int lineno = 0;

// Forward declarations
void free_variables();
void free_strings();

void parse_program();
void parse_statement();
void parse_expression();
void parse_term();
void parse_factor();
void next_token();
void error(const char *msg);
void emit(const char *fmt, ...);

// Tokenizer
void next_token() {
	while (*input == ' ' || *input == '\t') input++;

	if (*input == '\0') { current_token.type = TOKEN_EOF; return; }
	if (*input == '\n') { current_token.type = TOKEN_EOL; lineno_start = 1; input++; return; }

	if (!lineno_start && (isdigit(*input) || (*input == '-' && isdigit(*(input+1))))) {
		current_token.type = TOKEN_NUMBER;
		current_token.value = strtol(input, (char**)&input, 10);
		return;
	}

	if (lineno_start && isdigit(*input)) {
		current_token.type = TOKEN_LINENO;
		lineno = current_token.value = strtol(input, (char**)&input, 10);
		if (output) emit("label_%d:\n", current_token.value);
		lineno_start = 0;
		return;
	}

	if (*input == '\"') {
		input++;
		int i = 0;
		while (i < 63 && *input != '\"' && *input != '\n' && *input != '\0') {
			current_token.str_val[i++] = *input++;
		}
		current_token.str_val[i] = '\0';
		if (*input == '\"') input++;
		current_token.type = TOKEN_STRING;
		return;
	}

	if (isalpha(*input)) {
		if (strncasecmp(input, "PRINT", 5) == 0) { current_token.type = TOKEN_PRINT; input += 5; return; }
		if (strncasecmp(input, "INPUT", 5) == 0) { current_token.type = TOKEN_INPUT; input += 5; return; }
		if (strncasecmp(input, "DIM", 3) == 0) { current_token.type = TOKEN_DIM; input += 3; return; }
		if (strncasecmp(input, "LET", 3) == 0) { current_token.type = TOKEN_LET; input += 3; return; }
		if (strncasecmp(input, "IF", 2) == 0) { current_token.type = TOKEN_IF; input += 2; return; }
		if (strncasecmp(input, "GOTO", 4) == 0) { current_token.type = TOKEN_GOTO; input += 4; return; }
		if (strncasecmp(input, "EXIT", 4) == 0) { current_token.type = TOKEN_EXIT; input += 4; return; }
		if (isalpha(*input)) {
			int i = 0;
			while (i < 63 && isalpha(*input)) {
				current_token.name[i++] = *input++;
			}
			current_token.name[i] = '\0';
			current_token.type = TOKEN_IDENT;
			return;
		}
	}

	switch (*input) {
		case ',': current_token.type = TOKEN_COMMA; input++; break;
		case ':': current_token.type = TOKEN_COLON; input++; break;
		case '<': current_token.type = TOKEN_LT; input++; break;
		case '>': current_token.type = TOKEN_GT; input++; break;
		case '=': current_token.type = TOKEN_EQ; input++; break;
		case '+': current_token.type = TOKEN_ADD; input++; break;
		case '-': current_token.type = TOKEN_SUB; input++; break;
		case '*': current_token.type = TOKEN_MUL; input++; break;
		case '/': current_token.type = TOKEN_DIV; input++; break;
		case '(': current_token.type = TOKEN_LP; input++; break;
		case ')': current_token.type = TOKEN_RP; input++; break;
		default: error("Unexpected character"); return;
	}
}

// Error handling
void error(const char *msg) {
	fprintf(stderr, "Error: %s\n", msg);
	free_variables();
	free_strings();
	exit(1);
}

// Code generation helper
void emit(const char *fmt, ...) {
	if (!fmt) {
		fprintf(stderr, "Format string was NULL.\n");
		free_variables();
		free_strings();
		exit(1);
	}
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

// Find variable by name
Variable *find_variable(const char *name) {
	for (int i = 0; i < variable_counter; i++) {
		if (!strcasecmp(variables[i].name, name)) {
			return &variables[i];
		}
	}
	return NULL;
}

// Add variable number
void add_variable_number(int lineno, const char *name, int value, unsigned char flags) {
	if (!name) return;
	Variable *var = find_variable(name);
	if (!var) {
		if (variable_counter < 1024) {
			variables[variable_counter].lineno = lineno;
			variables[variable_counter].flags = flags;
			variables[variable_counter].type = TOKEN_NUMBER;
			strncpy(variables[variable_counter].name, name, 63);
			variables[variable_counter].name[63] = '\0';
			variables[variable_counter].value = value;
			variable_counter++;
		}
	}
}

// Add variable string
void add_variable_string(int lineno, const char *name, const char *value, unsigned char flags) {
	if (!name || !value) return;
	Variable *var = find_variable(name);
	if (!var) {
		if (variable_counter < 1024) {
			variables[variable_counter].lineno = lineno;
			variables[variable_counter].flags = flags;
			variables[variable_counter].type = TOKEN_STRING;
			strncpy(variables[variable_counter].name, name, 63);
			variables[variable_counter].name[63] = '\0';
			strncpy(variables[variable_counter].string, value, 255);
			variables[variable_counter].string[255] = '\0';
			variable_counter++;
		}
	}
}

// Free variables
void free_variables() {
	while (variable_counter >= 0) {
		variable_counter--;
		switch (variables[variable_counter].type) {
			case TOKEN_NUMBER:
				variables[variable_counter].lineno = 0;
				memset(variables[variable_counter].name, 0, sizeof(variables[variable_counter].name)-1);
				variables[variable_counter].value = 0;
				break;
			case TOKEN_STRING:
				variables[variable_counter].lineno = 0;
				memset(variables[variable_counter].name, 0, sizeof(variables[variable_counter].name)-1);
				memset(variables[variable_counter].string, 0, sizeof(variables[variable_counter].string)-1);
				break;
			default:
				break;
		}
		variables[variable_counter].type = 0;
	}
}

// Find string
PrintString *find_string(int lineno, const char *value) {
	for (int i = 0; i < print_strings_counter; i++) {
		PrintString *string = &print_strings[i];
		if (string) {
			if (lineno == string->lineno && !strcmp(string->value, value)) {
				return string;
			}
		}
	}
	return NULL;
}

// Add string to string variable
void add_string(int lineno, const char *string) {
	if (print_strings_counter < 1024) {
		print_strings[print_strings_counter].lineno = lineno;
		strncpy(print_strings[print_strings_counter].value, string, 63);
		print_strings[print_strings_counter].value[63] = '\0';
		print_strings_counter++;
	}
}

// Free strings variable
void free_strings() {
	while (print_strings_counter > 0) {
		print_strings_counter--;
		memset(print_strings[print_strings_counter].value, 0, sizeof(print_strings[print_strings_counter].value)-1);
		print_strings[print_strings_counter].lineno = 0;
	}
}

// Expression parser (supports +, -, *, /)
void parse_factor() {
	if (current_token.type == TOKEN_NUMBER) {
		if (output) emit("    mov rax, %d\n", current_token.value);
		new_value = current_token.value;
	} else if (current_token.type == TOKEN_STRING) {
		if (output) emit("    lea rax, str_%d\n", print_strings_counter);
		add_string(lineno, current_token.str_val);
	} else if (current_token.type == TOKEN_IDENT) {
		Variable *var = find_variable(current_token.name);
		if (var) {
			if (var->type == TOKEN_NUMBER) {
				if (output) emit("    mov rax, [%s]\n", current_token.name);
			} else if (var->type == TOKEN_STRING) {
				if (output) emit("    lea rax, [%s]\n", current_token.name);
			}
		} else {
			error("Undefined variable");
		}
	} else {
		error("Expected number, identifier or string");
	}
}

void parse_product() {
	parse_factor();
	next_token();
	current_value = new_value;
	while (current_token.type == TOKEN_MUL || current_token.type == TOKEN_DIV) {
		TokenType op = current_token.type;
		next_token();
		if (output) {
			emit("    push rax\n");
		}
		parse_factor();
		if (output) {
			emit("    mov rbx, rax\n");
			emit("    pop rax\n");
		}

		if (op == TOKEN_MUL) {
			current_value *= new_value;
			if (output) emit("    mul rbx\n");
		} else {
			current_value /= new_value;
			if (output) emit("    xor rdx, rdx\n    div rbx\n");
		}
	}
}

void parse_expression() {
	parse_product();
	current_value = new_value;
	while (current_token.type == TOKEN_ADD || current_token.type == TOKEN_SUB) {
		TokenType op = current_token.type;
		next_token();
		if (output) {
			emit("    push rax\n");
		}
		parse_product();
		if (output) {
			emit("    mov rbx, rax\n");
			emit("    pop rax\n");
		}

		if (op == TOKEN_ADD) {
			current_value += new_value;
			if (output) emit("    add rax, rbx\n");
		} else {
			current_value -= new_value;
			if (output) emit("    sub rax, rbx\n");
		}
	}
}

void parse_print() {
	next_token();
	if (current_token.type == TOKEN_STRING) {
		if (output) emit("    mov rax, 0\n    mov rdi, ostring\n    mov rsi, str_%d\n    call printf\n", lineno);
		add_string(lineno, current_token.str_val);
		next_token();
	} else if (current_token.type == TOKEN_IDENT) {
		Variable *var = find_variable(current_token.name);
		if (var) {
			if (var->type == TOKEN_STRING) {
				if (output) emit("    mov rax, 0\n    mov rdi, ostring\n    mov rsi, %s\n    call printf\n", var->name);
			} else {
				if (output) emit("    mov rax, 0\n    mov rdi, onumber\n    mov rsi, [%s]\n    call printf\n", var->name);
			}
		}
		next_token();
	}
}

void parse_input() {
	next_token(); // Consume INPUT token
	if (current_token.type != TOKEN_STRING) error("Expected string after INPUT.");

	add_string(lineno, current_token.str_val);

	next_token(); // Consume string token
	if (current_token.type != TOKEN_COMMA) error("Expected ','");

	next_token(); // Consume comma token
	if (current_token.type != TOKEN_IDENT) error("Expected identifier");

	char var_name[64];
	strncpy(var_name, current_token.name, 63);
	var_name[63] = '\0';

	next_token(); // Consume identifier token
	if (output) {
		emit("    mov rdi, ostring2\n    mov rsi, str_%d\n    call printf\n", lineno);
		Variable *var = find_variable(var_name);
		if (var) {
			if ((var->flags << 0) & 1) {
				if (var->type == TOKEN_STRING) {
					emit("    mov rdi, istring\n    mov rsi, %s\n    call scanf\n", var_name);
				} else {
					emit("    mov rdi, inumber\n    mov rsi, %s\n    call scanf\n", var_name);
				}
			}
		} else {
			error("Variable doesn't exist");
		}
	}
}

void parse_let() {
	next_token();
	if (current_token.type != TOKEN_IDENT) error("Expected identifier");

	char var_name[64];
	strncpy(var_name, current_token.name, 63);
	var_name[63] = '\0';

	next_token();
	if (current_token.type != TOKEN_EQ) error("Expected '='");
	next_token();

	if (current_token.type == TOKEN_STRING) {
		if (output) emit("    lea rax, str_%d\n", print_strings_counter);
		add_variable_string(lineno, var_name, current_token.str_val, 0);
		next_token();
	} else {
		parse_expression();
		if (output) emit("    mov [%s], rax\n", var_name);
		add_variable_number(lineno, var_name, current_value, 0);
		next_token();
	}
}

void parse_dim() {
	next_token();
	if (current_token.type != TOKEN_IDENT) error("Expected identifier");

	char var_name[64];
	strncpy(var_name, current_token.name, 63);
	var_name[63] = '\0';

	next_token();
	if (current_token.type != TOKEN_EQ) error("Expected '='");

	next_token();
	if (current_token.type == TOKEN_STRING) {
		add_variable_string(lineno, var_name, "", 1);
		next_token();
	} else {
		parse_expression();
		add_variable_number(lineno, var_name, 0, 1);
		next_token();
	}
}

void parse_goto() {
	next_token();
	if (current_token.type != TOKEN_NUMBER) error("Expected line number");
	if (output) emit("    jmp label_%d\n", current_token.value);
	next_token();
}

void parse_exit() {
	if (output) emit("    mov rax, 60\n    xor rdi, rdi\n    syscall\n");
	next_token();
}

void parse_statement() {
	if (current_token.type == TOKEN_LINENO) {
		next_token();
		if (current_token.type == TOKEN_PRINT) {
			parse_print();
		} else if (current_token.type == TOKEN_INPUT) {
			parse_input();
		} else if (current_token.type == TOKEN_DIM) {
			parse_dim();
		} else if (current_token.type == TOKEN_LET) {
			parse_let();
		} else if (current_token.type == TOKEN_GOTO) {
			parse_goto();
		} else if (current_token.type == TOKEN_IF) {
			int line = lineno;
			if_statement = 1;
			label_counter = 0;
			next_token();
			parse_expression();
			TokenType comparison = current_token.type;
			if (comparison == TOKEN_LT || comparison == TOKEN_GT || comparison == TOKEN_EQ) {
				if (output) emit("    push rax\n");
				next_token();
				parse_expression();
				if (output) {
					emit("    pop rbx\n");
					emit("    cmp rbx, rax\n");
					switch (comparison) {
						case TOKEN_EQ: emit("    jne label_%d_%d\n", line, label_counter); break;
						case TOKEN_LT: emit("    jl label_%d_%d\n", line); break;
						case TOKEN_GT: emit("    jg label_%d_%d\n", line, label_counter); break;
						default: error("Unknown comparison operator");
					}
				}
			}
			if (current_token.type != TOKEN_COLON) error("Expected ':'");
			next_token();
			parse_statement();
			if (output) emit("label_%d_%d:\n", line, label_counter);
			label_counter++;
		} else if (current_token.type == TOKEN_EXIT) {
			parse_exit();
		} else {
			error("Invalid statement");
		}
	} else if (if_statement) {
		if_statement = 0;
		if (current_token.type == TOKEN_PRINT) {
			parse_print();
		} else if (current_token.type == TOKEN_LET) {
			parse_let();
		} else if (current_token.type == TOKEN_GOTO) {
			parse_goto();
		} else if (current_token.type == TOKEN_EXIT) {
			parse_exit();
		} else {
			error("Invalid statement");
		}
	} else {
		error("Invalid statement");
	}
}

void parse_program() {
	while (current_token.type != TOKEN_EOF) {
		parse_statement();
		if (current_token.type == TOKEN_EOL) next_token();
	}
}

int main(int argc, char **argv) {
	char program[16384];
	char line[256];
	int size = 0;

	while (fgets(line, sizeof(line), stdin)) {
		int len = (int)strlen(line);
		if (size + len >= 16383) break;
		strcpy(program+size, line);
		size += len;
		program[size] = '\0';
	}

	output = 0;
	input = program;
	next_token();
	parse_program();

	printf("section .bss\n");

	for (int i = 0; i < variable_counter; i++) {
		if ((variables[i].flags << 0) & 1) {
			switch (variables[i].type) {
				case TOKEN_NUMBER:
					printf("%s resq %d\n", variables[i].name, (int)sizeof(variables[i].value));
					break;
				case TOKEN_STRING:
					printf("%s resb %d\n", variables[i].name, (int)sizeof(variables[i].string));
					break;
				default:
					break;
			}
		}
	}

	printf("\nsection .data\n");
	printf("istring db \"%%[^\", 10, \"]\", 0\n");
	printf("inumber db \"%%d\", 0\n");
	printf("ostring db \"%%s\", 10, 13, 0\n");
	printf("ostring2 db \"%%s\", 0\n");
	printf("onumber db \"%%d\", 10, 13, 0\n");
	printf("crlf db 10, 0\n");
	printf("newbuf dq 0\n");

	for (int i = 0; i < print_strings_counter; i++) {
		printf("str_%d db \"%s\", 0\n", print_strings[i].lineno, print_strings[i].value);
	}

	for (int i = 0; i < variable_counter; i++) {
		if (!((variables[i].flags << 0) & 1)) {
			switch (variables[i].type) {
				case TOKEN_NUMBER:
					printf("%s dq %d\n", variables[i].name, variables[i].value);
					break;
				case TOKEN_STRING:
					printf("%s db \"%s\", 0\n", variables[i].name, variables[i].string);
					break;
				default:
					break;
			}
		}
	}

	printf("\nsection .text\n");
	printf("global _start\n\n");

	printf("extern strlen\n");
	printf("extern malloc\n");
	printf("extern strcpy\n");
	printf("extern strcat\n");
	printf("extern printf\n");
	printf("extern scanf\n");
	printf("extern free\n\n");

	printf("string_concat:\n");
	printf("    ; Assume RSI and RDI hold pointers to two strings\n");
	printf("    ; Allocate memory for the new string\n");
	printf("    push rdi\n");
	printf("    push rsi\n");
	printf("    call strlen\n");
	printf("    mov rbx, rax\n");
	printf("    pop rsi\n");
	printf("    call strlen\n");
	printf("    add rbx, rax\n");
	printf("    add rbx, 1 ; Null terminator\n");
	printf("    mov rdi, rbx\n");
	printf("    call malloc\n");
	printf("    mov rdi, rax\n");
	printf("    mov [newbuf], rax\n");
	printf("    pop rsi\n");
	printf("    call strcpy\n");
	printf("    pop rsi\n");
	printf("    call strcat\n");
	printf("    ret\n\n");

	printf("_start:\n\n");

	output = 1;
	input = program;
	next_token();
	parse_program();

	free_strings();
	free_variables();

	return 0;
}

