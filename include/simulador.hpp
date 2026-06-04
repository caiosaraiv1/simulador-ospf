#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "roteador.hpp"

/**
 * @brief Classe Gerente responsável por orquestrar o ecossistema da rede.
 * Não possui threads próprias; seu papel é instanciar os roteadores,
 * montar a topologia física (os cabos) e monitorar eventos globais.
 */
class Simulador
{
      private:
            /** * @brief O mapa global da rede.
             * Chave: Router ID (ex: "1.1.1.1").
             * Valor: Ponteiro para o objeto Roteador instanciado em memória.
             */
            std::unordered_map<std::string, std::shared_ptr<Roteador>> rede;

      public:
            Simulador();
            ~Simulador();

            /**
             * @brief Lê o arquivo topologia.json e constrói os Roteadores e Links.
             * @param caminho_json Caminho relativo para o arquivo de dados.
             */
            void carregar_topologia(std::string caminho_json);

            /**
             * @brief Percorre o mapa da rede chamando o iniciar() de cada Roteador.
             * Dá o disparo inicial para que as threads comecem a rodar.
             */
            void iniciar_simulacao();

            /**
             * @brief Ferramenta de Chaos Engineering.
             * Busca um roteador específico e injeta uma POISON_PILL direto na FilaMensagens dele,
             * forçando a queda do nó para testar a convergência do resto da rede.
             * @param router_id O ID do roteador que será abatido.
             */
            void injetar_caos(std::string router_id);
};
