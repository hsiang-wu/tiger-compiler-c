#include "semant.h"

struct expty {Tr_exp exp; Ty_ty ty;};
struct expty expTy(Tr_exp exp, Ty_ty ty) {
  struct expty e; e.exp=exp; e.ty=ty; return e;
}

//#define dprintf(...) printf(...)
#define dprintf(...)

/*Lab4: Your implementation of lab4*/
F_fragList SEM_transProg(A_exp exp) 
{
  S_table tenv = E_base_tenv();
  S_table venv = E_base_venv();
  Tr_level out = Tr_outermost();
  struct expty et = transExp(venv, tenv, exp, 
      Tr_newLevel(out, S_Symbol("main"), NULL), 
      //out,
      NULL);
  Tr_procEntryExit(out, et.exp);
  F_fragList resl = Tr_getFragList();
  dprintf("resl : %p %p\n", resl, resl->tail);
  return resl;
}

Ty_ty actual_ty(Ty_ty ty)
{
  assert(ty);
  Ty_ty tmpty;
  while (ty->kind == Ty_name) {
    dprintf("ty...%s\n", S_name(ty->u.name.sym));
    tmpty = ty->u.name.ty;
    if (tmpty)
      ty = tmpty;
    else
      return NULL; // XXX ugly code
  }
  dprintf("return ty %d\n", ty->kind-Ty_record);
  return ty;
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList afl)
{
  if (!afl) return NULL; /* ugly special case */
  Ty_tyList ttl = checked_malloc(sizeof(*ttl));
  Ty_tyList ttl_hd = ttl;
  Ty_tyList last = NULL;
  while (afl != NULL) {
    ttl->tail = checked_malloc(sizeof(*(ttl->tail)));

    if (afl->head->typ == S_Symbol("int"))
      ttl->head = Ty_Int();
    else if (afl->head->typ == S_Symbol("string"))
      ttl->head = Ty_String();
    else
      ttl->head = S_look(tenv, afl->head->typ);

    dprintf("formal list:%s %x\n", S_name(afl->head->typ), ttl);
    if (ttl->head == NULL) {
      EM_error(afl->head->pos, " undefined type: %s", S_name(afl->head->typ));
    }
    last = ttl;
    ttl = ttl->tail;
    afl = afl->tail;
  }
  if (last) last->tail = NULL; // have to clear it.
  return ttl_hd;
}

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level level, Tr_exp brk) 
{
    switch (a->kind) {
        case A_varExp:
          {
            dprintf("evaluate var exp\n");
            return transVar(venv, tenv, a->u.var, level, brk);
          }
        case A_nilExp:
          {
            dprintf(" nil!\n");
            return expTy(Tr_nilExp(), Ty_Nil());
          }
        case A_intExp:
          {
            dprintf(" int!\n");
            return expTy(Tr_constVar(a->u.intt), Ty_Int());
          }
        case A_stringExp:
          {
            dprintf(" string!\n");
            return expTy(Tr_stringVar(a->u.stringg), Ty_String());
          }
        case A_callExp:
          {
            E_enventry et = S_look(venv, a->u.call.func);
            printf(" call! look in %p [%s:%p]\n", venv, S_name(a->u.call.func), et);
            if (et == NULL || et->kind != E_funEntry) {
              EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
              return expTy(Tr_constVar(0), Ty_Nil());
            }
            Ty_tyList formals = et->u.fun.formals;
            A_expList el = a->u.call.args;
            Tr_accessList paraList = NULL;
            Tr_expList telr = NULL;  // !!! reversed
            struct expty tmp;
            while (formals != NULL) {
              //dprintf("paramter: %x %x\n", formals, el);
              if (el == NULL) {
                EM_error(a->pos, "Too few arguments.");
                return expTy(Tr_constVar(0), Ty_Nil());
              } else {
                tmp = transExp(venv, tenv, el->head, level, brk);
                if (formals->head != tmp.ty)
                  EM_error(a->pos, "para type mismatch");
              }
              formals = formals->tail;
              el = el->tail;
              telr = Tr_ExpList(tmp.exp, telr);
            }
            if (el != NULL) {
                EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));
                return expTy(Tr_constVar(0), et->u.fun.result);
            } else {
                return expTy(Tr_callExp(et->u.fun.label, 
                                          /* call tr_formals will get the formals */
                                          et->u.fun.level,
                                          level, telr), et->u.fun.result);
            }
          }
        case A_opExp: 
          {
            dprintf(" op!\n");
            A_oper oper = a->u.op.oper;
            struct expty left = transExp(venv, tenv, a->u.op.left, level, brk);
            struct expty right = transExp(venv, tenv, a->u.op.right, level, brk);
            if (oper==A_plusOp || oper==A_minusOp
                || oper==A_timesOp || oper==A_divideOp) {
                if (left.ty->kind != Ty_int) {
                    goto err_int;
                }
                if (right.ty->kind != Ty_int) {
                    goto err_int;
                }
                // arithmatic return.
                return expTy(Tr_arithExp(oper, left.exp, right.exp), Ty_Int());
            } else if (left.ty->kind != right.ty->kind && left.ty != Ty_Nil() && right.ty != Ty_Nil()) {
              EM_error(a->u.op.left->pos, "same type required");
              goto err_ret;
            } else if (oper!=A_eqOp && oper!=A_neqOp) { // lt, le, gt, ge
              if (left.ty->kind != Ty_int || right.ty->kind != Ty_int) {
                goto err_comp;
              }
            } else if (!(left.ty->kind == Ty_int || left.ty->kind == Ty_array || left.ty->kind == Ty_record || left.ty->kind == Ty_string)) {
                if (right.ty != Ty_Nil()) {
                  goto err_comp;
                }
            }

            // otherwise. comp return
            return expTy(Tr_compExp(oper, left.exp, right.exp), Ty_Int());
err_int:
            EM_error(a->u.op.left->pos, "integer required");
            goto err_ret;
err_comp:
            EM_error(a->u.op.right->pos, "can't compare!");
            goto err_ret;
err_ret:
            return expTy(Tr_constVar(0), Ty_Int());
          } 
        case A_recordExp:
          {
            dprintf(" record!\n");
            Ty_ty recordty = S_look(tenv, a->u.record.typ);
            if (NULL == recordty) {
              EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
              return expTy(Tr_constVar(0), Ty_Record(NULL));
            }
            if (recordty->kind != Ty_record) {
              EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
            }
            Tr_expList el_reversed = checkRecordFieldList(tenv, venv, a->u.record.fields, recordty->u.record, level, brk);
            return expTy(Tr_recordExp(el_reversed), recordty);
          }
        case A_seqExp:
          {
            dprintf(" seq!\n");
            A_expList el = a->u.seq;

            if (el == NULL)
              return expTy(Tr_nilExp(), Ty_Void());
            for (;el->tail != NULL; el=el->tail) {
              transExp(venv, tenv, el->head, level, brk);
            }
            return transExp(venv, tenv, el->head, level, brk);
          }
        case A_assignExp:
          {
            dprintf("assgin!\n");
            struct expty var = transVar(venv, tenv, a->u.assign.var, level, brk);
            struct expty exp = transExp(venv, tenv, a->u.assign.exp, level, brk);
            if (var.ty != exp.ty)
              EM_error(a->pos, "unmatched assign exp %d %d", var.ty->kind, exp.ty->kind);
            return expTy(Tr_assignExp(var.exp, exp.exp), Ty_Void());
          }
        case A_ifExp:
          {
            dprintf(" if!\n");
            struct expty cond;
            if ((cond = transExp(venv, tenv, a->u.iff.test, level, brk)).ty != Ty_Int())
              EM_error(a->pos, "if test must produce int");
            dprintf("  then!\n");
            struct expty thenet = transExp(venv, tenv, a->u.iff.then, level, brk);
            if (a->u.iff.elsee != NULL) { // if-then-else
              dprintf("  else!\n");
              struct expty elseet = transExp(venv, tenv, a->u.iff.elsee, level, brk);
              // if-then: no return value. if-then-else: can have return value
              if (elseet.ty != thenet.ty) {
                // XXX: shoud this semantic contain Ty_Void()?
                if (!((elseet.ty == Ty_Nil() || elseet.ty == Ty_Void()) || (thenet.ty == Ty_Nil() || thenet.ty == Ty_Void())))
                EM_error(a->pos, "then exp and else exp type mismatch");
              }
              return expTy(Tr_ifExp(cond.exp, thenet.exp, elseet.exp), thenet.ty);
            } else if (thenet.ty != Ty_Void()) { // if-then
                EM_error(a->pos, "if-then exp's body must produce no value");
            }
            return expTy(Tr_ifExp(cond.exp, thenet.exp, Tr_nilExp()), thenet.ty);
          }
        case A_whileExp:
          {
            // not in test?
            dprintf("while!\n");
            struct expty test, body; 
            if ((test = transExp(venv, tenv, a->u.whilee.test, level, brk)).ty != Ty_Int())
              EM_error(a->pos, "while test must produce int");
            Tr_exp done = Tr_doneExp();
            body = transExp(venv, tenv, a->u.whilee.body, level, 
                    done/* tell break where to jump */);
            if (body.ty != Ty_Void() && body.ty != Ty_Nil())
              EM_error(a->pos, "while body must produce no value");
            return expTy(Tr_whileExp(test.exp, body.exp, done), Ty_Void());
          }
        case A_forExp:
          {
            dprintf("for! %s\n", S_name(a->u.forr.var));

            // rewrite for grammer in while.
            // XXX: malfunctioned when limit = maxint
            // or when the name "_limit" is used in program
            A_exp translated = A_LetExp(0, 
                A_DecList(A_VarDec(0, a->u.forr.var, S_Symbol("int"), a->u.forr.lo),
                  A_DecList(A_VarDec(0, S_Symbol("_limit"), 
                      S_Symbol("int"), a->u.forr.hi), NULL)),
                A_WhileExp(0, A_OpExp(0, A_ltOp, 
                      A_VarExp(0, A_SimpleVar(0, a->u.forr.var)),
                      A_VarExp(0, A_SimpleVar(0, S_Symbol("_limit")))),
                    A_SeqExp(0, A_ExpList(a->u.forr.body,
                        A_ExpList(
                          A_OpExp(0, A_plusOp, 
                            A_VarExp(0, A_SimpleVar(0, a->u.forr.var)), 
                            A_IntExp(0, 1)), 
                          /* make while produce no value */
                          A_ExpList(A_NilExp(0), NULL))))));
            return transExp(venv, tenv, translated, level, brk);


            // a implementation using forexp. but need to implement Tr_forExp()
            // which is not done yet.
            // S_beginScope(venv);
            // E_enventry eentry = E_VarEntry(Tr_allocLocal(level, TRUE), Ty_Int());
            // S_enter(venv, a->u.forr.var, eentry);
            // struct expty lo = transExp(venv, tenv, a->u.forr.lo, level, brk);
            // struct expty hi = transExp(venv, tenv, a->u.forr.hi, level, brk);
            // if (lo.ty != Ty_Int() || hi.ty != Ty_Int())
            //   EM_error(a->pos, "for exp's range type is not integer\n\
            //       loop variable can't be assigned");

            // Tr_exp done = Tr_doneExp();
            // struct expty body = transExp(venv, tenv, a->u.forr.body, level, 
            //     done/*change it to done so break know where to jump!*/);
            // S_endScope(venv);
            // return expTy(Tr_forExp(lo.exp, hi.exp, body.exp, done), Ty_Void());
          }
        case A_breakExp:
          { 
            // not tested in lab5 grademe.
            dprintf("break!\n");
            if (!brk) return expTy(Tr_constVar(0), Ty_Void());
            return expTy(Tr_breakExp(brk), Ty_Void());
          }
        case A_letExp:
          {
            dprintf("let!\n");
            A_decList d;
            S_beginScope(venv);
            S_beginScope(tenv);

            Tr_expList l = NULL, head = NULL; Tr_exp e;
            for (d = a->u.let.decs; d; d=d->tail) {
              // append in a reversed order. but don't worry. check return.
              e = transDec(venv, tenv, d->head, level, brk);
              l = Tr_ExpList(e, l);
            }
            dprintf("let body\n");

            struct expty et = transExp(venv, tenv, a->u.let.body, level, brk);
            l = Tr_ExpList(et.exp, l);

            dprintf("end let body\n");
            S_endScope(tenv);
            S_endScope(venv);
            // seqExp reverse the list again. so everything is good.
            return expTy(Tr_seqExp(l), et.ty);
          }
        case A_arrayExp:
          {
            dprintf("array!\n");
            struct expty init, size;
            Ty_ty typ = S_look(tenv, a->u.array.typ);
            dprintf("looking %s:%d\n", S_name(a->u.array.typ), typ->kind);
            init = transExp(venv, tenv, a->u.array.init, level, brk);
            if (typ->u.array->kind == Ty_name)
              typ->u.array = S_look(tenv, typ->u.array->u.name.sym);
            if (init.ty != typ->u.array) {
              EM_error(a->pos, "type mismatch");
              return expTy(Tr_constVar(0), Ty_Array(typ));
            }
            size = transExp(venv, tenv, a->u.array.size, level, brk);
            if (size.ty != Ty_Int())
              EM_error(a->pos, "Array length must be int.");
            return expTy(Tr_arrayExp(init.exp, size.exp), typ);
          }
        default:
                      ;
	}
    assert(0);
}

struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level level, Tr_exp brk) 
{
  switch(v->kind) {
    case A_simpleVar:
      {
        E_enventry x = S_look(venv, v->u.simple);
        dprintf("simple var! %s\n", S_name(v->u.simple));
        if (x && x->kind==E_varEntry) {
          Tr_access acc = x->u.var.access;
          return expTy(Tr_simpleVar(acc, level), actual_ty(x->u.var.ty));
        } else {
          EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
          return expTy(Tr_constVar(0), Ty_Int());
        }
      }
    case A_fieldVar:
      {
        dprintf("fieldvar!\n");
        struct expty et = transVar(venv, tenv, v->u.field.var, level, brk);
        if (et.ty->kind==Ty_record) {
          Ty_fieldList fl;
          int i = -1, n = 0;
          Ty_ty tmpty;
          for (fl = et.ty->u.record; fl; fl=fl->tail, n++) {
            if (fl->head->name == v->u.field.sym) {
              i = n; // because we want a reversed position.
              tmpty = fl->head->ty;
              if (tmpty->kind == Ty_name) {
                // XXX: buggy when type name is "shaded" by new definition. 
                // Use this to pass test42.tig
                tmpty = S_look(tenv, tmpty->u.name.sym);
              }
              break;
            }
          }
          if (i >= 0)
            return expTy(Tr_fieldVar(et.exp, n - i), tmpty);
          else 
            EM_error(v->pos, "field %s doesn't exist", S_name(v->u.field.sym));
          return expTy(Tr_constVar(0), Ty_Int());
        } else {
          EM_error(v->pos, "not a record type");
          return expTy(Tr_constVar(0), Ty_Int());
        }
      }
    case A_subscriptVar:
      {
        dprintf("subscript!\n");
        struct expty et = transVar(venv, tenv, v->u.subscript.var, level, brk);
        struct expty etint = transExp(venv, tenv, v->u.subscript.exp, level, brk);
        if (etint.ty != Ty_Int()) {
          EM_error(v->pos, "subscript err");
          return expTy(Tr_constVar(0), Ty_Int());
        }

        if (actual_ty(et.ty)->kind==Ty_array) {
          Ty_ty arr = et.ty->u.array;
          return expTy(Tr_subscriptVar(et.exp, etint.exp), arr);
        } else {
          EM_error(v->pos, "array type required");
          return expTy(Tr_constVar(0), Ty_Int());
        }
      }
    defalut:
      ;
  }
  assert(0);
}

Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level level, Tr_exp brk)
{
  switch(d->kind) {
    case A_varDec:
      {
        dprintf("dec:vardec:%p %p\n", d->u.var.var, d->u.var.typ);
        struct expty e = transExp(venv, tenv, d->u.var.init, level, brk);
        E_enventry eentry = E_VarEntry(Tr_allocLocal(level, TRUE), e.ty);
        
        Ty_ty ty;
        if (d->u.var.typ == NULL) {
          if (e.ty == Ty_Nil())
            EM_error(d->pos, "init should not be nil without type specified");
        } else if ((ty = S_look(tenv, d->u.var.typ)) != e.ty) {
          if (e.ty != Ty_Nil())
            EM_error(d->pos, "type mismatch");
        }

        dprintf("put to %p [%p:%p]\n", venv, d->u.var.var, eentry);
        S_enter(venv, d->u.var.var, eentry);
        return Tr_assignExp(Tr_simpleVar(eentry->u.var.access, level), e.exp);
      }
    case A_typeDec: 
      { 
        dprintf("dec:typedec:%p\n", d->u.type);
        Ty_tyList tl = nametyList(tenv, d->u.type);
        checkTypeDec(tenv, tl);
        return Tr_constVar(0);
      }
    case A_functionDec:
      {
        dprintf("dec:funcdec:%p\n", d->u.type);
        A_fundec f;
        A_fundecList fl;
        // pre scan the headers
        for (fl=d->u.function; fl; fl=fl->tail) {
          f = fl->head;
          Ty_ty resultTy;
          if (f->result != NULL) {
            // no new type will be declared in funcdec. It's safe
            resultTy = S_look(tenv, f->result);
          } else {
            resultTy = Ty_Void();
          }

          Ty_tyList formalTys = makeFormalTyList(tenv, f->params);
          E_enventry entry = E_FunEntry(level, f->name, formalTys, resultTy);
          S_enter(venv, f->name, entry);
          dprintf("\tfuncdec:label [%s]\n", S_name(f->name));
        }

        // then the body
        S_table dup_func = S_empty();
        for (fl=d->u.function; fl; fl=fl->tail) {
          f=fl->head;
          if (NULL != S_look(dup_func, f->name))
              EM_error(f->pos, "two functions have the same name %s\n", 
                        S_name(f->name));
          S_enter(dup_func, f->name, (void *) 1);

          //Ty_tyList formalTys = fml->head;
          E_enventry entry = S_look(venv, f->name);

          Ty_tyList formalTys = entry->u.fun.formals;
          Ty_ty resultTy = entry->u.fun.result;

          // now enter a new level
          U_boolList escapeList = NULL;
          for (Ty_tyList t=formalTys; t; t=t->tail) {
            escapeList = U_BoolList(TRUE, escapeList); // always escape, XXX: escapeList is reversed compared with tylist
          }
          Tr_level newlev = Tr_newLevel(level, entry->u.fun.label, escapeList);

          S_beginScope(venv);
          {
            A_fieldList l; Ty_tyList t;
            Tr_accessList accs = AccTail(Tr_formals(newlev)); // head is static link
            // XXX: reconsider the reverse/sequence of parameters.
            for (l=f->params, t=formalTys; l; l=l->tail, t=t->tail, accs=AccTail(accs)) {
              dprintf(" %s : enter param\n", S_name(entry->u.fun.label));
              // we don't have to alloc local here. because the parameters
              // are alloced(copied) by caller. So the accesses are already
              // there. In newlev's accessList.
              S_enter(venv, l->head->name, 
                  E_VarEntry(AccHead(accs), t->head));
            }
          }
          dprintf("evaluate body exp!\n");
          struct expty et = transExp(venv, tenv, f->body, newlev, brk); // not same as book
          if (resultTy == Ty_Void()) {
            if (et.ty != Ty_Void())
              EM_error(f->pos, "procedure returns value\n");
          } else {
            if (et.ty == Ty_Void())
              EM_error(f->pos, "Must return value! %s\n", S_name(fl->head->name));
            else if (et.ty != resultTy)
              EM_error(f->pos, "Return value not match! %s\n", S_name(fl->head->name));
          }

          dprintf("procentryexit:[%s]\n", S_name(f->name));
          Tr_procEntryExit(newlev, et.exp);
          S_endScope(venv);
        }
        dprintf("\tfuncdec:3 finish%p\n", fl);
        return Tr_constVar(0);
      }

  }
}

Ty_ty transTy(S_table tenv, A_ty a)
{
  Ty_ty t = checked_malloc(sizeof(*t));
  //assert(a != NULL); // might cause trouble in recursive dec!
  if (a == NULL) {
        dprintf("type void\n");
    return Ty_Void();
  }
  switch (a->kind) {
    case A_nameTy:
      {
        if (a->u.name == S_Symbol("int")) {
        dprintf("type int\n");
          return Ty_Int();
        } else if (a->u.name == S_Symbol("string")) {
        dprintf("type string\n");
          return Ty_String();
        } else {
        dprintf("type name\n");
          //Ty_ty ty = S_look(tenv, a->u.name);
          return Ty_Name(a->u.name, NULL);// set to null first
        }
      }
    case A_recordTy:
      {
        dprintf("type record\n");
        A_fieldList afl = a->u.record;
        return Ty_Record(makeTyFieldList(tenv, afl));
      }
    case A_arrayTy:
      {
        Ty_ty ty = transTy(tenv, A_NameTy(0, a->u.array));
        dprintf("type array of %s\n", S_name(a->u.array));
        return Ty_Array(ty);
      }
    default:
      dprintf("a:%p, %x\n", a, a->kind);
      assert(0);
      return Ty_Void();
  }
}

Ty_fieldList makeTyFieldList(S_table tenv, A_fieldList afl)
{
  if (afl == NULL) return NULL;
  A_ty aty = A_NameTy(afl->head->pos, afl->head->typ);
  Ty_field head = Ty_Field(afl->head->name, transTy(tenv, aty));
  return Ty_FieldList(head, makeTyFieldList(tenv, afl->tail));
}

Tr_expList checkRecordFieldList(S_table tenv, S_table venv, A_efieldList efl, Ty_fieldList fl, Tr_level level, Tr_exp brk)
{
  Tr_expList el = NULL;
  for (; fl; fl = fl->tail, efl=efl->tail) {
    if (efl == NULL) {
      EM_error(0, "Too few fields! %s\n", S_name(fl->head->name));
      return NULL;
    }

    struct expty exp = transExp(venv, tenv, efl->head->exp, level, brk);
    Ty_ty ty1 = actual_ty(exp.ty);
    Ty_ty ty2 = actual_ty(fl->head->ty);
    if (ty1 == NULL) ty1 = S_look(tenv, exp.ty->u.name.sym);
    if (ty2 == NULL) ty2 = S_look(tenv, fl->head->ty->u.name.sym);
    if (ty1 != ty2 && ty1->kind != Ty_nil /* allow nil */) {
      EM_error(efl->head->exp->pos, "Unmatched ty:%s, %s\n", ty1, ty2);
      return NULL;
    }
    el = Tr_ExpList(exp.exp, el);
  }

  return el;
  //checkRecordFieldList(tenv, venv, efl->tail, fl->tail); 
}

Ty_tyList nametyList(S_table tenv, A_nametyList nl)
{
  if (nl == NULL) return NULL;
  Ty_ty ty = transTy(tenv, nl->head->ty);  // for myint, get ty_int
  dprintf("dec:nametylist:%p, %d\n", nl, ty->kind);
  //Ty_ty alias;
  //if ((alias = S_look(tenv, nl->head->name)) != NULL) {
  //  //EM_error(nl->head->ty->pos, "two types have the same name");
  //}
  Ty_ty head;
  if (ty->kind == Ty_int || ty->kind == Ty_string || ty->kind == Ty_name) {
    dprintf("creating a name type, %s->%d\n", S_name(nl->head->name), ty->kind);
    head = Ty_Name(nl->head->name, ty);
  } else {
    head = ty;
  }
  S_enter(tenv, nl->head->name, ty); // now if you S_look arrtype, it'll be a type
  dprintf("senter:%s = %d\n", S_name(nl->head->name), ty->kind);
  return Ty_TyList(head, nametyList(tenv, nl->tail));
}

//void checkTypeDec(S_table tenv, A_nametyList nl)
void checkTypeDec(S_table tenv, Ty_tyList tl)
{

  //if (tl == NULL) return;
  //S_symbol s = nl->head->name;
  //Ty_ty ty = S_look(tenv, s), tmpty;
  Ty_ty ty, tmpty;
  S_table dup_check = S_empty();
  for (; tl; tl = tl->tail) {
    ty = tl->head;
    dprintf("check type.. %d\n", ty->kind);
  //Ty_ty alias;
  //if ((alias = S_look(tenv, nl->head->name)) != NULL) {
  //  //EM_error(nl->head->ty->pos, "two types have the same name");
  //}

    if (ty->kind == Ty_record) {
      checkRecord(tenv, ty->u.record);
    }
    
    //if (ty->kind == Ty_name) {
    //  ty->u.name.ty = S_look(tenv, ty->u.name.sym);
    //}

    if (ty->kind == Ty_name) {
      if (S_look(dup_check, ty->u.name.sym)) {
        EM_error(0, "two types have the same name");
      }
      S_enter(dup_check, ty->u.name.sym, (void *)1);

      S_table cycle_check = S_empty();
      Ty_ty nty = ty;
      while (nty && nty->kind == Ty_name) {
        dprintf("check cycle.. %s\n", S_name(nty->u.name.sym));
        if (S_look(cycle_check, nty->u.name.sym)) { // != NULL. entered before
          EM_error(0, "illegal type cycle");
          return;
        }
        S_enter(cycle_check, nty->u.name.sym, (void *)1);
        tmpty = S_look(tenv, nty->u.name.sym);
        //tmpty = nty->u.name.ty;
        //nty->u.name.ty = tmpty;
        dprintf("now jump namety %s to %d\n", S_name(nty->u.name.sym), tmpty->kind);
        nty = tmpty;
        dprintf("nty%p nty->kind == Ty_name?:%d\n", nty, (nty != NULL) ? (nty->kind == Ty_name) : 0);
      }

      //if (ty) {
      //  checkTypeDec(tenv, tl->tail); // move to next
      //  return;
      //} else {
      if (nty == NULL) {
        EM_error(0, "%%s contains undeclared type"/*, S_name(s)*/);
      } else {
        ty->u.name.ty = nty;
        S_enter(tenv, ty->u.name.sym, nty);
      }
    }
  }
  dprintf("end of check type\n");

  return;
}

void checkRecord(S_table tenv, Ty_fieldList fl)
{

  if (fl == NULL) return;
    dprintf("!!!!!!\n\n\n\n");
  Ty_ty ty = fl->head->ty, tmpty;
  S_symbol s;

  if (ty->kind == Ty_array) {
    dprintf("!!!!!!%d\n", ty->u.array->kind);
  }

  S_table cycle_check = S_empty();
  while (ty && ty->kind == Ty_name) {
    if (S_look(cycle_check, ty->u.name.sym)) { // != NULL. entered before
      EM_error(0, "illegal type cycle");
      return;
    }
    s = ty->u.name.sym;
    S_enter(cycle_check, s, (void *)1);
    tmpty = S_look(tenv, s);
    ty->u.name.ty = tmpty;
    ty = tmpty;
  }

  if (ty) {
    checkRecord(tenv, fl->tail); // move to next
    return;
  } else {
    EM_error(0, " undefined type %s", S_name(s));
  }
  return;
}
