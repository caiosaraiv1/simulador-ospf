// O #pragma once substitui o clássico Include Guard (#ifndef/#define/#endif) do C.
// Garante que o compilador leia este cabeçalho apenas uma vez por unidade de compilação.
#pragma once

#include <string>
#include <memory>

// Forward Declaration: Resolve a Dependência Circular (o "problema do ovo e da galinha").
// Avisa ao compilador que a classe existe, permitindo o uso do ponteiro logo abaixo
// sem precisar dar um #include no arquivo inteiro e causar um loop infinito.
class FilaMensagens;

/**
 * @brief Representa uma adjacência (um cabo conectado) entre dois roteadores.
 * * Funciona como a aresta direcionada do grafo na topologia da rede.
 */
struct Link
{
      /** @brief Router ID do roteador que está na outra ponta do cabo (ex: "2.2.2.2"). */
      std::string destino_id;

      /** @brief Métrica OSPF da interface, usada como peso no cálculo do caminho mais curto (Dijkstra). */
      int custo;

      /** * @brief A "ponte" de comunicação direta.
       * Ponteiro inteligente para a caixa de entrada exclusiva do vizinho.
       * Permite o envio de pacotes de forma thread-safe sem acessar o resto da memória do outro roteador.
       */
      std::shared_ptr<FilaMensagens> inbox_vizinho;
};
