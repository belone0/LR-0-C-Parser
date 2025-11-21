# PARSER LR (0) C++ - COMPILADORES

### GRUPO:

JOÃO PEDRO DO PRADO BELONE

SEGIO KENJI SAWASAKI TANAKA

RODRIGO CAMPOS SCARABELLO

GUSTAVO ARAUJO NAKAMURA

PEDRO GABRIEL XAVIER VEIGA

---

Files:
- `lr0_parser.cpp`: single-file C++ LR(0) parser and automaton builder
- `grammar.txt`: example grammar (space-separated tokens)

Grammar file format:
- Each production on its own line: `A -> alpha | beta`
- Alternatives separated by `|`
- Use `eps` to denote the empty production (epsilon)
- Nonterminals: any LHS symbols (typically uppercase)
- Terminals: other symbols (use single tokens separated by spaces)

Example grammar (included):
S -> A
A -> a A | a

Build:
```
make
```

Run (example):
```
./lr0_parser grammar.txt "a a a"
```

Notes:
- This is a teaching implementation for LR(0). Some grammars are not LR(0) and will produce conflicts.
- Conflict resolution is naive: shift preferred over reduce; reduce/reduce picks smaller production index.
- The program prints states, ACTION and GOTO tables, and a trace of parsing steps.
