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
      this->lsdb[this->router_id].push_back(novo_link);
}

void Roteador::executar_dijkstra()
{
      std::unordered_map<std::string, int> distancias;
      std::set<std::string> nao_visitados;

      for (auto& roteador : this->lsdb)
      {
            distancias[roteador.first] = std::numeric_limits<int>::max();
            nao_visitados.insert(roteador.first);
      }
      distancias[this->router_id] = 0;
}

std::string Roteador::get_router_id() const { return this->router_id; }

bool Roteador::is_ativo() const { return this->ativo; }

std::shared_ptr<FilaMensagens> Roteador::get_inbox() const { return this->inbox; }

