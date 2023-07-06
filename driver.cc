#include "driver.hh"
#include "parser.hh"
#include "llvm/Support/raw_os_ostream.h"
#include <typeinfo>

Value *LogErrorV(const std::string Str) {
  std::cerr << Str << std::endl;
  return nullptr;
}

// *********** Estensione 4 ***********
// Funzione per gestire le allocazioni, crea un blocco apposito per esse
// CreateEntryBlockAlloca - Crea un'istruzione "alloca" nel blocco di ingresso della funzione.
// Questo è usato per variabili mutabili, iteratori nei for, ecc...
static AllocaInst *CreateEntryBlockAlloca(driver &drv, Function *TheFunction, const std::string &VarName, Value *arraySize = nullptr) {
  IRBuilder<> TmpBuilder(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin()); // Genero un builder temporaneo
  return TmpBuilder.CreateAlloca(Type::getDoubleTy(*drv.context), arraySize, VarName); // Creo l'istruzione alloca
}

/*************************** Driver class *************************/
driver::driver(): trace_parsing (false), trace_scanning (false), ast_print (false) {
  context = new LLVMContext;
  module = new Module("Kaleidoscope", *context);
  builder = new IRBuilder(*context);
};

int driver::parse (const std::string &f) {
  file = f;
  location.initialize(&file);
  scan_begin();
  yy::parser parser(*this);
  parser.set_debug_level(trace_parsing);
  int res = parser.parse();
  scan_end();
  return res;
}

void driver::codegen() {
  if (ast_print) root->visit();
  std::cout << std::endl;
  root->codegen(*this);
};

/********************** Handle Top Expressions ********************/
Value* TopExpression(ExprAST* E, driver& drv) {
  // Crea una funzione anonima anonima il cui body è un'espressione top-level
  // viene "racchiusa" un'espressione top-level
  E->toggle(); // Evita la doppia emissione del prototipo
  PrototypeAST *Proto = new PrototypeAST("__espr_anonima"+std::to_string(++drv.Cnt),
		  std::vector<std::string>());
  Proto->noemit();
  FunctionAST *F = new FunctionAST(std::move(Proto),E);
  auto *FnIR = F->codegen(drv);
  FnIR->eraseFromParent();
  return nullptr;
};

/************************ Expression tree *************************/
  // Inverte il flag che definisce le TopLevelExpression
  // ando viene chiamata
void ExprAST::toggle() {
  top = top ? false : true;
};

bool ExprAST::gettop() {
  return top;
};

/************************* Sequence tree **************************/
SeqAST::SeqAST(RootAST* first, RootAST* continuation):
  first(first), continuation(continuation) {};

void SeqAST:: visit() {
  if (first != nullptr) {
    first->visit();
  } else {
    if (continuation == nullptr) {
      return;
    };
  };
  std::cout << ";" << "\n\n";
  continuation->visit();
};

Value *SeqAST::codegen(driver& drv) {
  if (first != nullptr) {
    Value *f = first->codegen(drv);
  } else {
    if (continuation == nullptr) return nullptr;
  }
  Value *c = continuation->codegen(drv);
  return nullptr;
};

/********************* Number Expression Tree *********************/
NumberExprAST::NumberExprAST(double Val): Val(Val) { top = false; };
void NumberExprAST::visit() {
  std::cout << Val << " ";
};

Value *NumberExprAST::codegen(driver& drv) {  
  if (gettop()) return TopExpression(this, drv);
  else return ConstantFP::get(*drv.context, APFloat(Val));
};

/****************** Variable Expression TreeAST *******************/
VariableExprAST::VariableExprAST(std::string &Name):
  Name(Name) { top = false; };

const std::string& VariableExprAST::getName() const {
  return Name;
};

void VariableExprAST::visit() {
  std::cout << getName() << " ";
};

Value *VariableExprAST::codegen(driver& drv) {
  if (gettop()) {
    return TopExpression(this, drv);

  } else {
    AllocaInst *A = drv.NamedValues[Name];

    if (!A)
      return LogErrorV("Unknown variable name");

    // "load" della variabile
    return drv.builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
  }
};

/******************** Binary Expression Tree **********************/
BinaryExprAST::BinaryExprAST(char Op, ExprAST* LHS, ExprAST* RHS):
  Op(Op), LHS(LHS), RHS(RHS) { top = false; };
 
void BinaryExprAST::visit() {
  std::cout << "( " << Op << " ";
  LHS->visit();
  
  if (RHS!=nullptr)
    RHS->visit();

  std::cout << " )";
};

Value *BinaryExprAST::codegen(driver& drv) {
  if (gettop()) {
    return TopExpression(this, drv);
    
  } else {

    // *********** Estensione 4 ***********
    if(Op == '=') {
      // Richediamo che LHS sia un identificatore/variabile
      VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS);
      if (!LHSE)
        return LogErrorV("destination of '=' must be a variable");

      // Solito gioco della codegen con RHS
      Value *Val = RHS->codegen(drv);
      if (!Val)
        return nullptr;

      // Controlliamo l'esistenza nella tabella dei simboli
      Value *Variable = drv.NamedValues[LHSE->getName()];
      if (!Variable)
        return LogErrorV("Unknown variable name");
     
      // Generiamo la "store"
      drv.builder->CreateStore(Val, Variable);
      return Val;
    }

    Value *L = LHS->codegen(drv);
    Value *R = RHS->codegen(drv);

    if (!L || !R)
      return nullptr;

    switch (Op) {
      case '+':
        return drv.builder->CreateFAdd(L, R, "addregister");
      case '-':
        return drv.builder->CreateFSub(L, R, "subregister");
      case '*':
        return drv.builder->CreateFMul(L, R, "mulregister");
      case '/':
        return drv.builder->CreateFDiv(L, R, "addregister");
      
      /***** Estensione 1 *****/
      case 'E':
        return drv.builder->CreateFCmpOEQ(L, R, "eqregister");
      case 'N': 
        return drv.builder->CreateFCmpONE(L, R, "neregister");
      case '>': 
        return drv.builder->CreateFCmpOGT(L, R, "gtregister");
      case '<': 
        return drv.builder->CreateFCmpOLT(L, R, "ltregister");
      case 'g': 
        return drv.builder->CreateFCmpOGE(L, R, "geregister");
      case 'l': 
        return drv.builder->CreateFCmpOLE(L, R, "leregister");
      
      /***** Estensione 2 *****/
      case ':':
        return R;

      default:  
        return LogErrorV("Operatore binario non supportato");
    }
  }
};

/********************* Call Expression Tree ***********************/
CallExprAST::CallExprAST(std::string Callee, std::vector<ExprAST*> Args): Callee(Callee),
									  Args(std::move(Args)) { top = false; };
                    
void CallExprAST::visit() {
  std::cout << Callee << "( ";
  for (ExprAST* arg : Args) {
    arg->visit();
  };
  std::cout << ')';
};

Value *CallExprAST::codegen(driver& drv) {
  if (gettop()) {
    return TopExpression(this, drv);
  } else {
    // Cerchiamo la funzione nell'ambiente globale
    Function *CalleeF = drv.module->getFunction(Callee);
    if (!CalleeF)
      return LogErrorV("Funzione non definita");
    // Controlliamo che gli argomenti coincidano in numero coi parametri
    if (CalleeF->arg_size() != Args.size())
      return LogErrorV("Numero di argomenti non corretto");
    std::vector<Value *> ArgsV;
    for (auto arg : Args) {
      ArgsV.push_back(arg->codegen(drv));
      if (!ArgsV.back())
	return nullptr;
    }
    return drv.builder->CreateCall(CalleeF, ArgsV, "calltmp");
  }
}

/************************* Prototype Tree *************************/
PrototypeAST::PrototypeAST(std::string Name, std::vector<std::string> Args): Name(Name),
									     Args(std::move(Args)) { emit = true; };
const std::string& PrototypeAST::getName() const { return Name; };
const std::vector<std::string>& PrototypeAST::getArgs() const { return Args; };
void PrototypeAST::visit() {
  std::cout << "EXTERN " << getName() << "( ";
  for (auto it=getArgs().begin(); it!= getArgs().end(); ++it) {
    std::cout << *it << ' ';
  };
  std::cout << ')';
};

void PrototypeAST::noemit() { emit = false; };

bool PrototypeAST::emitp() { return emit; };

Function *PrototypeAST::codegen(driver& drv) {
  // Costruisce una struttura double(double,...,double) che descrive 
  // tipo di ritorno e tipo dei parametri (in Kaleidoscope solo double)
  std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*drv.context));
  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(*drv.context), Doubles, false);
  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name, *drv.module);

  // Attribuiamo agli argomenti il nome dei parametri formali specificati dal programmatore
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  if (emitp()) {  // emitp() restituisce true se e solo se il prototipo è definito extern
    F->print(errs());
    fprintf(stderr, "\n");
  };
  
  return F;
}

/************************* Function Tree **************************/
FunctionAST::FunctionAST(PrototypeAST* Proto, ExprAST* Body):
  Proto(Proto), Body(Body) {
  if (Body == nullptr) external=true;
  else external=false;
};

void FunctionAST::visit() {
  std::cout << Proto->getName() << "( ";
  for (auto it=Proto->getArgs().begin(); it!= Proto->getArgs().end(); ++it) {
    std::cout << *it << ' ';
  };
  std::cout << ')';
  Body->visit();
};

Function *FunctionAST::codegen(driver& drv) {
  // Verifica che non esiste già, nel contesto, una funzione con lo stesso nome
  std::string name = Proto->getName();
  Function *TheFunction = drv.module->getFunction(name);
  // E se non esiste prova a definirla
  if (TheFunction) {
    LogErrorV("Funzione "+name+" già definita");
    return nullptr;
  }
  if (!TheFunction)
    TheFunction = Proto->codegen(drv);
  if (!TheFunction)
    return nullptr;  // Se la definizione "fallisce" restituisce nullptr

  // Crea un blocco di base in cui iniziare a inserire il codice
  BasicBlock *BB = BasicBlock::Create(*drv.context, "entry", TheFunction);
  drv.builder->SetInsertPoint(BB);

  // Registra gli argomenti nella symbol table
  drv.NamedValues.clear();

  // ESTENSIONE 4 -> Modifico questo codegen perchè per ogni argomento, creiamo un'alloca, memorizziamo il valore di input della funzione nell'alloca e registriamo l'alloca come posizione di memoria per l'argomento.
  for (auto &Arg : TheFunction->args()) {
    // Creo un alloca per quella variabile
    AllocaInst *Alloca = CreateEntryBlockAlloca(drv, TheFunction, std::string(Arg.getName()));

    // Creo una store per salvare il valore iniziale
    drv.builder->CreateStore(&Arg, Alloca);

    // Aggiungo argomento alla tabella dei simboli
    drv.NamedValues[std::string(Arg.getName())] = Alloca;
  }

  if (Value *RetVal = Body->codegen(drv)) {
    // Termina la creazione del codice corrispondente alla funzione
    drv.builder->CreateRet(RetVal);

    // Effettua la validazione del codice e un controllo di consistenza
    raw_os_ostream ostream(std::cout);
    std::cerr<<"\n";
    if(verifyFunction(*TheFunction, &ostream))
    {
      std::cerr<<"\nErrore: Funzione malformata\n";

      TheFunction->eraseFromParent();
      return nullptr;
    }

    TheFunction->print(errs());
    fprintf(stderr, "\n");
    return TheFunction;
  }

  // Errore nella definizione. La funzione viene rimossa
  TheFunction->eraseFromParent();
  return nullptr;
};


/************************* Estensione 1 **************************/
IfExprAST::IfExprAST(ExprAST* condizione, ExprAST* branchTrue, ExprAST* branchFalse) :
  condizione(std::move(condizione)),
  branchTrue(std::move(branchTrue)),
  branchFalse(std::move(branchFalse))
  {top = false;}

void IfExprAST::visit() {
  std::cout<<"( IF ";
  condizione->visit();
  std::cout<<" THEN ( ";
  branchTrue->visit();
  std::cout<<" ) ELSE ( ";
  branchFalse->visit();
  std::cout<<" ) ) ";
};

Value *IfExprAST::codegen(driver &drv) {
  // verifico che non sia un istruzione di tipo top
  if(gettop()) {
    return TopExpression(this, drv);

  } else {
    Value *checkCond = condizione->codegen(drv);

    if(!checkCond)
      return nullptr;
    
    if(checkCond->getType()->isDoubleTy()) //controllo se è un double
      checkCond = drv.builder->CreateFCmpONE(checkCond, ConstantFP::get(*drv.context, APFloat(0.0)), " IF COND DOUBLE ");
    
    Function *func = drv.builder->GetInsertBlock()->getParent();  // dove devo scrivere, prendo il blocco della funzione blocco entry

    BasicBlock *ThenBB = BasicBlock::Create(*drv.context, "THEN", func);
    BasicBlock *ElseBB = BasicBlock::Create(*drv.context, "ELSE");
    BasicBlock *MergeBB = BasicBlock::Create(*drv.context, "MERGE");

    drv.builder->CreateCondBr(checkCond, ThenBB, ElseBB);
    drv.builder->SetInsertPoint(ThenBB);

    Value *thenCode = branchTrue->codegen(drv);

    if(!thenCode)
      return nullptr;

    drv.builder->CreateBr(MergeBB);

    ThenBB = drv.builder->GetInsertBlock();   //serve per non sminchiare gli if innestati ovvero blocchi in più

    func->getBasicBlockList().push_back(ElseBB);

    drv.builder->SetInsertPoint(ElseBB);

    Value *elseCode = branchFalse->codegen(drv);

    if(!elseCode)
      return nullptr;

    drv.builder->CreateBr(MergeBB);

    ElseBB = drv.builder->GetInsertBlock();

    func->getBasicBlockList().push_back(MergeBB);

    drv.builder->SetInsertPoint(MergeBB);

    PHINode *phiInstr = drv.builder->CreatePHI(Type::getDoubleTy(*drv.context), 2, " PHI ");

    phiInstr->addIncoming(thenCode, ThenBB);
    phiInstr->addIncoming(elseCode, ElseBB);

    return phiInstr;
  }
};


/************************* Estensione 2 **************************/
UnaryExprAST::UnaryExprAST(char operand, ExprAST *espressione) :
  operand(std::move(operand)),
  espressione(std::move(espressione))
  {top = false;}

void UnaryExprAST::visit() {
  std::cout << "(" << operand << " ";
  espressione->visit();
  std::cout << ")";
};


Value *UnaryExprAST::codegen(driver &drv) {
  // Verifico che non sia un istruzione di tipo top
  if(gettop()) {
    return TopExpression(this, drv);

  } else {
    Value *checkCond = espressione->codegen(drv);

    if(!checkCond)
      return nullptr;

    switch (operand) {
    case '+' :
      return checkCond;
      break;
    
    case '-' :
      return drv.builder->CreateFSub(ConstantFP::get(Type::getDoubleTy(*drv.context), 0), checkCond, "negativeRegister");
      break;

    default:
      return LogErrorV("Operatore binario non supportato");
    }
  }
}

/************************* Estensione 3,(adattamento per estensione 4) **************************/
ForExprAST::ForExprAST(std::string id, ExprAST* init, ExprAST* exp, ExprAST* step, ExprAST* stmt) :
  id(std::move(id)),
  init(std::move(init)),
  exp(std::move(exp)),
  step(std::move(step)),
  stmt(std::move(stmt))
  {top = false;}

void ForExprAST::visit() {
  std::cout << "( FOR "+ id + " = ";
  init->visit();
  std::cout << ", ";
  exp->visit();
  std::cout << " , ";
  if(step)
    step->visit();
  else
    std::cout << "1";
  std::cout << " IN ";
  stmt->visit();
  std::cout << " END )";
};

Value *ForExprAST::codegen(driver &drv) {
  // Verifico che non sia un istruzione di tipo top
  if(gettop()) {
    return TopExpression(this, drv);

  } else {
    // Genero gli oggetti "alloca" e "TheFunction"
    Function *TheFunction = drv.builder->GetInsertBlock()->getParent();
    AllocaInst *Alloca = CreateEntryBlockAlloca(drv, TheFunction, id);

    // Codegen di StartVal, se uguale nullptr ritorno nullptr
    Value *StartVal = init->codegen(drv);
    if (!StartVal)
      return nullptr;
    
    // Faccio la "store"
    drv.builder->CreateStore(StartVal, Alloca);

    // GetInsertBlock restituisce il BB dove è stata inserita la Store
    BasicBlock *PreheaderBB = drv.builder->GetInsertBlock();
    
    // Basicblock creato per generare l'header, senza questo il for sarebbe un do-while fa lameno 1 iterazione senza controllare la condizione
    BasicBlock *HeaderBB = BasicBlock::Create(*drv.context, "HEADER", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(*drv.context, "LOOP", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*drv.context, "AFTERLOOP", TheFunction);

    // Genero un salto al blocco dell'header
    drv.builder->CreateBr(HeaderBB);
    
    // Imposto il punto di inserimento nel blocco Header per le prossime istruzioni
    drv.builder->SetInsertPoint(HeaderBB);

    // Creo il nodo PHI che restituirà valore 0 se non entrerà mai nel loop, se no restituirà il valore passato dal blocco LoopBB
    PHINode *Variable = drv.builder->CreatePHI(Type::getDoubleTy(*drv.context), 2, "PHI");

    // Definisco nel nodo PHI di ritornare 0 se non sono mai entrato nel loop e quindi arrivo dal PreheaderBB
    Variable->addIncoming(ConstantFP::get(*drv.context, APFloat(0.0)), PreheaderBB);

    // Se nella definizione della variabile del "for" vado a sovrascrivere una precedente dichiarazione esterna al
    // loop della stessa varibiale allora devo memorizzare il precedente valore, mettendo temporaneamente la vecchia definizione
    // nella tabella dei simboli (shadowed variable)

    // Es: int i = 10; ... for(i = 0, ...) { ... }
    AllocaInst *OldValue = drv.NamedValues[id]; // Salvo il valore della variabile esterna al loop in OldValue
    drv.NamedValues[id] = Alloca; // Sovrascrivo la tabella dei simboli, con variabile definita internamente al "for"

    // Codegen della condizione di terminazione, solito controllo
    Value *EndCond = exp->codegen(drv);
    if (!EndCond)
      return nullptr;

    // Come per l'if controllo se è double e lo gestisco
    if (EndCond->getType()->isDoubleTy())
      EndCond = drv.builder->CreateFCmpONE(EndCond, ConstantFP::get(*drv.context, APFloat(0.0)), "loopcond");
      
    // Genero una condizione di salto che dipende dalla codegen di End
    drv.builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // Definisco punto di inserimento delle istruzione nel blocco LoopBB
    drv.builder->SetInsertPoint(LoopBB);

    // codegen del corpo del ciclo
    Value *BodyValue = stmt->codegen(drv);
    if (!BodyValue)
      return nullptr;

    // Il codegen del body potrebbe aver creato altri blocchi o alterato il blocco su cui stiamo lavorando
    BasicBlock *bodyExitBB = drv.builder->GetInsertBlock();

    // Codegen dello Step
    Value *StepVal = nullptr;

    if (step) {
      StepVal = step->codegen(drv);

      if (!StepVal)
        return nullptr;

    } else
      // Se non viene definito inserisco 1
      StepVal = ConstantFP::get(*drv.context, APFloat(1.0));

    // Ora devo gestire l'operazione di step
    // Carico tramite load il valore attuale dell'indice
    Value *CurVar = drv.builder->CreateLoad(Alloca->getAllocatedType(), Alloca, "loadCurrentVar");

    // Vi effettuo un operazione di somma con il valore dello Step
    Value *NextVar = drv.builder->CreateFAdd(CurVar, StepVal, "nextVarCalcolated");

    // Salvo il risultato tramite una store
    drv.builder->CreateStore(NextVar, Alloca);

    // Genero un salto per tornare all'header e verificare nuovamente la condizione
    drv.builder->CreateBr(HeaderBB);

    // Aggiunge una nuova voce al nodo PHI per il backedge.
    Variable->addIncoming(BodyValue, bodyExitBB);

    // Imposto come punto di inserimento il blocco AfterBB, così le prossime istruzioni vengono messe in tale blocco
    drv.builder->SetInsertPoint(AfterBB);
    
    // Ripristina il valore della variabile esterna al loop che potrebbe essere stata sovrascritta
    if (OldValue)
      drv.NamedValues[id] = OldValue;

    else
      drv.NamedValues.erase(id);

    // Ritorno il nodo PHI
    return Variable;
  }
}

/************************* Estensione 4 **************************/
VarExprAST::VarExprAST(std::vector<std::pair<std::string, ExprAST*>> varNames, ExprAST* exp) :
  varNames(std::move(varNames)),
  exp(std::move(exp))
  {top = false;}

void VarExprAST::visit() {
  std::cout<<"( ";
  for (unsigned i = 0, e = varNames.size(); i != e; ++i)
  {
    const std::string &VarName = varNames[i].first;
    ExprAST *Init = varNames[i].second;
    std::cout<<VarName;
    if(Init)
    {
      std::cout<<" = ";
      Init->visit();
    } 
    std::cout<<" , ";
  }
  std::cout<<" IN ";
  exp->visit();
  std::cout << " END )";
};

Value *VarExprAST::codegen(driver &drv) {
  // Verifico che non sia un istruzione di tipo top
  if(gettop()) {
    return TopExpression(this, drv);

  } else {
    std::vector<AllocaInst *> OldBindings;

    Function *TheFunction = drv.builder->GetInsertBlock()->getParent();

    // Registra tutte le variabili ed emette il loro inizializzatore.
    for (unsigned i = 0, e = varNames.size(); i != e; ++i) {
      const std::string &varName = varNames[i].first;
      ExprAST *Init = varNames[i].second;

      // var a = 1 in
      //    var a = a in ...  <-- si riferisce alla 'a' esterna.
      Value *InitVal;
      if (Init) {
        InitVal = Init->codegen(drv);

        if (!InitVal)
          return nullptr;

      } else  // Se non è specificato il valore iniziale, lo inizializzo a 0
          InitVal = ConstantFP::get(*drv.context, APFloat(0.0));

      AllocaInst *Alloca = CreateEntryBlockAlloca(drv, TheFunction, varName);
      drv.builder->CreateStore(InitVal, Alloca);

      // Tengo traccia degli attuali valori delle variabili inizializzate esternamente al loop
      OldBindings.push_back(drv.NamedValues[varName]);

      // Sovrascrivo nella tabella dei simboli il valore della variabile inizializzata internamente al loop
      drv.NamedValues[varName] = Alloca;
    }

    // Genero il codice ora che ho tutte le variabili inizializzate
    Value *expVal = exp->codegen(drv);

    if (!expVal)
      return nullptr;

    // Aggiorno la tabella dei simboli con i valori delle variabili esterne al loop
    for (unsigned i = 0, e = varNames.size(); i != e; ++i)
      drv.NamedValues[varNames[i].first] = OldBindings[i];

    // Ritorno il valore dell'espressione
    return expVal;
  }
}

/************************* Estensione 5 **************************/
WhileExprAST::WhileExprAST(ExprAST* end, ExprAST* exp) :
  end(std::move(end)),
  exp(std::move(exp))
  {top=false;}

void WhileExprAST::visit() {
  std::cout << "( WHILE ";
  end->visit();
  std::cout << " IN ";
  exp->visit();
  std::cout << " END )";
};

Value *WhileExprAST::codegen(driver& drv) {
  if(gettop()) {
    return TopExpression(this, drv);
  } else {
    Function *TheFunction = drv.builder->GetInsertBlock()->getParent();

    // Crea i nuovi BB per la gestione del while
    BasicBlock *PreheaderBB = drv.builder->GetInsertBlock();
    BasicBlock *WhileBB = BasicBlock::Create(*drv.context, "loop", TheFunction);
    BasicBlock *HeaderBB = BasicBlock::Create(*drv.context, "header", TheFunction); // Basicblock creato per generare l'header, senza questo il for sarebbe un do-while fa lameno 1 iterazione senza controllare la condizione
    BasicBlock *AfterBB = BasicBlock::Create(*drv.context, "afterloop", TheFunction);
    // Inserisco un istruzione di salto all'header del while
    drv.builder->CreateBr(HeaderBB);

    // Inizia l'inserimento nel Basic Block
    drv.builder->SetInsertPoint(HeaderBB); // InsertPoint definisce il punto in cui stiamo aggiungendo le istruzioni
    
     // Definisco nel nodo PHI di ritornare 0 se non sono mai entrato nel loop e quindi arrivo dal PreheaderBB
    PHINode *Variable = drv.builder->CreatePHI(Type::getDoubleTy(*drv.context), 2, "phi");
    
    Variable->addIncoming(ConstantFP::get(*drv.context, APFloat(0.0)), PreheaderBB);

    // Genero la condizione di terminazione
    Value *EndCond = end->codegen(drv);
    if (!EndCond)
      return nullptr;

    // Come per l'if controllo se è double e lo gestisco
    if (EndCond->getType()->isDoubleTy())
      EndCond = drv.builder->CreateFCmpONE(EndCond, ConstantFP::get(*drv.context, APFloat(0.0)), "loopcond");

    // Genero una condizione di salto che dipende dalla codegen di End
    drv.builder->CreateCondBr(EndCond, WhileBB, AfterBB);

    // Inizia l'inserimento nel BasicBlock
    // InsertPoint definisce il punto in cui stiamo aggiungendo le istruzioni
    drv.builder->SetInsertPoint(WhileBB);
    
    // Codegen della condizione di terminazione, solito controllo
    Value *BodyValue = exp->codegen(drv);
    if (!BodyValue)
      return nullptr;

    drv.builder->CreateBr(HeaderBB);

    // Aggiunge una nuova voce al nodo PHI per il backedge.
    Variable->addIncoming(BodyValue, WhileBB);

     // Imposto come punto di inserimento il blocco AfterBB, così le prossime istruzioni vengono messe in tale blocco
    drv.builder->SetInsertPoint(AfterBB);

    return Variable;
  }
}