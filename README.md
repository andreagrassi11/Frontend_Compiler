# Frontend compiler

**Corso**: Linguaggi e compilatori <br/>
**Autori**: Mattia Lazzarini, Andrea Grassi <br/>

Modulo contente la logica di creazione del Front-end di un compilatore

## Per compilare e generare i file Lexer (flex) e Parser (Bison)

All'interno della directory del progetto:
```
make
```

Per cancellare i file generati dal file ``Makefile``:
```
make clean
```

## Per la generazione dell'Intermediate Representation (IR)

Eseguire il binario ``kfe`` con il parametro ``-o`` per generare il file oggetto:
```
./kfe -o simplefun simplefun.k 
```

Con l'opzione ``-v`` produce anche (su stdout) una rappresentazione dell'AST:
```
./kfe -v -o simplefun simplefun.k 
```

## Per testare il codice IR prodotto

Generare il file oggetto di ``main.cc``:
```
g++ -c main.cc
```

Per effettuare il link e compilare i file ``main.o`` e ``simplefun.o``:
```
g++ main.cc simplefun.k
```

L'eseguibile generato prende il nome ``a.out`` di default

Eseguire il programma:
```
./a.out 
```