#pragma once

#include "roteador.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

// Roteador derrubado pela rotina de caos; elegível para ressureição após 30s
struct RoteadorMorto
{
      std::string id;
      std::chrono::steady_clock::time_point tempo_morte;
};

// Orquestra o ecossistema da rede: carrega a topologia, liga os roteadores e injeta falhas
class Simulador
{
    private:
      // Chave: Router ID — Valor: ponteiro para o objeto Roteador em memória
      std::unordered_map<std::string, std::shared_ptr<Roteador>> rede;

      std::chrono::steady_clock::time_point tempo_inicial;

      std::atomic<bool> caos_rodando{true};
      std::thread thread_caos;
      std::vector<std::string> vetor_ativos;
      std::vector<RoteadorMorto> vetor_mortos;

    public:
      Simulador() = default;
      ~Simulador() = default;
      Simulador(const Simulador &) = delete;
      Simulador &operator=(const Simulador &) = delete;
      Simulador(Simulador &&) = delete;
      Simulador &operator=(Simulador &&) = delete;

      // Configuração
      void carregar_topologia(const std::string &caminho_json);
      void registrar_roteador(const std::string &id, const std::shared_ptr<Roteador> &roteador);

      // Controle da simulação
      void iniciar_simulacao();
      void desligar_simulacao();

      // Injeção de caos
      void rotina_caos();

      // Utilitários
      int get_tempo_simulacao() const;
      void enviar_mensagem_global(const std::string &destino_id, const Mensagem &msg);
};
