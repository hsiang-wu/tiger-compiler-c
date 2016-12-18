a.out: main.o parse.o prabsyn.o y.tab.o lex.yy.o errormsg.o util.o table.o absyn.o symbol.o semant.o types.o env.o tree.o temp.o escape.o printtree.o frame.o translate.o canon.o codegen.o assem.o graph.o flowgraph.o liveness.o color.o regalloc.o
	clang -g main.o parse.o prabsyn.o y.tab.o lex.yy.o errormsg.o util.o table.o absyn.o symbol.o semant.o types.o env.o tree.o temp.o escape.o printtree.o frame.o translate.o canon.o codegen.o assem.o graph.o flowgraph.o liveness.o color.o regalloc.o

main.o: main.c 
	clang -g -c main.c

regalloc.o: regalloc.c regalloc.h
	clang -g -c regalloc.c

color.o: color.c color.h
	clang -g -c color.c

liveness.o: liveness.c liveness.h
	clang -g -c liveness.c

flowgraph.o: flowgraph.c flowgraph.h
	clang -g -c flowgraph.c

graph.o: graph.c graph.h
	clang -g -c graph.c

codegen.o: codegen.c codegen.h
	clang -g -c codegen.c

parse.o: parse.c errormsg.h util.h
	clang -g -c parse.c

assem.o: assem.c assem.h
	clang -g -c assem.c

canon.o: canon.c canon.h
	clang -g -c canon.c

translate.o: translate.c translate.h
	clang -g -c translate.c 

frame.o: x86frame.c frame.h
	clang -g -c x86frame.c -o frame.o

temp.o: temp.c temp.h
	clang -g -c temp.c

escape.o: escape.c escape.h
	clang -g -c escape.c

tree.o: tree.c tree.h 
	clang -g -c tree.c

printtree.o: printtree.c printtree.h
	clang -g -c printtree.c

prabsyn.o: prabsyn.c prabsyn.h
	clang -g -c prabsyn.c
semant.o: semant.c semant.h
	clang -g -c semant.c
env.o: env.c env.h
	clang -g -c env.c
types.o: types.c types.h
	clang -g -c types.c
y.tab.o: y.tab.c
	clang -g -c y.tab.c

y.tab.c: tiger.y
	yacc -dv tiger.y

y.tab.h: y.tab.c

errormsg.o: errormsg.c errormsg.h util.h
	clang -g -c errormsg.c

lex.yy.o: lex.yy.c symbol.h absyn.h y.tab.h errormsg.h util.h
	clang -g -c lex.yy.c

lex.yy.c: tiger.lex
	lex tiger.lex

util.o: util.c util.h
	clang -g -c util.c
table.o: table.c table.h
	clang -g -c table.c
absyn.o: absyn.h absyn.c
	clang -g -c absyn.c
symbol.o: symbol.c symbol.h
	clang -g -c symbol.c

handin:
	tar -czf id.name.tar.gz  absyn.[ch] errormsg.[ch] makefile gradeMe.sh parse.[ch] prabsyn.[ch] refs-5 symbol.[ch] table.[ch] testcases tiger.lex tiger.y util.[ch] env.[ch] semant.[ch] translate.[ch] *.h *.c
clean: 
	rm -f a.out *.o y.tab.c y.tab.h lex.yy.c y.output *~
