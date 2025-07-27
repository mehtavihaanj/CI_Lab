#include "interpreter.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "command_type.h"
#include "mem.h"
#include <stdlib.h>

static bool    cond_holds(Interpreter *intr, BranchCondition cond);
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im);
static bool    print_base(Interpreter *intr, Command *cmd);


void interpreter_init(Interpreter *intr, LabelMap *map) {
    if (!intr) {
        return;
    }

    intr->had_error  = false;
    intr->label_map  = map;
    intr->is_greater = false;
    intr->is_equal   = false;
    intr->is_less    = false;
    intr->the_stack  = NULL;

    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        intr->variables[i] = 0;
    }
}

void interpret(Interpreter *intr, Command *commands) {
    
    if (!intr || !commands) {   
        return;
    }
  
   
    Command *current = commands;
  
    while (current && !intr->had_error) {
        switch (current->type) {         
            default:
                break;
            case CMD_MOV:
            {
                intr->variables[current->destination.num_val] = current->val_a.num_val;

                current = current->next;
                break;
            }
            case CMD_ADD:
            {
                int64_t num_1 = intr->variables[current->val_a.num_val];  
                int64_t num_2 = 0;
                if (current->is_b_immediate) {
                    num_2 = current->val_b.num_val;
                }
                else {
                    num_2 = intr->variables[current->val_b.num_val];
                }
              
                intr->variables[current->destination.num_val] = num_1 + num_2;

                current = current->next;
                break;
            }
            case CMD_SUB:
            {
                int64_t num_1 = intr->variables[current->val_a.num_val];  
                int64_t num_2 = 0;
                if (current->is_b_immediate) {
                    num_2 = current->val_b.num_val;
                }
                else {
                    num_2 = intr->variables[current->val_b.num_val];
                }
              
                intr->variables[current->destination.num_val] = num_1 - num_2;

                current = current->next;
                break;
            }
            case CMD_CMP:
            {
                int64_t first_val = 0;
                int64_t dest_val = intr->variables[current->destination.num_val];
                
                if (current->is_a_immediate) {
                    first_val = current->val_a.num_val;
                }
                else {
                    first_val = intr->variables[current->val_a.num_val];
                }
             
                if (dest_val > first_val) {
                    intr->is_greater = true;
                    intr->is_less = false;
                    intr->is_equal = false;
                }
                else if (dest_val < first_val) {
                    intr->is_less = true;
                    intr->is_greater = false;
                    intr->is_equal = false;
                }
                else {
                    intr->is_equal = true;
                    intr->is_greater = false;
                    intr->is_less = false;
                }
                
                current = current->next;
                break;
            }
            case CMD_CMP_U:
            {
                uint64_t first_val = 0;
                uint64_t dest_val = intr->variables[current->destination.num_val];
                
                if (current->is_a_immediate) {
                   
                    first_val = current->val_a.num_val;
                }
                else {
                    first_val = intr->variables[current->val_a.num_val];
                }
               
                if (dest_val > first_val) {
                    intr->is_greater = true;
                    intr->is_less = false;
                    intr->is_equal = false;
                }
                else if (dest_val < first_val) {
                    intr->is_less = true;
                    intr->is_greater = false;
                    intr->is_equal = false;
                }
                else {
                    intr->is_equal = true;
                    intr->is_greater = false;
                    intr->is_less = false;
                }
                
                current = current->next;
                break;
            }
            case CMD_AND: {
                int64_t val_1 = intr->variables[current->val_a.num_val];
                int64_t val_2 = intr->variables[current->val_b.num_val];
                intr->variables[current->destination.num_val] = val_1 & val_2;

                current = current->next;
                break;
            }
            case CMD_EOR: {
                int64_t val_1 = intr->variables[current->val_a.num_val];
                int64_t val_2 = intr->variables[current->val_b.num_val];
                intr->variables[current->destination.num_val] = val_1 ^ val_2;

                current = current->next;
                break;
            }
            case CMD_ASR: {
                int64_t val_1 = intr->variables[current->val_a.num_val];
                int64_t val_2 = current->val_b.num_val;
                intr->variables[current->destination.num_val] = val_1 >> val_2;

                current = current->next;
                break;
            }
            case CMD_LSL: {
                int64_t val_1 = intr->variables[current->val_a.num_val];
                int64_t val_2 = current->val_b.num_val;
               
                intr->variables[current->destination.num_val] = val_1 << val_2;

                current = current->next;
                break;
            }
            case CMD_LSR: {
                
                intr->variables[current->destination.num_val] = (uint64_t) intr->variables[current->val_a.num_val] >> current->val_b.num_val;

                current = current->next;
                break;
            }
            case CMD_ORR: {
                int64_t val_1 = intr->variables[current->val_a.num_val];
                int64_t val_2 = intr->variables[current->val_b.num_val];
                intr->variables[current->destination.num_val] = val_1 | val_2;

                current = current->next;
                break;
            }
            case CMD_STORE: {
             
                unsigned long numBytesToStore = (unsigned long) current->val_b.num_val; 
                unsigned long startingMemAddress = 0;  

                if (current->is_a_immediate) {
                    startingMemAddress = (unsigned long) current->val_a.num_val;
                }
                else {
                    startingMemAddress = (unsigned long) intr->variables[current->val_a.num_val];
                }

                if (!mem_store((uint8_t*)&intr->variables[current->destination.num_val], startingMemAddress, numBytesToStore)) {
                    intr->had_error = true;
                }
               
                current = current->next;
                break;
            }
            case CMD_LOAD: {

                unsigned long numBytesToLoad =  current->val_a.num_val;

                int64_t imCopy = current->val_b.num_val; 
                int64_t valCopy = intr->variables[current->val_b.num_val];
                intr->variables[current->destination.num_val] = 0;
                unsigned long startingMemAddress = 0;  
                

                if (current->is_b_immediate) {
                    startingMemAddress =  imCopy;
                }
                else {
                    startingMemAddress =  valCopy;
                }
              
                if (!mem_load((uint8_t*)&intr->variables[current->destination.num_val], startingMemAddress, numBytesToLoad)) {
                    intr->had_error = true;
                }
                current = current->next;
                break;
            }
            case CMD_PUT: {
                unsigned long startingMemAddress = 0;
                if (current->is_a_immediate) {
                    startingMemAddress = current->val_a.num_val;
                }
                else {
                    startingMemAddress = intr->variables[current->val_a.num_val];
                }
                char* strVal = current->val_b.str_val;
                int length = strlen(strVal);
                
                for (int i = 0; i < length + 1; i++) {
                    uint8_t charVal = (uint8_t) strVal[i];
              
                    if (!mem_store(&charVal, 
                     (startingMemAddress + i),  1)) {
                        intr->had_error = true;
                }
            }

                free(strVal);
                current = current->next;
                break;
                
            }
            case CMD_BRANCH: {
                if (   (current->branch_condition == BRANCH_EQUAL && intr->is_equal && !intr->is_greater && !intr->is_less)
                    || (current->branch_condition == BRANCH_GREATER && intr->is_greater && !intr->is_equal)
                    || (current->branch_condition == BRANCH_GREATER_EQUAL && (intr->is_greater || intr->is_equal))
                    || (current->branch_condition == BRANCH_LESS && intr->is_less && !intr->is_equal)
                    || (current->branch_condition == BRANCH_LESS_EQUAL && (intr->is_less || intr->is_equal))
                    || (current->branch_condition == BRANCH_NOT_EQUAL && (!intr->is_equal)) || 
                       (current->branch_condition == BRANCH_NONE)) {

                    char* id = current->destination.str_val;
                
                    Entry* entry = get_label(intr->label_map, id);
                    if (entry->id == NULL) {
                        intr->had_error = true;
                        printf("Label not found: %s\n", id);
                        break;
                    }
                
                    if (strcmp(id, entry->id) != 0) {
                        while (strcmp(id, entry->id) != 0 && entry->next != NULL) {                 
                            entry = entry->next;
                        }
                    }

                    if (entry->command == NULL) {
                        intr->had_error = true;
                        
                    }
                    
                    current = entry->command;
                }
                else {
                    current = current->next;
                }

                
                break;
            }
            case CMD_CALL: {
                
                StackEntry* st = (StackEntry*) calloc(1, sizeof(StackEntry));
                for (int i = 0; i < NUM_VARIABLES; i++) {
                    st->variables[i] = intr->variables[i];
                }

                st->command = current->next;
                if (intr->the_stack == NULL) {
                    intr->the_stack = st;
                }
                else {
                    StackEntry* temp = intr->the_stack;
                    while (temp->next != NULL) {
                        temp = temp->next;
                    }
                    temp->next = st;
                }

                char* id = current->destination.str_val;
                Entry* entry = get_label(intr->label_map, id);  

                if (entry == NULL || entry->command == NULL) {
                    intr->had_error = true;
                    printf("Label not found: %s\n", id);
                    break;
                }

                if (strcmp(id, entry->id) != 0) {
                    while (strcmp(id, entry->id) != 0 && entry->next != NULL) {
                            entry = entry->next;
                    }
                }

                

                current = entry->command; 
                break;
            }
            case CMD_RET: {
                
                if (intr->the_stack == NULL) {
                    current = NULL;
                    break;
                }
                
            
                StackEntry* temp = intr->the_stack;
                if (temp->next == NULL) {
                    for (int i = 1; i < NUM_VARIABLES; i++) {
                        intr->variables[i] = temp->variables[i];
                    }
                    
                    current = temp->command;
                    free(intr->the_stack);
                    intr->the_stack = NULL;
                    break;
                }
                while (temp->next != NULL) {
                    temp = temp->next;
                }
                
              
                for (int i = 1; i < NUM_VARIABLES; i++) {
                    intr->variables[i] = temp->variables[i];
                }

                current = temp->command;
                temp = intr->the_stack;
                while (temp->next != NULL && temp->next->next != NULL) {
                    temp = temp->next;
                }

                free(temp->next);
                temp->next = NULL;
                break;
            }
            
            case CMD_PRINT:
            {
                print_base(intr, current);
                current = current->next; 
                break;
            }
            
        }    
                  
    }
   
    if (intr->the_stack != NULL)
    {
        while (intr->the_stack->next != NULL) {
            StackEntry* temp = intr->the_stack;
            intr->the_stack = intr->the_stack->next;
            free(temp);
        }
        
    
    free(intr->the_stack);
    }
}

void print_interpreter_state(Interpreter *intr) {
    if (!intr) {
        return;
    }

    printf("Error: %d\n", intr->had_error);
    printf("Flags:\n");
    printf("Is greater: %d\n", intr->is_greater);
    printf("Is equal: %d\n", intr->is_equal);
    printf("Is less: %d\n", intr->is_less);

    printf("\n");

    printf("Variable values:\n");
    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        printf("x%zu: %" PRId64 "", i, intr->variables[i]);

        if (i < NUM_VARIABLES - 1) {
            printf(", ");
        }

        if ((i + 1) % 8 == 0) {
            printf("\n");
        }
    }

    printf("\n");
}

/**
 * @brief Fetches the appropriate value from the given operand.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param op The operand used to fetch the value.
 * @param is_im A boolean representing whether this value is an immediate or
 * must be read in from the interpreter state.
 * @return The fetched value.
 */
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im) {
    // STUDENT TODO: Fetch either a variable from the interpreter's state or directly output a value
    return -1;
}

/**
 * @brief Determines whether a given branch condition holds.
 *
 * @param intr The pointer to the interpreter holding the result of the
 * comparison.
 * @param cond The condition to check.
 * @return True if the given condition holds, false otherwise.
 */
static bool cond_holds(Interpreter *intr, BranchCondition cond) {
    // STUDENT TODO: Determine whether a given condition holds using the interpreter's state
    return false;
}

/**
 * @brief Prints the given command's value in a specified base.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param cmd The command being processed.
 * @return True whether the print was successful, false otherwise.
 */
static bool print_base(Interpreter *intr, Command *cmd) {

    int64_t first_val = 0;
    if (cmd->is_a_immediate) {
        first_val = cmd->val_a.num_val;
    }
    else {
        first_val = intr->variables[cmd->val_a.num_val];
    }

    char base = cmd->val_b.base;
    if (base == 'd') {
        printf("%ld\n", first_val);
    }
    else if (base == 'x') {
        printf("0x");
        printf("%lx\n", (uint64_t) first_val);
    }
    else if (base == 'b') {
        printf("0b");
        if (first_val == 0) {
            printf("0");
        }
        else {
            int cutoff = 63;
            for (int i = 63; i >= 0; i--) {
                if (first_val & (1LL << i)) {
                    break;
                }
                cutoff--;
            }
          
            for (int i = cutoff; i >= 0; i--) {
                printf("%d", (first_val & (1LL << i) ? 1 : 0));
            }
        }    
        printf("\n");
    }
    else {
        uint8_t val = 1;
        mem_load(&val, first_val, 1);
       
        int i = 1;
        while (val != '\0') {
            printf("%c",  val);
            mem_load(&val, first_val + i, 1);
            i++;
        }
        printf("\n");
    }
    return true;
}