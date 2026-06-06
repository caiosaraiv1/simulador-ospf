#include "roteador.hpp"
#include <iostream>
#include <string>
#include <limits>
#include <set>

Roteador::Roteador(std::string router_id)
{
      this->router_id = router_id;
      this->ativo = true;
      this->inbox = std::make_shared<FilaMensagens>();
}

void Roteador::adicionar_link(const Link& novo_link)
{
      this->adicionar_link_na_lsdb(this->router_id, novo_link);
}

void Roteador::adicionar_link_na_lsdb(std::string id_origem, const Link& novo_link)
{
    this->lsdb[id_origem].push_back(novo_link);
}

void Roteador::executar_dijkstra()
{
      std::unordered_map<std::string, int> distancias;
      std::unordered_map<std::string, std::string> anteriores;
      std::set<std::string> nao_visitados;

      for (auto& roteador : this->lsdb)
      {
            distancias[roteador.first] = std::numeric_limits<int>::max();
            nao_visitados.insert(roteador.first);
      }
      distancias[this->router_id] = 0;

      while(!nao_visitados.empty())
      {
            // Passo 1: Nó de Menor Custo
            std::string no_atual = "";
            int menor_distancia = std::numeric_limits<int>::max();
            for (auto& roteador : nao_visitados)
            {
                  if (distancias[roteador] < menor_distancia)
                  {
                        menor_distancia = distancias[roteador];
                        no_atual = roteador;
                  }
            }
            // Se o nó mais próximo ainda está no infinito, a rede dividiu e o resto é inalcançável.
            if (menor_distancia == std::numeric_limits<int>::max()) break; // Sai do while imediatamente e encerra o algoritmo

            // Se o no_atual não tiver nenhuma informação de links na nossa LSDB, pula ele
            if (this->lsdb.count(no_atual) == 0)
            {
                  nao_visitados.erase(no_atual); // Tira ele dos não visitados para não travar o loop
                  continue; // Vai para a próxima rodada do while
            }

            std::vector<Link> links = this->lsdb[no_atual];
            int custo_acumulado = 0;
            for (auto& link : links)
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

      this->tabela_roteamento.clear();
      for (auto& par : anteriores)
      {
            std::string destino = par.first;
            std::string atual = destino;

             while (anteriores.count(atual) && anteriores[atual] != this->router_id)
                  atual = anteriores[atual];

            this->tabela_roteamento[destino] = atual;
      }
}

void Roteador::imprimir_tabela_roteamento() const
{
      std::cout << "\n--- TABELA DE ROTEAMENTO DO ROTEADOR [" << this->router_id << "] ---" << std::endl;
      std::cout << "Destino Final\t->\tNext Hop (Próximo Salto)" << std::endl;

      if (this->tabela_roteamento.empty())
      {
            std::cout << "[AVISO] Tabela vazia. Rode o Dijkstra primeiro!" << std::endl;
            return;
      }

      for (auto& par : this->tabela_roteamento)
      {
            std::cout << "    " << par.first << "\t\t->\t    " << par.second << std::endl;
      }
      std::cout << "---------------------------------------------------\n" << std::endl;
}

std::string Roteador::get_router_id() const { return this->router_id; }

bool Roteador::is_ativo() const { return this->ativo; }

std::shared_ptr<FilaMensagens> Roteador::get_inbox() const { return this->inbox; }

FilaMensagens::FilaMensagens() {}
FilaMensagens::~FilaMensagens() {}
Roteador::~Roteador() {}
