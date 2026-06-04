#pragma once

#include "links.hpp"
#include <string>
#include <vector>

/*
 * @brief Define os tipos de pacotes que trafegam na rede simulada.
 * * HELLO: Usado para descobrir vizinhos e manter a adjacência viva.
 * LSU: Link State Update. Carrega a topologia (payload) para inundar a rede.
 * POISON_PILL: Mensagem Out-of-Band para forçar a morte da thread.
 */
enum class TipoMensagem
{
      HELLO, LSU, POISON_PILL
};

/*
 * @brief Representa a unidade fundamental de comunicação entre os roteadores.
 * * @param tipo O propósito do pacote (HELLO, LSU, etc).
 * @param remetente_id O Router ID de quem gerou a mensagem (ex: "1.1.1.1").
 * @param payload Lista de links contendo o mapa local do remetente (usado apenas no tipo LSU).
 */
struct Mensagem
{
      TipoMensagem tipo;
      std::string remetente_id;
      std::vector<Link> payload;
};
