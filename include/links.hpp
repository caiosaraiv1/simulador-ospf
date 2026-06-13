#pragma once

#include <memory>
#include <string>

// Forward declaration para resolver a dependência circular com FilaMensagens
class FilaMensagens;

// Aresta direcionada do grafo de topologia; representa um cabo entre dois roteadores
struct Link
{
      std::string destino_id;

      // Peso da aresta no Dijkstra
      int custo = 0;

      // Acesso direto à inbox do vizinho para envio thread-safe de pacotes
      std::shared_ptr<FilaMensagens> inbox_vizinho;
};

inline bool operator==(const Link &a, const Link &b)
{
      return a.destino_id == b.destino_id && a.custo == b.custo;
}
