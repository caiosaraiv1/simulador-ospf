#include "roteador.hpp"
#include "simulador.hpp"

#include <chrono>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <thread>
#include <iomanip>

static std::mutex mtx_log;

// Construtor: Aloca a infraestrutura básica e a caixa de entrada protegida
Roteador::Roteador(std::string router_id, Simulador* simulador)
    : router_id(std::move(router_id)),
      inbox(std::make_shared<FilaMensagens>()),
      simulador(simulador)
{
}

// Interface local: Adiciona conexões físicas do próprio nó na LSDB
void Roteador::adicionar_link(const Link &novo_link) { this->adicionar_link_na_lsdb(this->router_id, novo_link); }

// Interface global: Atualiza a LSDB com links recebidos de terceiros via LSU
void Roteador::adicionar_link_na_lsdb(const std::string &id_origem, const Link &novo_link) { this->lsdb[id_origem].push_back(novo_link); }

// Algoritmo Dijkstra (Shortest Path First) para cálculo de caminhos mínimos
void Roteador::executar_dijkstra()
{
	std::unordered_map<std::string, int> distancias;
	std::unordered_map<std::string, std::string> anteriores;
	std::set<std::string> nao_visitados;

	// Inicializa o grafo: define custos como infinito e popula o set de não visitados
	for (const auto &roteador : this->lsdb)
	{
		distancias[roteador.first] = std::numeric_limits<int>::max();
		nao_visitados.insert(roteador.first);
	}
	// O nó de origem tem custo zero para ele mesmo
	distancias[this->router_id] = 0;

	while (!nao_visitados.empty())
	{
		// Passo 1: Busca o nó não visitado com a menor distância acumulada
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

		// Proteção: Se a menor distância for infinito, os nós restantes estão isolados
		if (menor_distancia == std::numeric_limits<int>::max())
			break;

		// Proteção: Evita crash caso o nó seja destino conhecido mas não possua links de saída na LSDB
		if (!this->lsdb.contains(no_atual))
		{
			nao_visitados.erase(no_atual);
			continue;
		}

		// Passo 2: Relaxamento de arestas para os vizinhos do nó atual
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

	// Passo 3: Engenharia reversa no mapa de predecessores para extrair o Next-Hop
	this->tabela_roteamento.clear();
	for (auto &par : anteriores)
	{
		std::string destino = par.first;
		std::string atual = destino;

		// Rastreia o caminho de volta até encontrar o vizinho direto conectado à origem
		while (anteriores.contains(atual) && anteriores[atual] != this->router_id)
			atual = anteriores[atual];

		this->tabela_roteamento[destino] = atual;
	}
}

// Chave de Ignição: Ativa o sinalizador e dispara a execução paralela
void Roteador::ligar_roteador()
{
	this->ativo = true;
	// Instancia a thread apontando para a rotina de execução passando o ponteiro do objeto
	this->thread_trabalho = std::thread(&Roteador::ciclo_vida, this);
}

// Graceful Shutdown: Encerra o loop concorrente de forma limpa e reativa
void Roteador::desligar_roteador()
{
	this->ativo = false;

	// Modela o padrão Out-of-Band (Poison Pill) para destravar o wait da fila
	Mensagem pilula;
	pilula.tipo = TipoMensagem::POISON_PILL;

	// Injeta na frente da fila com prioridade máxima
	this->inbox->push_front(pilula);

	// Bloqueia a thread principal até que a linha de execução paralela finalize
	if (thread_trabalho.joinable())
		thread_trabalho.join();
      log_evento(2, "SHUTDOWN", "OSPF process terminating. Cleaning up neighbors and links.");
}

// Loop de Ciclo de Vida: Executado de forma autônoma pela std::thread
void Roteador::ciclo_vida()
{
      auto ultimo_hello = std::chrono::steady_clock::now();
	while (this->ativo)
	{
            auto agora = std::chrono::steady_clock::now();

            auto tempo_passado = agora - ultimo_hello;
            auto timeout_dinamico = std::chrono::seconds(2) - tempo_passado;

            if (timeout_dinamico < std::chrono::seconds(0)) timeout_dinamico = std::chrono::seconds(0);

		// Consumo bloqueante: Hiberna se a fila estiver vazia sem gastar ciclos de CPU
            auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout_dinamico);
		Mensagem msg = this->inbox->wait_pop(timeout_ms);

		// Intercepta a Pílula de Veneno para quebrar o laço imediatamente
		if (msg.tipo == TipoMensagem::POISON_PILL) break;
            if (msg.tipo != TipoMensagem::TIMEOUT) this->processar_mensagem(msg);

            agora = std::chrono::steady_clock::now();
            if (agora - ultimo_hello >= std::chrono::seconds(2))
            {
                  this->enviar_hello();
                  ultimo_hello = agora;
            }
	}
}

void Roteador::enviar_hello()
{
      std::vector<std::string> ids_vizinhos;
      for (const auto& vizinho : this->tabela_estados)
            if (vizinho.second == EstadoVizinho::INIT || vizinho.second == EstadoVizinho::FULL)
                  ids_vizinhos.push_back(vizinho.first);

      Mensagem msg;
      msg.tipo = TipoMensagem::HELLO;
      msg.remetente_id = this->router_id;
      msg.vizinhos_conhecidos = std::move(ids_vizinhos);

      std::vector<Link> links = this->lsdb[this->router_id];
      for (const auto& link : links)
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

            auto it = std::find(msg.vizinhos_conhecidos.begin(), msg.vizinhos_conhecidos.end(), this->router_id);
            if (it != msg.vizinhos_conhecidos.end()) this->tabela_estados[msg.remetente_id] = EstadoVizinho::FULL;
            else this->tabela_estados[msg.remetente_id] = EstadoVizinho::INIT;

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

void Roteador::inundar_lsu_msg(const Mensagem& msg)
{
      log_evento(6, "LSU_TX", "Flooding local LSDB updates to adjacent neighbors");
      for (const auto& vizinho : this->tabela_estados)
      {
            if (vizinho.second == EstadoVizinho::FULL && vizinho.first != msg.remetente_id)
                  this->simulador->enviar_mensagem_global(vizinho.first, msg);
      }
}

void Roteador::inundar_lsu()
{
      log_evento(6, "LSU_TX", "Flooding local LSDB updates to adjacent neighbors");
      Mensagem msg;
      msg.tipo = TipoMensagem::LSU;
      msg.remetente_id = this->router_id;
      msg.payload = this->lsdb[this->router_id];

      for (const auto& vizinho : this->tabela_estados)
      {
            if (vizinho.second == EstadoVizinho::FULL)
                  this->simulador->enviar_mensagem_global(vizinho.first, msg);
      }
}

void Roteador::log_evento(int severidade, const std::string& tag, const std::string& mensagem) const
{
      if (this->simulador == nullptr) return;

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

      std::scoped_lock lock(mtx_log);
      std::cout << oss.str();
}

// Getters thread-safe para leitura de estado externa
std::string Roteador::get_router_id() const { return this->router_id; }

bool Roteador::is_ativo() const { return this->ativo; }

std::shared_ptr<FilaMensagens> Roteador::get_inbox() const { return this->inbox; }

std::unordered_map<std::string, std::string> Roteador::get_tabela_roteamento() const { return this->tabela_roteamento; }

// Produtor Padrão: Insere mensagens no fim da fila (HELLOs, LSUs)
void FilaMensagens::push_back(const Mensagem &msg)
{
	// Garante exclusão mútua escopada desfazendo o lock no fim do método
	std::scoped_lock lock(this->mtx);
	this->fila.push_back(msg);
	// Notifica e acorda a thread consumidora que estava em wait
	this->cv.notify_one();
}

// Produtor Prioritário: Insere comandos de controle na frente do deque (Poison Pill)
void FilaMensagens::push_front(const Mensagem &msg)
{
	std::scoped_lock lock(this->mtx);
	this->fila.push_front(msg);
	this->cv.notify_one();
}

// Consumidor Seguro: Lê e extrai dados da fila controlando a hibernação da thread
Mensagem FilaMensagens::pop()
{
	// unique_lock: Obrigatório para uso com condition_variable devido à flexibilidade de trancar/destrancar
	std::unique_lock<std::mutex> lock(this->mtx);

	// Bloqueia a execução se o lambda retornar false. Libera o mutex atonicamente enquanto dorme
	this->cv.wait(lock, [this]
	              { return !this->fila.empty(); });

	// Resgata o dado de forma segura antes de removê-lo da memória (pop_front em C++ é void)
	Mensagem front = this->fila.front();
	this->fila.pop_front();
	return front;
}

Mensagem FilaMensagens::wait_pop(std::chrono::milliseconds timeout)
{
      std::unique_lock<std::mutex> lock(this->mtx);

      if ( this->cv.wait_for(lock, timeout, [this] { return !this->fila.empty(); }) )
      {
            Mensagem front = this->fila.front();
	      this->fila.pop_front();
	      return front;
      }

      Mensagem msg;
      msg.tipo = TipoMensagem::TIMEOUT;

      return msg;
}
