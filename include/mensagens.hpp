#pragma once

#include "links.hpp"

#include <string>
#include <vector>

// Tipos de pacotes que trafegam na rede simulada.
// TIMEOUT é gerado internamente pelo wait_pop quando não há mensagem no prazo.
// POISON_PILL é injetado pelo desligar_roteador para destravar a thread de trabalho.
enum class TipoMensagem : std::uint8_t
{
      HELLO,
      LSU,
      TIMEOUT,
      POISON_PILL
};

// Máquina de estados da adjacência OSPF
enum class EstadoVizinho : std::uint8_t
{
      DOWN,
      INIT,
      FULL
};

struct Mensagem
{
      TipoMensagem tipo{};
      std::string remetente_id;
      std::vector<Link> payload;              // Links locais do remetente (somente LSU)
      std::vector<std::string> vizinhos_conhecidos; // Vizinhos conhecidos pelo remetente (somente HELLO)
};
