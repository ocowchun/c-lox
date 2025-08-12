//
// Created by ocowchun on 2025/8/11.
//
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE

#include "debug.h"

#endif


typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} precedence;

typedef void (*parse_fn)();

typedef struct {
    parse_fn prefix;
    parse_fn infix;
    precedence prec;
} parse_rule;

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} parser;

parser global_parser;

Chunk *compiling_chunk;

static Chunk *current_chunk() {
    return compiling_chunk;
}


static void error_at(Token *t, const char *message) {
    if (global_parser.panic_mode) {
        return;
    }

    global_parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", t->line);
    if (t->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (t->type == TOKEN_ERROR) {
        // do nothing
    } else {
        fprintf(stderr, " at '%.*s'", t->length, t->start);
    }

    fprintf(stderr, ": %s\n", message);
    global_parser.had_error = true;
}

static void error(const char *message) {
    error_at(&global_parser.previous, message);
}

static void error_at_current(const char *message) {
    error_at(&global_parser.current, message);
}

static void advance() {
    global_parser.previous = global_parser.current;

    for (;;) {
        global_parser.current = scan_token();
        if (global_parser.current.type != TOKEN_ERROR) {
            break;
        }
        error_at_current(global_parser.current.start);
    }
}

static void consume(TokenType type, const char *message) {
    if (global_parser.current.type == type) {
        advance();
        return;
    }

    error_at_current(message);
}

static void emit_byte(uint8_t b) {
    write_chunk(current_chunk(), b, global_parser.previous.line);
}

static void emit_bytes(uint8_t b1, uint8_t b2) {
    emit_byte(b1);
    emit_byte(b2);
}

static void emit_return() {
    emit_byte(OP_RETURN);
}

static uint8_t make_constant(Value val) {
    int constant = add_constant(current_chunk(), val);
    // TODO: increase constant size to 2 bytes
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t) constant;
}

static void emit_constant(Value val) {
    emit_bytes(OP_CONSTANT, make_constant(val));
}


static void end_compiler() {
    emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!global_parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
}

static void expression();

static parse_rule *get_rule(TokenType type);

static void parse_precedence(precedence precedence);

static void binary() {
    TokenType operator_type = global_parser.previous.type;
    parse_rule *rule = get_rule(operator_type);
    parse_precedence((precedence) (rule->prec + 1));

    switch (operator_type) {
        case TOKEN_BANG_EQUAL: {
            emit_bytes(OP_EQUAL, OP_NOT);
            break;
        }
        case TOKEN_EQUAL_EQUAL: {
            emit_byte(OP_EQUAL);
            break;
        }
        case TOKEN_GREATER: {
            emit_byte(OP_GREATER);
            break;
        }
        case TOKEN_GREATER_EQUAL: {
            emit_bytes(OP_LESS, OP_NOT);
            break;
        }
        case TOKEN_LESS: {
            emit_byte(OP_LESS);
            break;
        }
        case TOKEN_LESS_EQUAL: {
            emit_bytes(OP_GREATER, OP_NOT);
            break;
        }
        case TOKEN_PLUS: {
            emit_byte(OP_ADD);
            break;
        }
        case TOKEN_MINUS: {
            emit_byte(OP_SUBTRACT);
            break;
        }
        case TOKEN_STAR: {
            emit_byte(OP_MULTIPLY);
            break;
        }
        case TOKEN_SLASH: {
            emit_byte(OP_DIVIDE);
            break;
        }
        default:
            // unreachable
            return;
    }
}

static void literal() {
    switch (global_parser.previous.type) {
        case TOKEN_FALSE: {
            emit_byte(OP_FALSE);
            break;
        }
        case TOKEN_NIL: {
            emit_byte(OP_NIL);
            break;
        }
        case TOKEN_TRUE: {
            emit_byte(OP_TRUE);
            break;
        }
        default:
            // unreachable
            return;
    }
}


static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
    double val = strtod(global_parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(val));
}

static void unary() {
    TokenType operator_type = global_parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_BANG: {
            emit_byte(OP_NOT);
            break;
        }
        case TOKEN_MINUS: {
            emit_byte(OP_NEGATE);
            break;
        }
        default:
            // unreachable
            return;
    }
}

parse_rule rules[] = {
        [TOKEN_LEFT_PAREN]    = {grouping, NULL, PREC_NONE},
        [TOKEN_RIGHT_PAREN]   = {NULL, NULL, PREC_NONE},
        [TOKEN_LEFT_BRACE]    = {NULL, NULL, PREC_NONE},
        [TOKEN_RIGHT_BRACE]   = {NULL, NULL, PREC_NONE},
        [TOKEN_COMMA]         = {NULL, NULL, PREC_NONE},
        [TOKEN_DOT]           = {NULL, NULL, PREC_NONE},
        [TOKEN_MINUS]         = {unary, binary, PREC_TERM},
        [TOKEN_PLUS]          = {NULL, binary, PREC_TERM},
        [TOKEN_SEMICOLON]     = {NULL, NULL, PREC_NONE},
        [TOKEN_SLASH]         = {NULL, binary, PREC_FACTOR},
        [TOKEN_STAR]          = {NULL, binary, PREC_FACTOR},
        [TOKEN_BANG]          = {unary, NULL, PREC_NONE},
        [TOKEN_BANG_EQUAL]    = {NULL, binary, PREC_NONE},
        [TOKEN_EQUAL]         = {NULL, NULL, PREC_NONE},
        [TOKEN_EQUAL_EQUAL]   = {NULL, binary, PREC_EQUALITY},
        [TOKEN_GREATER]       = {NULL, binary, PREC_COMPARISON},
        [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS]          = {NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS_EQUAL]    = {NULL, binary, PREC_COMPARISON},
        [TOKEN_IDENTIFIER]    = {NULL, NULL, PREC_NONE},
        [TOKEN_STRING]        = {NULL, NULL, PREC_NONE},
        [TOKEN_NUMBER]        = {number, NULL, PREC_NONE},
        [TOKEN_AND]           = {NULL, NULL, PREC_NONE},
        [TOKEN_CLASS]         = {NULL, NULL, PREC_NONE},
        [TOKEN_ELSE]          = {NULL, NULL, PREC_NONE},
        [TOKEN_FALSE]         = {literal, NULL, PREC_NONE},
        [TOKEN_FOR]           = {NULL, NULL, PREC_NONE},
        [TOKEN_FUN]           = {NULL, NULL, PREC_NONE},
        [TOKEN_IF]            = {NULL, NULL, PREC_NONE},
        [TOKEN_NIL]           = {literal, NULL, PREC_NONE},
        [TOKEN_OR]            = {NULL, NULL, PREC_NONE},
        [TOKEN_PRINT]         = {NULL, NULL, PREC_NONE},
        [TOKEN_RETURN]        = {NULL, NULL, PREC_NONE},
        [TOKEN_SUPER]         = {NULL, NULL, PREC_NONE},
        [TOKEN_THIS]          = {NULL, NULL, PREC_NONE},
        [TOKEN_TRUE]          = {literal, NULL, PREC_NONE},
        [TOKEN_VAR]           = {NULL, NULL, PREC_NONE},
        [TOKEN_WHILE]         = {NULL, NULL, PREC_NONE},
        [TOKEN_ERROR]         = {NULL, NULL, PREC_NONE},
        [TOKEN_EOF]           = {NULL, NULL, PREC_NONE},
};

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}


static void parse_precedence(precedence prec) {
    advance();
    parse_fn prefix_rule = get_rule(global_parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    prefix_rule();

    while (prec <= get_rule(global_parser.current.type)->prec) {
        advance();
        parse_fn infix_rule = get_rule(global_parser.previous.type)->infix;
        infix_rule();
    }
}

static parse_rule *get_rule(TokenType type) {
    return &rules[type];
}


bool compile(const char *source, Chunk *chunk) {
    init_scanner(source);
    compiling_chunk = chunk;

    global_parser.had_error = false;
    global_parser.panic_mode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    end_compiler();
    return !global_parser.had_error;
}
