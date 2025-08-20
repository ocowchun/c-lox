//
// Created by ocowchun on 2025/8/11.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"

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
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} parse_rule;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    // TODO: allow more than 256 local variables to be in scope at a time.
    Local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} Compiler;

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} parser;

parser global_parser;

Compiler *current_compiler = NULL;

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

static bool check(TokenType type) {
    return global_parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

static void emit_byte(uint8_t b) {
    write_chunk(current_chunk(), b, global_parser.previous.line);
}

static void emit_bytes(uint8_t b1, uint8_t b2) {
    emit_byte(b1);
    emit_byte(b2);
}

static void emit_loop(int loop_start) {
    emit_byte(OP_LOOP);

    int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static int emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
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

static void patch_jump(int offset) {
    // operand for how much to offset the ip

    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

static void init_compiler(Compiler *compiler) {
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    current_compiler = compiler;
}

static void end_compiler() {
    emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!global_parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
}

static void begin_scope() {
    current_compiler->scope_depth++;
}

static void end_scope() {
    current_compiler->scope_depth--;

    while (current_compiler->local_count > 0 &&
           current_compiler->locals[current_compiler->local_count - 1].depth > current_compiler->scope_depth) {
        emit_byte(OP_POP);
        current_compiler->local_count--;
    }
}

static void expression();

static void declaration();

static void statement();

static parse_rule *get_rule(TokenType type);

static void parse_precedence(Precedence precedence);

static uint8_t identifier_constant(Token *name) {
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static bool identifiers_equal(Token *a, Token *b) {
    if (a->length != b->length) {
        return false;
    }

    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(Compiler *compiler, Token *name) {
    for (int i = compiler->local_count - 1; i >= 0; --i) {
        Local *local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }

            return i;
        }
    }

    return -1;
}

static void add_local(Token name) {
    if (current_compiler->local_count == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local *local = &current_compiler->locals[current_compiler->local_count++];
    local->name = name;
    // set depth to -1 to point out the local is uninitialized.
    local->depth = -1;
}

static void declare_variable() {
    if (current_compiler->scope_depth == 0) {
        return;
    }

    Token *name = &global_parser.previous;

    for (int i = current_compiler->local_count - 1; i >= 0; --i) {
        Local *local = &current_compiler->locals[i];
        if (local->depth != -1 && local->depth < current_compiler->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    add_local(*name);
}

static uint8_t parse_variable(const char *error_message) {
    consume(TOKEN_IDENTIFIER, error_message);

    declare_variable();
    if (current_compiler->scope_depth > 0) {
        return 0;
    }


    return identifier_constant(&global_parser.previous);
}

static void mark_initialized() {
    current_compiler->locals[current_compiler->local_count - 1].depth = current_compiler->scope_depth;
}

static void define_variable(uint8_t global) {
    if (current_compiler->scope_depth > 0) {
        mark_initialized();
        return;
    }

    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static void and_(bool can_assign) {
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void binary(bool can_assign) {
    TokenType operator_type = global_parser.previous.type;
    parse_rule *rule = get_rule(operator_type);
    parse_precedence((Precedence) (rule->precedence + 1));

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

static void literal(bool can_assign) {
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


static void grouping(bool can_assign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool can_assign) {
    double val = strtod(global_parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(val));
}

static void or_(bool can_assign) {
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);
    parse_precedence(PREC_OR);

    patch_jump(end_jump);
}

static void string(bool can_assign) {
    emit_constant(OBJ_VAL(copy_string(global_parser.previous.start + 1, global_parser.previous.length - 2)));
}

static void named_variable(Token name, bool can_assign) {
    uint8_t getOp, setOp;
    int arg = resolve_local(current_compiler, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifier_constant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(setOp, (uint8_t) arg);
    } else {
        emit_bytes(getOp, (uint8_t) arg);
    }
}

static void variable(bool can_assign) {
    named_variable(global_parser.previous, can_assign);
}

static void unary(bool can_assign) {
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
        [TOKEN_IDENTIFIER]    = {variable, NULL, PREC_NONE},
        [TOKEN_STRING]        = {string, NULL, PREC_NONE},
        [TOKEN_NUMBER]        = {number, NULL, PREC_NONE},
        [TOKEN_AND]           = {NULL, and_, PREC_AND},
        [TOKEN_CLASS]         = {NULL, NULL, PREC_NONE},
        [TOKEN_ELSE]          = {NULL, NULL, PREC_NONE},
        [TOKEN_FALSE]         = {literal, NULL, PREC_NONE},
        [TOKEN_FOR]           = {NULL, NULL, PREC_NONE},
        [TOKEN_FUN]           = {NULL, NULL, PREC_NONE},
        [TOKEN_IF]            = {NULL, NULL, PREC_NONE},
        [TOKEN_NIL]           = {literal, NULL, PREC_NONE},
        [TOKEN_OR]            = {NULL, or_, PREC_OR},
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

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void var_declaration() {
    uint8_t global = parse_variable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    define_variable(global);
}

static void expression_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

static void for_statement() {
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    //initializer clause
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        expression_statement();
    }

    int loop_start = current_chunk()->count;
    int exit_jump = -1;
    // condition clause
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP); // pop condition
    }

    // increment clause
    if (!match(TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP); // pop condition
    }
    end_scope();
}

static void if_statement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (match(TOKEN_ELSE)) {
        statement();
    }
    patch_jump(else_jump);
}

static void print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void while_statement() {
    int loop_start = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);

}

static void synchronize() {
    global_parser.panic_mode = false;

    while (global_parser.current.type != TOKEN_EOF) {
        if (global_parser.previous.type == TOKEN_SEMICOLON) {
            return;
        }

        switch (global_parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                // do nothing
                ;
        }
        advance();
    }

}

static void declaration() {
    if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (global_parser.panic_mode) {
        synchronize();
    }
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else if (match(TOKEN_IF)) {
        if_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else {
        expression_statement();
    }
}


static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(global_parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(global_parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(global_parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}


static parse_rule *get_rule(TokenType type) {
    return &rules[type];
}


bool compile(const char *source, Chunk *chunk) {
    init_scanner(source);
    Compiler compiler;
    init_compiler(&compiler);
    compiling_chunk = chunk;

    global_parser.had_error = false;
    global_parser.panic_mode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    end_compiler();
    return !global_parser.had_error;
}
