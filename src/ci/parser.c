
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include <string.h>
#include "command_type.h"
#include "token_type.h"

static Token    advance(Parser *parser);
static bool     consume(Parser *parser, TokenType type);
static bool     is_at_end(Parser *parser);
static void     skip_nls(Parser *parser);
static bool     consume_newline(Parser *parser);
static Command *create_command(CommandType type);
static bool     is_variable(Token token);
static bool     parse_variable(Token token, int64_t *var_num);
static bool     parse_number(Token token, int64_t *result);
static bool     parse_variable_operand(Parser *parser, Operand *op);
static bool     parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate);
static Command *parse_cmd(Parser *parser);


void parser_init(Parser *parser, Lexer *lexer, LabelMap *map) {
    if (!parser) {
        return;
    }

    parser->lexer     = lexer;
    parser->had_error = false;
    parser->label_map = map;
    parser->current   = lexer_next_token(parser->lexer);
    parser->next      = lexer_next_token(parser->lexer);
}

/**
 * @brief Advances the parser in the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return The token that was just consumed.
 */
static Token advance(Parser *parser) {
    Token ret_token = parser->current;
    if (!is_at_end(parser)) {
        parser->current = parser->next;
        parser->next    = lexer_next_token(parser->lexer);
    }
    return ret_token;
}

/**
 * @brief Determines if the parser reached the end of the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True if the parser is at the end of the token stream, false
 * otherwise.
 */
static bool is_at_end(Parser *parser) {
    return parser->current.type == TOK_EOF;
}

/**
 * @brief Consumes the token if it matches the specified token type.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param type The type of the token to match.
 * @return True if the token was consumed, false otherwise.
 */
static bool consume(Parser *parser, TokenType type) {
    if (parser->current.type == type) {
        advance(parser);
        return true;
    }

    return false;
}

/**
 * @brief Creates a command of the given type.
 *
 * @param type The type of the command to create.
 * @return A pointer to a command with the requested type.
 *
 * @note It is the responsibility of the caller to free the memory associated
 * with the returned command.
 */
static Command *create_command(CommandType type) {
    Command *cmd = (Command *) calloc(1, sizeof(Command));
    if (!cmd) {
        return NULL;
    }
    cmd->type             = type;
    cmd->next             = NULL;
    cmd->is_a_immediate   = false;
    cmd->is_a_string      = false;
    cmd->is_b_immediate   = false;
    cmd->is_b_string      = false;
    cmd->branch_condition = BRANCH_NONE;
    return cmd;
}

/**
 * @brief Determines if the given token is a valid variable.
 *
 * A valid (potential) variable is a token that begins with the prefix "x",
 * followed by any other character(s).
 *
 * @param token The token to check.
 * @return True if this token could be a variable, false otherwise.
 */
static bool is_variable(Token token) {
    return token.length >= 2 && token.lexeme[0] == 'x';
}

/**
 * @brief Determines if the given token is a valid base signifier.
 *
 * A valid base signifier is one of d (decimal), x (hex), b (binary) or s (string).
 *
 * @param token The token to check.
 * @return True if this token is a base signifier, false otherwise
 */
static bool is_base(Token token) {
    return token.length == 1 && (token.lexeme[0] == 'd' || token.lexeme[0] == 'x' ||
                                 token.lexeme[0] == 's' || token.lexeme[0] == 'b');
}

/**
 * @brief Parses the given token as a base signifier
 *
 * A base is a single character, either d, s, x, or b.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if the current token was parsed as a base, false otherwise.
 */
static bool parse_base(Parser *parser, Operand *op) {

    Token cur = parser->current; 
    if (!is_base(cur)) {
        return false;
    }

    op->base = cur.lexeme[0];   
    if (!is_at_end(parser)) {
     advance(parser); 
    }
    return true;
}

/**
 * @brief Parses the given token as a variable.
 *
 * @param token The token to parse.
 * @param var_num a pointer to modify on success.
 * @return True if `var_num` was successfully modified, false otherwise.
 *
 * @note It is assumed that the token already was verified to begin with a valid
 * prefix, "x".
 */
static bool parse_variable(Token token, int64_t *var_num) {
    char   *endptr;
    
    int64_t tempnum = strtol(token.lexeme + 1, &endptr, 10);

    if ((token.lexeme + token.length) != endptr || tempnum < 0 || tempnum > 31) {
        return false;
    }

    *var_num = tempnum;
    return true;
}

/**
 * @brief Parses the given value as a number.
 *
 * @param token The token to parse.
 * @param result A pointer to the value to modify on success.
 * @return True if `result` was successfully modified, false otherwise.
 */
static bool parse_number(Token token, int64_t *result) {
    const char *parse_start = token.lexeme;
    int         base        = 10;

    if (token.length > 2 && token.lexeme[0] == '0') {
        if (token.lexeme[1] == 'x') {
            parse_start += 2;
            base = 16;
        } else if (token.lexeme[1] == 'b') {
            parse_start += 2;
            base = 2;
        }
    }

    char *endptr;
    *result = strtoll(parse_start, &endptr, base);

    return (token.lexeme + token.length) == endptr;
}

/**
 * @brief Conditionally parses the current token as a number.
 *
 * Note that this won't advance the parser if the token cannot be converted to
 * an integer.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this token is a number and was was converted successfully,
 * false otherwise.
 */
static bool parse_im(Parser *parser, Operand *op) {
    Token cur = parser->current;
    if (cur.type != TOK_NUM || !parse_number(cur, &op->num_val)) {
        return false;
    }
    if (!is_at_end(parser)) {
     advance(parser); 
    }
    return true;
}

/**
 * @brief Parses the next token as a variable.
 *
 * A variable is anything starting with the prefix x and will be of type
 * TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this was parsed as a variable, false otherwise.
 */
static bool parse_variable_operand(Parser *parser, Operand *op) {
    
    Token cur = parser->current; 
    if (!is_variable(cur)) {
        return false;
    }
   

    bool parsed = parse_variable(cur, &op->num_val);
    int64_t val = op->num_val;

    if (val < 0 || val > 31 || !parsed) {
        return false;
    }

    if (!is_at_end(parser)) {
     advance(parser); 
    }
    return parse_variable(cur, &op->num_val);
}

/**
 * @brief Parses the next token as either a variable or an immediate.
 *
 * A number is considered to be anything beginning with a decimal digit or the
 * prefixes 0b or 0x and will be of type TOK_NUM. A variable is anything
 * starting with the prefix x and will be of type TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @param is_immediate A pointer to a boolean to modify upon determining whether
 * the given value is an immediate.
 * @return True if this was parsed as an immediate or a variable, false
 * otherwise.
 */
static bool parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate) {
     
        Token cur = parser->current;
        
        if (cur.type == TOK_NUM) {
            *is_immediate = true;
            if (!parse_number(cur, &op->num_val)) {
                
                return false;
            }
            else {
                if (!is_at_end(parser))
                {
                    advance(parser);
                }
                return true;
            }
        }
        if (!is_variable(cur)) {
            return false;
        }
    bool parsed = parse_variable(cur, &op->num_val);
    int64_t val = op->num_val;
    if (val < 0 || val > 31 || !parsed) {
        return false;
    }
    *is_immediate = false;
    
   if (!is_at_end(parser))
   {
    advance(parser);
   }
    return true;
}

/**
 * @brief Skips past tokens that signal the start of a new line
 *
 * Consumes newlines until the end of file is reached.
 * An EOF is not considered to be a "new line" in this context because it is a
 * sentinel token, I.e, there is nothing after it.
 *
 * @param parser A pointer to the parser to read tokens from.
 */
static void skip_nls(Parser *parser) {
    while (consume(parser, TOK_NL))
        ;
}

/**
 * @brief Consumes a single newline
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True whether a "new line" was consumed, false otherwise.
 *
 * @note An encounter of TOK_EOF should not be considered a failure, as this
 * procedure is designed to "reset" the grammar. In other words, it should be
 * used to ensure that we have a valid ending token after encountering an
 * instruction. Since TOK_EOF signals no more possible instructions, it should
 * effectively play the role of a new line when checking for a valid ending
 * sequence for a command.
 */
static bool consume_newline(Parser *parser) {
    return consume(parser, TOK_NL) || consume(parser, TOK_EOF);
}

/**
 * @brief Parses a singular command.
 *
 * Reads in the token(s) from the lexer that the parser owns and determines the
 * appropriate matching command. Updates the parser->had_error if an error
 * occurs.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return A pointer to the appropriate command.
 * Returns null if an error occurred or there are no commands to parse.
 *
 * @note The caller is responsible for freeing the memory associated with the
 * returned command.
 */
static Command *parse_cmd(Parser *parser) {

    Token token = parser->current;
     
    if (!is_at_end(parser)) {
        advance(parser);
    }   
    
    
    if (token.type == TOK_NL) {
        consume_newline(parser);
        return NULL;
    }

    if (token.type == TOK_EOF) {
        consume_newline(parser);
        return NULL;
    }

  
   
    struct cmd *command_ptr = create_command(CMD_ADD);

    if (token.type == TOK_IDENT) { 
        
        if (parser->current.type != TOK_COLON) {
            parser->had_error = true;
        }

        char* inputString = calloc(1, token.length + 1);
        strncpy(inputString, token.lexeme, token.length);
        inputString[token.length] = '\0'; 
        
        put_label(parser->label_map, inputString, command_ptr);
    
        advance(parser);
        if (parser->current.type == TOK_NL) {
            while (parser->current.type == TOK_NL && !is_at_end(parser)) {
                advance(parser);
            } 
        }
        token = parser->current;
        if (!is_at_end(parser)) {
            advance(parser); 
        }
        else {
           
            command_ptr->is_b_immediate = true;
            command_ptr->destination.num_val = 0;
            command_ptr->val_a.num_val = 0;
            return command_ptr;
        }
    }
    
    
    
    if (token.type == TOK_EOF) {
        consume_newline(parser);
        free(command_ptr);
        return NULL;
    }
   

    switch (token.type) {
        
        case TOK_ADD:
        {
           
           if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_var_or_imm(parser, &command_ptr->val_b, &command_ptr->is_b_immediate) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_ADD;
           return command_ptr;
        }
        case TOK_SUB: {
           if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_var_or_imm(parser, &command_ptr->val_b, &command_ptr->is_b_immediate) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {
                
                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_SUB;
           return command_ptr;
        }
        case TOK_MOV:
        {
           if (!parse_variable_operand(parser, &command_ptr->destination) || 
                !parse_im(parser, &command_ptr->val_a)||
                (parser->current.type != TOK_NL  && parser->current.type != TOK_EOF)) {

                free(command_ptr);    
                parser->had_error = true;
                return NULL;    
           }

           command_ptr->is_a_immediate = true;
           command_ptr->type = CMD_MOV;    
           return command_ptr;
        }
        case TOK_CMP: {
            
           if (!parse_variable_operand(parser, &command_ptr->destination) ||
                !parse_var_or_imm(parser, &command_ptr->val_a, &command_ptr->is_a_immediate) ||
                (parser->current.type != TOK_NL  && parser->current.type != TOK_EOF)) {

                free(command_ptr);    
                parser->had_error = true;
                return NULL;    
           }          
           
           command_ptr->type = CMD_CMP;
           return command_ptr;
        }
        case TOK_CMP_U: {

           if (!parse_variable_operand(parser, &command_ptr->destination) ||
                !parse_var_or_imm(parser, &command_ptr->val_a, &command_ptr->is_a_immediate) ||
                (parser->current.type != TOK_NL  && parser->current.type != TOK_EOF)) {

                free(command_ptr);    
                parser->had_error = true;
                return NULL;    
           }          
           
           command_ptr->type = CMD_CMP_U;   
           return command_ptr;
        }
        case TOK_AND: {

            if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_variable_operand(parser, &command_ptr->val_b) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_AND;
           return command_ptr;
        }
        case TOK_EOR: {
            if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_variable_operand(parser, &command_ptr->val_b) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_EOR;
           return command_ptr;
        }
        case TOK_ASR: {
            if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_im(parser, &command_ptr->val_b) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_ASR;
           return command_ptr;
        }
        case TOK_LSL: {
            if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_im(parser, &command_ptr->val_b) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_LSL;
           return command_ptr;
        }
        case TOK_LSR: {
            if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_im(parser, &command_ptr->val_b) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_LSR;
           return command_ptr;
        }
        case TOK_ORR: {
            if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_variable_operand(parser, &command_ptr->val_a) || 
               !parse_variable_operand(parser, &command_ptr->val_b) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_ORR;
           return command_ptr;
        }
        case TOK_STORE: {
             if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_var_or_imm(parser, &command_ptr->val_a, &command_ptr->is_a_immediate) || 
               !parse_im(parser, &command_ptr->val_b) || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_STORE;
           return command_ptr;
        }
        case TOK_LOAD: {
            if (!parse_variable_operand(parser, &command_ptr->destination) || 
               !parse_im(parser, &command_ptr->val_a) || 
               !parse_var_or_imm(parser, &command_ptr->val_b, &command_ptr->is_b_immediate)
               || (parser->current.type != TOK_NL 
               && parser->current.type != TOK_EOF)) {

                free(command_ptr);
                parser->had_error = true;
                return NULL;
           }
           
           command_ptr->type = CMD_LOAD;
           return command_ptr;
        }
        case TOK_PUT: {
            Token input = advance(parser);
        
            if (!parse_var_or_imm(parser, &command_ptr->val_a, &command_ptr->is_a_immediate) || input.type
                != TOK_STR) {
                free(command_ptr);
                parser->had_error = true;
                return NULL;
            }

         
            char* inputString = calloc(1, input.length + 1);
            strncpy(inputString, input.lexeme, input.length);  
            inputString[input.length] = '\0';
            command_ptr->type = CMD_PUT;
            command_ptr->val_b.str_val = inputString;
            return command_ptr;
        }
        
        case TOK_PRINT: {

            if (!parse_var_or_imm(parser, &command_ptr->val_a, &command_ptr->is_a_immediate) || 
                !parse_base(parser, &command_ptr->val_b) ||
                (parser->current.type != TOK_NL  && parser->current.type != TOK_EOF)) {
    
                free(command_ptr);    
                parser->had_error = true;
                return NULL;    
           }          
           
           command_ptr->type = CMD_PRINT; 
           return command_ptr;
        }
        case TOK_BRANCH: {
            token = advance(parser);
        
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }

            command_ptr->type = CMD_BRANCH;
            command_ptr->branch_condition = BRANCH_NONE;
            
            
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_BRANCH_EQ: {
            token = advance(parser);
        
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }

            command_ptr->type = CMD_BRANCH;
            command_ptr->branch_condition = BRANCH_EQUAL;
            
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_BRANCH_GE: {
            token = advance(parser);
        
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }

            command_ptr->type = CMD_BRANCH;
            command_ptr->branch_condition = BRANCH_GREATER_EQUAL;
            
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_BRANCH_GT: {
            token = advance(parser);
        
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }

            command_ptr->type = CMD_BRANCH;
            command_ptr->branch_condition = BRANCH_GREATER;
            
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_BRANCH_LE: {
            token = advance(parser);
        
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }

            command_ptr->type = CMD_BRANCH;
            command_ptr->branch_condition = BRANCH_LESS_EQUAL;
            
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_BRANCH_LT: {
            token = advance(parser);
        
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }

            command_ptr->type = CMD_BRANCH;
            command_ptr->branch_condition = BRANCH_LESS;
            
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_BRANCH_NEQ: {
            token = advance(parser);
        
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }

            command_ptr->type = CMD_BRANCH;
            command_ptr->branch_condition = BRANCH_NOT_EQUAL;
            
            
          
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_CALL: {
    

            token = parser->current;
            if ((parser->next.type != TOK_NL && parser->next.type != TOK_EOF) || token.type != TOK_IDENT) {
                if (parser->next.type != TOK_NL && parser->next.type != TOK_EOF) {
                    advance(parser);
                }
                parser->had_error = true;
                free(command_ptr);   
                return NULL;
            }
            advance(parser);
            command_ptr->type = CMD_CALL;
    
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            return command_ptr;
        }
        case TOK_RET: {
            
          
            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free(command_ptr);
                return NULL;
            }

            command_ptr->type = CMD_RET;
            char* inputString = calloc(1, token.length + 1);
            strncpy(inputString, token.lexeme, token.length);
            inputString[token.length] = '\0';  
            command_ptr->destination.str_val = inputString;
            advance(parser);
            return command_ptr;
        }
        
        default:
        {

            parser->had_error = true;
            free(command_ptr);
            break;
        }
    }
    
    return command_ptr;
}



Command *parse_commands(Parser *parser) {

    struct cmd *command_ptr = parse_cmd(parser);
    if (command_ptr == NULL) {
        while (command_ptr == NULL && !is_at_end(parser) && !parser->had_error) {
            command_ptr = parse_cmd(parser);
        }
    }

    struct cmd *ret_cmd = command_ptr;
    while (!is_at_end(parser) && !parser->had_error) {
       
        struct cmd *temp_cmd = parse_cmd(parser);
        if (temp_cmd != NULL) {
        
            command_ptr->next = temp_cmd;
            command_ptr = command_ptr->next;
        }
    }
    
    return ret_cmd;
}