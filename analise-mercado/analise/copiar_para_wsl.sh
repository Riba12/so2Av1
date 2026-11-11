#!/bin/bash
# Copia a analise + os dados para dentro do Linux do WSL (~/so2/analise-mercado) e compila.
# Serve para rodar FORA do container. Os arquivos ficam no disco do Linux: ler a pasta do Windows
# (/mnt/c/...) e lento e atrapalharia a medicao.
#
# Uso, no PowerShell (na pasta so2/):
#   wsl -d Ubuntu-24.04 -u root -- bash analise-mercado/analise/copiar_para_wsl.sh
# Depois:
#   wsl -d Ubuntu-24.04 -u root
#   cd ~/so2/analise-mercado
#   ./analise/build/threads 4
#   bash analise/benchmark.sh 16 5

set -e

ORIGEM=$(cd "$(dirname "$0")/.." && pwd)   # analise-mercado/ na pasta do Windows
DESTINO=$HOME/so2/analise-mercado

rm -rf $DESTINO/analise $DESTINO/dados
mkdir -p $DESTINO/analise $DESTINO/dados
cp $ORIGEM/analise/*.cpp $ORIGEM/analise/*.h $ORIGEM/analise/*.sh $ORIGEM/analise/Makefile $DESTINO/analise/
cp -r $ORIGEM/dados/processados $ORIGEM/dados/eventos $DESTINO/dados/

cd $DESTINO
make -C analise
echo "Pronto: $DESTINO"
