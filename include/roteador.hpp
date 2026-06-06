#pragma once

#include "links.hpp"
#include "mensagens.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

/**
 * @brief Fila de mensagens thread-safe exclusiva de cada roteador.
 * Utiliza o modelo de produtores (vizinhos/simulador) e consumidor (a própria thread do roteador).
 */
class FilaMensagens
{
    private:
	std::deque<Mensagem> fila;  // Buffer circular estável para manipulação FIFO nas duas pontas
	std::mutex mtx;             // Mecanismo de exclusão mútua para evitar Race Conditions na fila
	std::condition_variable cv; // Sinalizador reativo (campainha) para sincronização de threads

    public:
	FilaMensagens() = default;
	~FilaMensagens() = default;
	FilaMensagens(const FilaMensagens &) = delete;
	FilaMensagens &operator=(const FilaMensagens &) = delete;
	FilaMensagens(FilaMensagens &&) = delete;
	FilaMensagens &operator=(FilaMensagens &&) = delete;

	void push_back(const Mensagem &msg);  // Tráfego regular assíncrono do protocolo OSPF
	void push_front(const Mensagem &msg); // Tráfego emergencial prioritário out-of-band
	Mensagem pop();                       // Método bloqueante de extração e tratamento de mensagens
};

/**
 * @brief Representa um Roteador OSPF autônomo rodando em sua própria thread.
 */
class Roteador
{
    private:
	std::string router_id; // Identificador único do nó na rede física

	// Link State Database: Tabela topológica global contendo o mapa de links de toda a rede
	std::unordered_map<std::string, std::vector<Link>> lsdb;

	/**
	 * @brief Map de Timers de Adjacência
	 * Guarda o instante exato do relógio físico da CPU (steady_clock) do último HELLO de cada vizinho.
	 */
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> timers_vizinho;

	// Tabela de Encaminhamento gerada pelo cálculo do algoritmo de caminhos mínimos
	std::unordered_map<std::string, std::string> tabela_roteamento;

	std::shared_ptr<FilaMensagens> inbox; // Ponteiro compartilhado para a caixa de entrada
	std::thread thread_trabalho;          // Handle da linha de execução independente no processador

	bool ativo = false; // Flag de estado para controle lógico do laço concorrente

    public:
	// Construtor e Destrutor da Unidade Autônoma
	Roteador(std::string router_id);
	~Roteador() = default;
	Roteador(const Roteador &) = delete;
	Roteador &operator=(const Roteador &) = delete;
	Roteador(Roteador &&) = delete;
	Roteador &operator=(Roteador &&) = delete;

	// Ações de Infraestrutura, Concorrência e Ciclo de Vida
	void ciclo_vida();                                                                // Rotina executada pela thread contendo o loop de eventos
	void processar_mensagem(Mensagem msg);                                            // Despachante interno baseado no TipoMensagem
	void enviar_hello();                                                              // Dispara anúncios HELLO locais para manter adjacências
	void executar_dijkstra();                                                         // Processa a inteligência do protocolo (calcula rotas e Next-Hops)
	void suicidio();                                                                  // Rotina de auto-encerramento disparada pelo recebimento da Poison Pill
	void adicionar_link(const Link &novo_link);                                       // Configuração direta de interfaces locais
	void adicionar_link_na_lsdb(const std::string &id_origem, const Link &novo_link); // Inserção genérica na LSDB
	void ligar_roteador();                                                            // Instancia a thread e starta o ciclo concorrente
	void desligar_roteador();                                                         // Sinaliza parada, injeta pílula e performa o join seguro

	// Getters públicos e constantes com garantia thread-safe para monitoramento externo
	std::string get_router_id() const;
	bool is_ativo() const;
	std::shared_ptr<FilaMensagens> get_inbox() const;
	std::unordered_map<std::string, std::string> get_tabela_roteamento() const;
};
