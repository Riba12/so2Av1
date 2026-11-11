FROM ubuntu:24.04

# build-essential: gcc, g++ e make (tudo que a analise precisa para compilar)
RUN apt-get update && \
    apt-get install -y build-essential git vim && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Os executaveis da analise viram comandos: "sequencial" em vez de "./analise/build/sequencial"
ENV PATH="/app/analise-mercado/analise/build:${PATH}"

CMD ["/bin/bash"]
