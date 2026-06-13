#include "roteador.hpp"

#include "logger.hpp"
#include "simulador.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ranges>
#include <set>
#include <string>
#include <thread>

// ─── Roteador ────────────────────────────────────────────────────────────────

Roteador::Roteador(std::string router_id, Simulador *simulador)
    : router_id(std::move(router_id)),
      inbox(std::make_shared<FilaMensagens>()),
      simulador(simulador)
{
}

// Registra um link do próprio nó na LSDB
void Roteador::adicionar_link(const Link &novo_link) { this->adicionar_link_na_lsdb(this->router_id, novo_link); }

// Registra um link de nó externo na LSDB (chamado ao processar LSUs de vizinhos)
void Roteador::adicionar_link_na_lsdb(const std::string &id_origem, const Link &novo_link) { this->lsdb[id_origem].push_back(novo_link); }

// ─── Controle de Ciclo de Vida ────────────────────────────────────────────────

void Roteador::ligar_roteador()
{
	this->ativo = true;
	this->thread_trabalho = std::thread(&Roteador::ciclo_vida, this);
}

void Roteador::desligar_roteador()
{
	this->ativo = false;

	// Padrão Poison Pill: destranca o wait_pop da thread de trabalho sem busy-wait
	Mensagem pilula;
	pilula.tipo = TipoMensagem::POISON_PILL;
	this->inbox->push_front(pilula);

	if (thread_trabalho.joinable())
		thread_trabalho.join();

	log_evento(2, "SHUTDOWN", "OSPF process terminating. Cleaning up neighbors and links.");
}

void Roteador::ressucitar()
{
	if (this->thread_trabalho.joinable())
		this->thread_trabalho.join();

	this->tabela_estados.clear();
	this->timers_vizinho.clear();

	// Preserva os links locais e descarta o estado de roteamento aprendido
	std::vector<Link> links = std::move(this->lsdb[this->router_id]);
	this->lsdb.clear();
	this->lsdb[this->router_id] = std::move(links);

	while (!this->inbox->empty())
		auto msg = this->inbox->pop();

	this->ativo = true;
	this->thread_trabalho = std::thread(&Roteador::ciclo_vida, this);
}

// ─── Loop Principal ───────────────────────────────────────────────────────────

// Executado pela thread de trabalho: gerencia timers, mensagens e envio de HELLOs
void Roteador::ciclo_vida()
{
	auto ultimo_hello = std::chrono::steady_clock::now();
	while (this->ativo)
	{
		auto agora = std::chrono::steady_clock::now();

		// Calcula o tempo restante até o próximo HELLO para usar como timeout do wait_pop
		auto tempo_passado = agora - ultimo_hello;
		auto timeout_dinamico = std::chrono::seconds(2) - tempo_passado;

		if (timeout_dinamico < std::chrono::seconds(0))
			timeout_dinamico = std::chrono::seconds(0);

		auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout_dinamico);
		Mensagem msg = this->inbox->wait_pop(timeout_ms);

		if (msg.tipo == TipoMensagem::POISON_PILL)
			break;
		if (msg.tipo != TipoMensagem::TIMEOUT)
			this->processar_mensagem(msg);

		// Verifica dead timer de cada vizinho ativo
		for (auto &estado : this->tabela_estados)
		{
			if (estado.second == EstadoVizinho::INIT || estado.second == EstadoVizinho::FULL)
			{
				auto ultimo_hello = this->timers_vizinho[estado.first];
				auto delta_tempo = std::chrono::duration_cast<std::chrono::seconds>(agora - ultimo_hello);
				if (delta_tempo > this->dead_interval)
				{
					estado.second = EstadoVizinho::DOWN;
					this->log_evento(3, "NBR_DOWN", "Dead timer expired for neighbor " + estado.first + ". State changed to DOWN");
					inundar_lsu();
				}
			}
		}

		agora = std::chrono::steady_clock::now();
		if (agora - ultimo_hello >= std::chrono::seconds(2))
		{
			this->enviar_hello();
			ultimo_hello = agora;
		}
	}
}

// ─── Protocolo OSPF ──────────────────────────────────────────────────────────

// Envia HELLO para todos os vizinhos com link direto, anunciando os vizinhos já conhecidos
void Roteador::enviar_hello()
{
	std::vector<std::string> ids_vizinhos;
	for (const auto &vizinho : this->tabela_estados)
		if (vizinho.second == EstadoVizinho::INIT || vizinho.second == EstadoVizinho::FULL)
			ids_vizinhos.push_back(vizinho.first);

	Mensagem msg;
	msg.tipo = TipoMensagem::HELLO;
	msg.remetente_id = this->router_id;
	msg.vizinhos_conhecidos = std::move(ids_vizinhos);

	std::vector<Link> links = this->lsdb[this->router_id];
	for (const auto &link : links)
		this->simulador->enviar_mensagem_global(link.destino_id, msg);
}

void Roteador::processar_mensagem(Mensagem msg)
{
	if (msg.tipo == TipoMensagem::HELLO)
	{
		EstadoVizinho estado_inicial = this->tabela_estados.contains(msg.remetente_id)
		                                     ? this->tabela_estados[msg.remetente_id]
		                                     : EstadoVizinho::DOWN;

		if (!this->tabela_estados.contains(msg.remetente_id))
		{
			this->tabela_estados[msg.remetente_id] = EstadoVizinho::INIT;
			log_evento(6, "NBR_CHG", "Neighbor " + msg.remetente_id + " state changed to INIT");
		}

		// Transição INIT → FULL: o vizinho nos listou no próprio HELLO (bidirecionalidade confirmada)
		auto it = std::ranges::find(msg.vizinhos_conhecidos, this->router_id);
		if (it != msg.vizinhos_conhecidos.end())
			this->tabela_estados[msg.remetente_id] = EstadoVizinho::FULL;
		else
			this->tabela_estados[msg.remetente_id] = EstadoVizinho::INIT;

		auto agora = std::chrono::steady_clock::now();
		this->timers_vizinho[msg.remetente_id] = agora;

		if (estado_inicial == EstadoVizinho::INIT && this->tabela_estados[msg.remetente_id] == EstadoVizinho::FULL)
		{
			log_evento(5, "NHB_CHG", "Neighbor " + msg.remetente_id + " state changed to FULL");
			inundar_lsu();
		}
	}
	else if (msg.tipo == TipoMensagem::LSU)
	{
		if (this->lsdb[msg.remetente_id] != msg.payload)
		{
			log_evento(4, "LSDB_UPD", "Received newer LSDB update from " + msg.remetente_id + ". Updating local database.");
			this->lsdb[msg.remetente_id] = msg.payload;
			log_evento(6, "SPF_RUN", "LSDB changed. Triggering shortest path first (Dijkstra) calculation.");
			inundar_lsu_msg(msg);
			executar_dijkstra();
		}
	}
}

// Propaga o LSU recebido para todos os vizinhos FULL, exceto o remetente original
void Roteador::inundar_lsu_msg(const Mensagem &msg)
{
	log_evento(6, "LSU_TX", "Flooding local LSDB updates to adjacent neighbors");
	for (const auto &vizinho : this->tabela_estados)
	{
		if (vizinho.second == EstadoVizinho::FULL && vizinho.first != msg.remetente_id)
			this->simulador->enviar_mensagem_global(vizinho.first, msg);
	}
}

// Gera e propaga um LSU com os links locais para todos os vizinhos FULL
void Roteador::inundar_lsu()
{
	log_evento(6, "LSU_TX", "Flooding local LSDB updates to adjacent neighbors");
	Mensagem msg;
	msg.tipo = TipoMensagem::LSU;
	msg.remetente_id = this->router_id;
	msg.payload = this->lsdb[this->router_id];

	for (const auto &vizinho : this->tabela_estados)
	{
		if (vizinho.second == EstadoVizinho::FULL)
			this->simulador->enviar_mensagem_global(vizinho.first, msg);
	}
}

// ─── Dijkstra (SPF) ───────────────────────────────────────────────────────────

void Roteador::executar_dijkstra()
{
	std::unordered_map<std::string, int> distancias;
	std::unordered_map<std::string, std::string> anteriores;
	std::set<std::string> nao_visitados;

	for (const auto &roteador : this->lsdb)
	{
		distancias[roteador.first] = std::numeric_limits<int>::max();
		nao_visitados.insert(roteador.first);
	}
	distancias[this->router_id] = 0;

	while (!nao_visitados.empty())
	{
		// Seleciona o nó não visitado com menor distância acumulada
		std::string no_atual;
		int menor_distancia = std::numeric_limits<int>::max();
		for (const auto &roteador : nao_visitados)
		{
			if (distancias[roteador] < menor_distancia)
			{
				menor_distancia = distancias[roteador];
				no_atual = roteador;
			}
		}

		// Nós restantes estão isolados (inalcançáveis)
		if (menor_distancia == std::numeric_limits<int>::max())
			break;

		// Nó destino conhecido na topologia mas sem links de saída na LSDB
		if (!this->lsdb.contains(no_atual))
		{
			nao_visitados.erase(no_atual);
			continue;
		}

		// Relaxamento de arestas
		std::vector<Link> links = this->lsdb[no_atual];
		int custo_acumulado = 0;
		for (auto &link : links)
		{
			custo_acumulado = distancias[no_atual] + link.custo;
			if (custo_acumulado < distancias[link.destino_id])
			{
				distancias[link.destino_id] = custo_acumulado;
				anteriores[link.destino_id] = no_atual;
			}
		}
		nao_visitados.erase(no_atual);
	}

	// Reconstrói a tabela de roteamento extraindo o Next-Hop de cada caminho mínimo
	this->tabela_roteamento.clear();
	for (auto &par : anteriores)
	{
		std::string destino = par.first;
		std::string atual = destino;

		// Caminha de volta no mapa de predecessores até encontrar o vizinho direto
		while (anteriores.contains(atual) && anteriores[atual] != this->router_id)
			atual = anteriores[atual];

		this->tabela_roteamento[destino] = atual;
	}
}

// ─── Utilitários ─────────────────────────────────────────────────────────────

void Roteador::log_evento(int severidade, const std::string &tag, const std::string &mensagem) const
{
	if (this->simulador == nullptr)
		return;

	int tempo_segundos = this->simulador->get_tempo_simulacao();

	int horas = tempo_segundos / 3600;
	int minutos = (tempo_segundos % 3600) / 60;
	int segundos = tempo_segundos % 60;

	std::ostringstream oss;
	oss << "["
	    << std::setw(2) << std::setfill('0') << horas << ":"
	    << std::setw(2) << std::setfill('0') << minutos << ":"
	    << std::setw(2) << std::setfill('0') << segundos
	    << "] "
	    << this->router_id << " %%01OSPF/"
	    << severidade << "/" << tag << ": "
	    << mensagem << "\n";

	Logger::imprimir(oss.str());
}

// ─── Getters ─────────────────────────────────────────────────────────────────

std::string Roteador::get_router_id() const { return this->router_id; }

bool Roteador::is_ativo() const { return this->ativo; }

std::shared_ptr<FilaMensagens> Roteador::get_inbox() const { return this->inbox; }

std::unordered_map<std::string, std::string> Roteador::get_tabela_roteamento() const { return this->tabela_roteamento; }

// ─── FilaMensagens ────────────────────────────────────────────────────────────

// Insere no fim da fila (HELLOs, LSUs)
void FilaMensagens::push_back(const Mensagem &msg)
{
	std::scoped_lock lock(this->mtx);
	this->fila.push_back(msg);
	this->cv.notify_one();
}

// Insere no início da fila com prioridade máxima (Poison Pill)
void FilaMensagens::push_front(const Mensagem &msg)
{
	std::scoped_lock lock(this->mtx);
	this->fila.push_front(msg);
	this->cv.notify_one();
}

// Bloqueia a thread até haver mensagem disponível
Mensagem FilaMensagens::pop()
{
	// unique_lock é obrigatório para uso com condition_variable
	std::unique_lock<std::mutex> lock(this->mtx);
	this->cv.wait(lock, [this] { return !this->fila.empty(); });

	// Resgata antes de remover (pop_front em C++ é void)
	Mensagem front = this->fila.front();
	this->fila.pop_front();
	return front;
}

// Bloqueia até mensagem disponível ou expiração do timeout; retorna TIMEOUT se não houver mensagem
Mensagem FilaMensagens::wait_pop(std::chrono::milliseconds timeout)
{
	std::unique_lock<std::mutex> lock(this->mtx);

	if (this->cv.wait_for(lock, timeout, [this] { return !this->fila.empty(); }))
	{
		Mensagem front = this->fila.front();
		this->fila.pop_front();
		return front;
	}

	Mensagem msg;
	msg.tipo = TipoMensagem::TIMEOUT;

	return msg;
}
