#pragma once

#include <string>
#include <vector>
#include "links.hpp"
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include "mensagens.hpp"
#include <chrono>

/**
 * @brief Fila de mensagens thread-safe exclusiva de cada roteador.
 * Utiliza o modelo de produtores (vizinhos) e consumidor (a thread do roteador).
 */
class FilaMensagens
{
      private:
            std::deque<Mensagem> fila;
            std::mutex mtx; // Tranca de memória para evitar Race Conditions
            std::condition_variable cv; // Campainha para acordar a thread hibernando

      public:
            FilaMensagens();
            ~FilaMensagens();

            void push_back(Mensagem msg);  // Tráfego padrão (HELLO, LSU)
            void push_front(Mensagem msg); // Tráfego prioritário (POISON_PILL)
            Mensagem pop(); // Retira a mensagem mais antiga (bloqueia se a fila estiver vazia)
};

/**
 * @brief Representa um Roteador OSPF autônomo rodando em sua própria thread.
 */
class Roteador
{
      private:
            std::string router_id;

            // Link State Database: Mapeia o RouterID de qualquer roteador da rede para a lista de links dele
            std::unordered_map<std::string, std::vector<Link>> lsdb;

            /**
             * @brief O que é o 'std::chrono::steady_clock::time_point'?
             * * Pense nisso como uma "foto" tirada de um cronômetro de corrida perfeito:
             * - chrono: É a biblioteca de tempo moderna e segura do C++.
             * - steady_clock: É o cronômetro físico da CPU. Diferente do relógio do sistema
             * (que pode ser adiantado/atrasado pelo usuário ou pela internet via NTP),
             * esse cronômetro NUNCA volta no tempo e ignora fusos horários.
             * - time_point: O instante matemático exato em que a "foto" foi tirada.
             * * @var timers_vizinho
             * Guarda o instante exato do último pacote HELLO recebido de cada vizinho.
            */
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> timers_vizinho;

            std::shared_ptr<FilaMensagens> inbox;
            std::thread thread_trabalho;
            bool ativo;

      public:
            // Construtor: Inicializa a classe com o ID.
            Roteador(std::string router_id);
            ~Roteador();

            // Ações do Ciclo de Vida
            void iniciar(); // Dispara a thread_trabalho e entra no loop infinito
            void processar_mensagem(Mensagem msg); // Avalia o enum e direciona a lógica
            void enviar_hello(); // Faz o flood multicast para as portas conectadas
            void executar_dijkstra(); // Roda o SPF (Shortest Path First) atualizando as rotas
            void suicidio(); // Quebra o loop da thread graciosamente ao ler a Poison Pill
            void adicionar_link(const Link& novo_link); // Insere um link mapeado na tabela de vizinhos (LSDB)

            // Getters seguros para leitura externa
            std::string get_router_id() const;
            bool is_ativo() const;
            std::shared_ptr<FilaMensagens> get_inbox() const;
};
