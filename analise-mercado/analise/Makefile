# Analise de mercado (sequencial, threads e processos). De dentro de analise-mercado/:
#   make -C analise
#   ./analise/build/sequencial
#   ./analise/build/threads 4
#   ./analise/build/processos 4

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
COMUM    = comum.cpp relatorio.cpp relatorio_md.cpp comum.h

all: build/sequencial build/threads build/processos

build/sequencial: sequencial.cpp $(COMUM)
	mkdir -p build
	$(CXX) $(CXXFLAGS) sequencial.cpp comum.cpp relatorio.cpp relatorio_md.cpp -o build/sequencial

# -pthread: liga a biblioteca de threads (pthread_create, pthread_join)
build/threads: threads.cpp $(COMUM)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread threads.cpp comum.cpp relatorio.cpp relatorio_md.cpp -o build/threads

build/processos: processos.cpp $(COMUM)
	mkdir -p build
	$(CXX) $(CXXFLAGS) processos.cpp comum.cpp relatorio.cpp relatorio_md.cpp -o build/processos

clean:
	rm -rf build

.PHONY: all clean
