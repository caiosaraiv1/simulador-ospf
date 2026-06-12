#include "simulador.hpp"
#include "logger.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

/**
 * @brief Lê o arquivo JSON de topologia, instancia os roteadores e interconecta suas portas.
 * O processo é feito em dois passos para garantir a integridade dos ponteiros de memória.
 */
void Simulador::carregar_topologia(const std::string &caminho_json)
{
	std::ifstream arquivo(caminho_json);
	json dados_json = json::parse(arquivo);

	// --- PASSO 1: Instanciação dos Roteadores ---
	// Garante que todos os nós do grafo existam na memória antes de passar os cabos
	for (const auto &roteador : dados_json["routers"])
	{
		std::string id = roteador["id"].get<std::string>();

		// Instancia o roteador com ponteiro inteligente (contador de referências seguro)
		auto novo_roteador = std::make_shared<Roteador>(id, this);

		// Registra o nó recém-criado no mapa global do simulador
		this->rede[id] = novo_roteador;
	}

	arquivo.close(); // Fecha o arquivo físico, pois os dados já estão todos parseados na RAM

	// --- PASSO 2: Cabeamento Lógico da Rede ---
	// Interconecta os roteadores espelhando os ponteiros das caixas de entrada (inbox).
	for (const auto &roteador : dados_json["routers"])
	{
		std::string id = roteador["id"].get<std::string>();

		for (const auto &link : roteador["links"])
		{
			std::string destino_id = link["destino_id"].get<std::string>();
			int custo = link["custo"].get<int>();

			// Monta a estrutura física da aresta
		      Link l;
			l.destino_id = destino_id;
			l.custo = custo;

			// Extrai o ponteiro da inbox do vizinho
			// e pluga diretamente na interface de saída do roteador de origem.
			auto inbox_destino = rede[destino_id]->get_inbox();
			l.inbox_vizinho = inbox_destino;

			// Entrega o cabo configurado para o roteador de origem catalogar na LSDB local
			rede[id]->adicionar_link(l);
		}
	}
}

void Simulador::iniciar_simulacao()
{
      this->tempo_inicial = std::chrono::steady_clock::now();

      for (const auto& rtr : this->rede)
      {
            rtr.second->ligar_roteador();
      }

      this->thread_caos = std::thread(&Simulador::rotina_caos, this);
}

void Simulador::desligar_simulacao()
{
      for (const auto& rtr : this->rede)
      {
            if (rtr.second->is_ativo())
                  rtr.second->desligar_roteador();
      }

      this->caos_rodando = false;
      if (this->thread_caos.joinable())
            this->thread_caos.join();
}

void Simulador::rotina_caos()
{
      while (this->caos_rodando)
      {
            std::this_thread::sleep_for(std::chrono::seconds(8));

            std::vector<std::string> roteadores_ativos;
            for (const auto& rtr : this->rede)
            {
                  if (rtr.second->is_ativo())
                        roteadores_ativos.push_back(rtr.first);
            }

            if (roteadores_ativos.size() == 0 || roteadores_ativos.size() == 1)
                  continue;

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, roteadores_ativos.size() - 1);

            int roteador_escolhido = dist(gen);
            std::string id_roteador = roteadores_ativos[roteador_escolhido];
            const auto& roteador = this->rede[id_roteador];

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

            if (!this->caos_rodando) break;
      }
}

int Simulador::get_tempo_simulacao() const
{
      auto agora = std::chrono::steady_clock::now();
      auto duracao = std::chrono::duration_cast<std::chrono::seconds>(agora - this->tempo_inicial);
      return duracao.count();
}

void Simulador::enviar_mensagem_global(std::string destino_id, Mensagem msg)
{
      if (this->rede.contains(destino_id) && this->rede[destino_id]->is_ativo())
            this->rede[destino_id]->get_inbox()->push_back(msg);
}

void Simulador::registrar_roteador(const std::string& id, std::shared_ptr<Roteador> roteador)
{
      this->rede[id] = roteador;
}
