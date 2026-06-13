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

class Simulador;

// ─── FilaMensagens ────────────────────────────────────────────────────────────

// Fila thread-safe com modelo produtor/consumidor exclusiva de cada roteador
class FilaMensagens
{
    private:
	std::deque<Mensagem> fila;
	std::mutex mtx;
	std::condition_variable cv;

    public:
	FilaMensagens() = default;
	~FilaMensagens() = default;
	FilaMensagens(const FilaMensagens &) = delete;
	FilaMensagens &operator=(const FilaMensagens &) = delete;
	FilaMensagens(FilaMensagens &&) = delete;
	FilaMensagens &operator=(FilaMensagens &&) = delete;

	void push_back(const Mensagem &msg);                      // Tráfego regular do protocolo OSPF
	void push_front(const Mensagem &msg);                     // Tráfego prioritário out-of-band (Poison Pill)
	Mensagem pop();                                           // Bloqueante sem timeout
	Mensagem wait_pop(std::chrono::milliseconds timeout);     // Bloqueante com timeout; retorna TIMEOUT se expirar

	[[nodiscard]]
	bool empty() const { return fila.empty(); }
};

// ─── Roteador ─────────────────────────────────────────────────────────────────

// Roteador OSPF autônomo; cada instância roda em sua própria thread
class Roteador
{
    private:
	std::string router_id;

	std::unordered_map<std::string, std::vector<Link>> lsdb;
	std::unordered_map<std::string, std::string> tabela_roteamento;
	std::unordered_map<std::string, EstadoVizinho> tabela_estados;

	// Instante do último HELLO recebido de cada vizinho; base do cálculo do dead timer
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> timers_vizinho;
	std::chrono::seconds dead_interval{5};

	std::shared_ptr<FilaMensagens> inbox;
	std::thread thread_trabalho;
	Simulador *simulador;
	bool ativo = false;

    public:
	Roteador(std::string router_id, Simulador *simulador);
	~Roteador() = default;
	Roteador(const Roteador &) = delete;
	Roteador &operator=(const Roteador &) = delete;
	Roteador(Roteador &&) = delete;
	Roteador &operator=(Roteador &&) = delete;

	// Configuração
	void adicionar_link(const Link &novo_link);
	void adicionar_link_na_lsdb(const std::string &id_origem, const Link &novo_link);

	// Ciclo de vida
	void ligar_roteador();
	void desligar_roteador();
	void ressucitar();

	// Loop principal
	void ciclo_vida();

	// Protocolo OSPF
	void enviar_hello();
	void processar_mensagem(Mensagem msg);
	void inundar_lsu_msg(const Mensagem &msg);
	void inundar_lsu();

	// Dijkstra
	void executar_dijkstra();

	// Utilitários
	void log_evento(int severidade, const std::string &tag, const std::string &mensagem) const;

	// Getters
	std::string get_router_id() const;
	bool is_ativo() const;
	std::shared_ptr<FilaMensagens> get_inbox() const;
	std::unordered_map<std::string, std::string> get_tabela_roteamento() const;
};
