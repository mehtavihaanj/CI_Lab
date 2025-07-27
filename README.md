# CI_Lab
An implementation of a command interpreter in the C Language
# Overview
This interpreter is divided into several segments: a lexer, parser and interpreter. 
These are appropriately named in the file structure, and work to implement a simple machine language (ASML)
in C. Lexical operators are first broken down and interpreted according to the ASML rules, then parsed and stored in a linked list of commands.
Function calls are also stored in a stack with a hash code to quickly access the respective commands that a function points to.
The final step is interpretation, where each command (branch, add, sub, load, etc.) is processed.
