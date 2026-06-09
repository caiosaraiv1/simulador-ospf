#include "simulador.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

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
		auto novo_roteador = std::make_shared<Roteador>(id);

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

void Simulador::enviar_mensagem_global(std::string destino_id, Mensagem msg)
{
      if (this->rede[destino_id]->is_ativo() && this->rede.contains(destino_id))
            this->rede[destino_id]->get_inbox()->push_back(msg);
}
