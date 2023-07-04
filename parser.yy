%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.2"
%defines

%define api.token.constructor
%define api.location.file none
%define api.value.type variant
%define parse.assert

%code requires {
  # include <string>
  #include <exception>
  class driver;
  class RootAST;
  class ExprAST;
  class FunctionAST;
  class SeqAST;
  class PrototypeAST;

  class IfExprAST;
  class UnaryExprAST;
  class ForExprAST;
}

// The parsing context.
%param { driver& drv }

%locations

%define parse.trace
%define parse.error verbose

%code {
# include "driver.hh"
}

%define api.token.prefix {TOK_}
%token
	END  0  "end of file"
	SEMICOLON  ";"
	COMMA      ","
	MINUS      "-"
	PLUS       "+"
	STAR       "*"
	SLASH      "/"
	LPAREN     "("
	RPAREN     ")"
	EXTERN     "extern"
	DEF        "def"

	// ********** Estensione 1 **********
	LT         "<"
	GT         ">"
	LE         "<="  
	GE         ">=" 
	EQ         "=="
	NE         "!=" 

	IF         "if" 
	THEN       "then"
	ELSE       "else"
  ENDTOK     "end"
  
  // ********** Estensione 2 **********
  COLON      ":"

  // ********** Estensione 3 **********
  FOR        "for"
  ASSIGN     "="
  IN         "in"
;

%token <std::string> IDENTIFIER "id"
%token <double> NUMBER "number"
%type <ExprAST*> exp
%type <ExprAST*> idexp
%type <std::vector<ExprAST*>> optexp
%type <std::vector<ExprAST*>> explist
%type <RootAST*> program
%type <RootAST*> top
%type <FunctionAST*> definition
%type <PrototypeAST*> external
%type <PrototypeAST*> proto
%type <std::vector<std::string>> idseq

// ********** Estensione 1 **********
%type <IfExprAST*> ifexpr

// ********** Estensione 2 **********
%type <UnaryExprAST*> unaryexpr

// ********** Estensione 3 **********
%type <ForExprAST*> forexpr
%type <ExprAST*> step

%%
%start startsymb;

startsymb:
program             { drv.root = $1; }

program:
  %empty               { $$ = new SeqAST(nullptr,nullptr); }
|  top ";" program     { $$ = new SeqAST($1,$3); };

top:
%empty                 { $$ = nullptr; }
| definition           { $$ = $1; }
| external             { $$ = $1; }
| exp                  { $$ = $1; $1->toggle(); };

definition:
  "def" proto exp      { $$ = new FunctionAST($2,$3); $2->noemit(); };

external:
  "extern" proto       { $$ = $2; };

proto:
  "id" "(" idseq ")"   { $$ = new PrototypeAST($1,$3); };

idseq:
  %empty               { std::vector<std::string> args;
                         $$ = args; }
| "id" idseq           { $2.insert($2.begin(),$1); $$ = $2; };


%left "+" "-";
%left "*" "/";

exp:
  exp "+" exp          { $$ = new BinaryExprAST('+',$1,$3); }
| exp "-" exp          { $$ = new BinaryExprAST('-',$1,$3); }
| exp "*" exp          { $$ = new BinaryExprAST('*',$1,$3); }
| exp "/" exp          { $$ = new BinaryExprAST('/',$1,$3); }

// ********** Estensione 1 **********
| exp "<" exp          { $$ = new BinaryExprAST('<',$1,$3); }
| exp ">" exp          { $$ = new BinaryExprAST('>',$1,$3); }
| exp "<=" exp         { $$ = new BinaryExprAST('l',$1,$3); }
| exp ">=" exp         { $$ = new BinaryExprAST('g',$1,$3); }
| exp "==" exp         { $$ = new BinaryExprAST('E',$1,$3); }
| exp "!=" exp         { $$ = new BinaryExprAST('N',$1,$3); }

| ifexpr               { $$ = $1; }

// ********** Estensione 2 **********
| unaryexpr            { $$ = $1; }
| exp ":" exp          { $$ = new BinaryExprAST(':', $1,$3); }

// ********** Estensione 3 **********
| forexpr              { $$ = $1; }

| idexp                { $$ = $1; }
| "(" exp ")"          { $$ = $2; }
| "number"             { $$ = new NumberExprAST($1); };

idexp:
  "id"                 { $$ = new VariableExprAST($1); }
| "id" "(" optexp ")"  { $$ = new CallExprAST($1,$3); };

optexp:
%empty                 { std::vector<ExprAST*> args;
                         args.push_back(nullptr);
			 $$ = args;
                       }
| explist              { $$ = $1; };

explist:
  exp                  { std::vector<ExprAST*> args;
                         args.push_back($1);
			 $$ = args;
                       }
| exp "," explist      { $3.insert($3.begin(), $1); $$ = $3; };

// ********** Estensione 1 **********
ifexpr:
  "if" exp "then" exp "else" exp "end" {$$ = new IfExprAST($2, $4, $6); };

// ********** Estensione 2 **********
unaryexpr:
  "-" exp              { $$ = new UnaryExprAST('-', $2); }
| "+" exp              { $$ = new UnaryExprAST('+', $2); };

// ********** Estensione 3 **********
forexpr:
  "for" "id" "=" exp "," exp step "in" exp "end"         { $$ = new ForExprAST($2, $4, $6, $7, $9); };

step:
  %empty                   { $$ = nullptr; }
| "," exp                  { $$ = $2; };

%%

void
yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}
