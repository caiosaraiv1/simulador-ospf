#include "simulador.hpp"

#include "logger.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

// ─── Configuração ─────────────────────────────────────────────────────────────

// Lê o arquivo JSON de topologia, instancia os roteadores e interconecta suas portas.
// Feito em dois passos para garantir que todos os ponteiros de inbox já existam antes do cabeamento.
void Simulador::carregar_topologia(const std::string &caminho_json)
{
	std::ifstream arquivo(caminho_json);
	json dados_json = json::parse(arquivo);

	// Passo 1: instancia todos os nós antes de passar qualquer cabo
	for (const auto &roteador : dados_json["routers"])
	{
		std::string id = roteador["id"].get<std::string>();
		this->rede[id] = std::make_shared<Roteador>(id, this);
	}

	arquivo.close();

	// Passo 2: configura os links com os ponteiros de inbox já válidos
	for (const auto &roteador : dados_json["routers"])
	{
		std::string id = roteador["id"].get<std::string>();

		for (const auto &link : roteador["links"])
		{
			std::string destino_id = link["destino_id"].get<std::string>();
			int custo = link["custo"].get<int>();

			Link l;
			l.destino_id = destino_id;
			l.custo = custo;
			l.inbox_vizinho = rede[destino_id]->get_inbox();

			rede[id]->adicionar_link(l);
		}
	}
}

void Simulador::registrar_roteador(const std::string &id, const std::shared_ptr<Roteador> &roteador)
{
	this->rede[id] = roteador;
}

// ─── Controle da Simulação ────────────────────────────────────────────────────

void Simulador::iniciar_simulacao()
{
	this->tempo_inicial = std::chrono::steady_clock::now();

	for (const auto &rtr : this->rede)
		rtr.second->ligar_roteador();

	this->thread_caos = std::thread(&Simulador::rotina_caos, this);
}

void Simulador::desligar_simulacao()
{
	for (const auto &rtr : this->rede)
	{
		if (rtr.second->is_ativo())
			rtr.second->desligar_roteador();
	}

	this->caos_rodando = false;
	if (this->thread_caos.joinable())
		this->thread_caos.join();
}

// ─── Injeção de Caos ──────────────────────────────────────────────────────────

// A cada ciclo: 40% de chance de derrubar um roteador ativo, 30% de chance de ressucitar um elegível.
// Um roteador só é elegível para ressureição após 30s offline.
void Simulador::rotina_caos()
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> roleta_porcentagem(1, 100);

	while (this->caos_rodando)
	{
		this->vetor_ativos.clear();
		std::this_thread::sleep_for(std::chrono::seconds(6));

		for (const auto &rtr : this->rede)
		{
			if (rtr.second->is_ativo())
				this->vetor_ativos.push_back(rtr.first);
		}

		// Evento de falha
		int dado_morte = roleta_porcentagem(gen);
		if (dado_morte <= 40 && this->vetor_ativos.size() > 1)
		{
			std::uniform_int_distribution<std::size_t> dist_ativos(0, this->vetor_ativos.size() - 1);

			std::size_t roteador_escolhido = dist_ativos(gen);
			std::string id_roteador = this->vetor_ativos[roteador_escolhido];
			const auto &roteador = this->rede[id_roteador];

			std::erase(this->vetor_ativos, id_roteador);

			RoteadorMorto roteador_morto;
			roteador_morto.id = id_roteador;
			roteador_morto.tempo_morte = std::chrono::steady_clock::now();
			this->vetor_mortos.push_back(roteador_morto);

			int tempo_segundos = this->get_tempo_simulacao();
			int horas = tempo_segundos / 3600;
			int minutos = (tempo_segundos % 3600) / 60;
			int segundos = tempo_segundos % 60;

			std::ostringstream oss;
			oss << "["
			    << std::setw(2) << std::setfill('0') << horas << ":"
			    << std::setw(2) << std::setfill('0') << minutos << ":"
			    << std::setw(2) << std::setfill('0') << segundos
			    << "] SIMULATOR %%CHAOS/1/KILL: Injecting failure. Powering off router " << id_roteador << "\n";
			Logger::imprimir(oss.str());

			roteador->desligar_roteador();
		}

		// Evento de recuperação
		int dado_vida = roleta_porcentagem(gen);
		if (dado_vida <= 30 && !this->vetor_mortos.empty())
		{
			// Calcula agora uma única vez, pois o loop termina rapidamente
			auto agora = std::chrono::steady_clock::now();
			std::vector<RoteadorMorto> elegiveis;

			for (const auto &rtr_morto : this->vetor_mortos)
			{
				auto delta_tempo = std::chrono::duration_cast<std::chrono::seconds>(agora - rtr_morto.tempo_morte);
				if (delta_tempo >= std::chrono::seconds(30))
					elegiveis.push_back(rtr_morto);
			}

			if (!elegiveis.empty())
			{
				std::uniform_int_distribution<std::size_t> dist_elegiveis(0, elegiveis.size() - 1);

				std::size_t roteador_escolhido = dist_elegiveis(gen);
				const RoteadorMorto &roteador_ressucitado = elegiveis[roteador_escolhido];
				std::string id_roteador = roteador_ressucitado.id;

				std::erase_if(this->vetor_mortos, [&](const RoteadorMorto &rm)
				              { return rm.id == id_roteador; });

				int tempo_segundos = this->get_tempo_simulacao();
				int horas = tempo_segundos / 3600;
				int minutos = (tempo_segundos % 3600) / 60;
				int segundos = tempo_segundos % 60;

				std::ostringstream oss;
				oss << "["
				    << std::setw(2) << std::setfill('0') << horas << ":"
				    << std::setw(2) << std::setfill('0') << minutos << ":"
				    << std::setw(2) << std::setfill('0') << segundos
				    << "] SIMULATOR %%CHAOS/5/RECOVERY: Maintenance finished. Powering ON router " << id_roteador << "\n";
				Logger::imprimir(oss.str());

				this->rede[id_roteador]->ressucitar();
			}
		}

		if (!this->caos_rodando)
			break;
	}
}

// ─── Utilitários ─────────────────────────────────────────────────────────────

int Simulador::get_tempo_simulacao() const
{
	auto agora = std::chrono::steady_clock::now();
	auto duracao = std::chrono::duration_cast<std::chrono::seconds>(agora - this->tempo_inicial);
	return static_cast<int>(duracao.count());
}

// Entrega a mensagem diretamente na inbox do destino, se ele estiver ativo
void Simulador::enviar_mensagem_global(const std::string &destino_id, const Mensagem &msg)
{
	if (this->rede.contains(destino_id) && this->rede[destino_id]->is_ativo())
		this->rede[destino_id]->get_inbox()->push_back(msg);
}
