//
// Created by ocowchun on 2025/8/11.
//
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"


typedef struct {
    const char *start;
    const char *current;
    int line;
} scanner;

scanner global_scanner;

void init_scanner(const char *source) {
    global_scanner.start = source;
    global_scanner.current = source;
    global_scanner.line = 1;
}

static bool is_at_end() {
    return *global_scanner.current == '\0';
}

static token make_token(token_type type) {
    token t;
    t.type = type;
    t.start = global_scanner.start;
    t.length = (int) (global_scanner.current - global_scanner.start);
    t.line = global_scanner.line;
    return t;
}

static token error_token(const char *message) {
    token t;
    t.type = TOKEN_ERROR;
    t.start = message;
    t.length = (int) strlen(message);
    t.line = global_scanner.line;
    return t;
}

static char advance() {
    global_scanner.current++;
    return global_scanner.current[-1];
}

static bool match(char expected) {
    if (is_at_end()) {
        return false;
    }
    if (*global_scanner.current != expected) {
        return false;
    }
    global_scanner.current++;
    return true;
}

static char peek() {
    return *global_scanner.current;
}

static char peek_next() {
    if (is_at_end()) {
        return '\0';
    }

    return global_scanner.current[1];
}

static void skip_whitespace() {
    for (;;) {
        switch (peek()) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                global_scanner.line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) {
                        advance();
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static token string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            global_scanner.line++;
        }
        advance();
    }

    if (is_at_end()) {
        return error_token("Unterminated string.");
    }

    // The closing quote.
    advance();
    return make_token(TOKEN_STRING);
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static token number() {
    while (is_digit(peek())) {
        advance();
    }

    if (peek() == '.' && is_digit(peek_next())) {
        advance();
        while (is_digit(peek())) {
            advance();
        }
    }

    return make_token(TOKEN_NUMBER);
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static token_type check_keyword(int start, int length, const char *rest, token_type type) {
    if (global_scanner.current - global_scanner.start == start + length &&
        memcmp(global_scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static token_type identifier_type() {
    switch (global_scanner.start[0]) {
        case 'a':
            return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e':
            return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (global_scanner.current - global_scanner.start > 1) {
                switch (global_scanner.start[1]) {
                    case 'a':
                        return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n':
            return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p':
            return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (global_scanner.current - global_scanner.start > 1) {
                switch (global_scanner.start[1]) {
                    case 'h':
                        return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w':
            return check_keyword(1, 4, "hile", TOKEN_WHILE);

    }
    return TOKEN_IDENTIFIER;
}

static token identifier() {
    while (is_alpha(peek()) || is_digit(peek())) {
        advance();
    }

    return make_token(identifier_type());
}

token scan_token() {
    skip_whitespace();
    global_scanner.start = global_scanner.current;
    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    char c = advance();

    if (is_alpha(c)) {
        return identifier();
    }

    if (is_digit(c)) {
        return number();
    }

    switch (c) {
        case '(':
            return make_token(TOKEN_LEFT_PAREN);
        case ')':
            return make_token(TOKEN_RIGHT_PAREN);
        case '{':
            return make_token(TOKEN_LEFT_BRACE);
        case '}':
            return make_token(TOKEN_RIGHT_BRACE);
        case ';':
            return make_token(TOKEN_SEMICOLON);
        case ',':
            return make_token(TOKEN_COMMA);
        case '.':
            return make_token(TOKEN_DOT);
        case '-':
            return make_token(TOKEN_MINUS);
        case '+':
            return make_token(TOKEN_PLUS);
        case '/':
            return make_token(TOKEN_SLASH);
        case '*':
            return make_token(TOKEN_STAR);
        case '!':
            return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string();
    }

    return error_token("Unexpected character.");
}



