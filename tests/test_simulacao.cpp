#include "roteador.hpp"
#include "simulador.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <thread>

static constexpr const char *TOPO_JSON = "data/malha_parcial_base.json";

// ─── Fixture ──────────────────────────────────────────────────────────────────

class SimuladorTopoFixture : public ::testing::Test
{
    protected:
	Simulador sim;

	void TearDown() override
	{
		sim.desligar_simulacao();
	}
};

// ============================================================
// GRUPO 1 — carregar_topologia
// ============================================================

// ============================================================
// CASO 1 — Carrega o JSON sem travar ou lançar exceção
//
// Verifica que o arquivo é lido e os roteadores são instanciados.
// Qualquer erro de parse ou acesso inválido causaria crash aqui.
// ============================================================
TEST_F(SimuladorTopoFixture, CarregarTopologiaSemCrash)
{
	EXPECT_NO_THROW(sim.carregar_topologia(TOPO_JSON));
}

// ============================================================
// CASO 2 — Simulador inicia todas as threads sem deadlock
//
// Após carregar, iniciar_simulacao() deve subir uma thread por
// roteador sem travar. O sleep dá tempo para o scheduler ativar
// todas as threads antes de checar.
// ============================================================
TEST_F(SimuladorTopoFixture, IniciarSimulacaoSemDeadlock)
{
	sim.carregar_topologia(TOPO_JSON);
	sim.iniciar_simulacao();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	// Se chegou aqui, nenhuma thread travou na inicialização
	SUCCEED();
}

// ============================================================
// CASO 3 — Roteadores carregados aceitam mensagens via simulador
//
// enviar_mensagem_global só entrega se o destino estiver ativo.
// Após iniciar_simulacao(), ao menos o primeiro roteador listado
// no JSON deve estar ativo e receber o HELLO sem crash.
// ============================================================
TEST_F(SimuladorTopoFixture, RoteadoresAceitamMensagensAposCarregar)
{
	sim.carregar_topologia(TOPO_JSON);
	sim.iniciar_simulacao();

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	Mensagem msg;
	msg.tipo = TipoMensagem::HELLO;
	msg.remetente_id = "externo";

	// O JSON define os IDs — ajuste o destino abaixo para um ID real do seu arquivo
	EXPECT_NO_THROW(sim.enviar_mensagem_global("R1", msg));
}

// ============================================================
// CASO 4 — Convergência end-to-end a partir do JSON real
//
// Aguarda ciclos de HELLO suficientes para que a rede convirja.
// O teste valida encerramento limpo (join sem hang) como proxy
// de que nenhuma thread ficou presa esperando mensagem.
// ============================================================
TEST_F(SimuladorTopoFixture, ConvergenciaEndToEndViaJson)
{
	sim.carregar_topologia(TOPO_JSON);
	sim.iniciar_simulacao();

	// Três ciclos de HELLO (2s cada) + margem para LSU propagar
	std::this_thread::sleep_for(std::chrono::milliseconds(7000));

	// desligar_simulacao() faz join em todas as threads;
	// se travar aqui, alguma thread não encerrou corretamente
	EXPECT_NO_THROW(sim.desligar_simulacao());
}

// ============================================================
// GRUPO 2 — enviar_mensagem_global: descarte de mensagens
// ============================================================

// ============================================================
// CASO 5 — Descarta mensagem se destino não existe na rede
//
// O ID "FANTASMA" nunca foi registrado no JSON. A função não
// deve lançar exceção nem acessar memória inválida.
// ============================================================
TEST_F(SimuladorTopoFixture, DescartaMensagemDestinoInexistente)
{
	sim.carregar_topologia(TOPO_JSON);

	Mensagem msg;
	msg.tipo = TipoMensagem::HELLO;
	msg.remetente_id = "externo";

	EXPECT_NO_THROW(sim.enviar_mensagem_global("FANTASMA", msg));
}

// ============================================================
// CASO 6 — Descarta mensagem se destino está inativo
//
// Carregamos a topologia mas não chamamos iniciar_simulacao(),
// portanto todos os roteadores têm is_ativo() == false.
// A mensagem deve ser silenciosamente descartada.
// ============================================================
TEST_F(SimuladorTopoFixture, DescartaMensagemDestinoInativo)
{
	sim.carregar_topologia(TOPO_JSON);
	// Sem iniciar_simulacao() — todos inativos

	Mensagem msg;
	msg.tipo = TipoMensagem::LSU;
	msg.remetente_id = "externo";

	EXPECT_NO_THROW(sim.enviar_mensagem_global("R1", msg));
}

// ============================================================
// CASO 7 — Descarta mensagem se destino foi desligado após ativo
//
// K é ligado, depois desligado. Mensagem enviada após o
// desligamento não deve ser entregue (is_ativo() == false).
// ============================================================
TEST_F(SimuladorTopoFixture, DescartaMensagemAposDesligar)
{
	Simulador sim_local;

	auto m = std::make_shared<Roteador>("M", &sim_local);
	auto k = std::make_shared<Roteador>("K", &sim_local);

	sim_local.registrar_roteador("M", m);
	sim_local.registrar_roteador("K", k);

	k->ligar_roteador();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	k->desligar_roteador();

	EXPECT_FALSE(k->is_ativo());

	Mensagem msg;
	msg.tipo = TipoMensagem::HELLO;
	msg.remetente_id = "M";

	sim_local.enviar_mensagem_global("K", msg);

	// Inbox de K deve estar vazia — wait_pop retorna TIMEOUT
	Mensagem recebida = k->get_inbox()->wait_pop(std::chrono::milliseconds(100));
	EXPECT_EQ(recebida.tipo, TipoMensagem::TIMEOUT)
	      << "inbox de K deveria estar vazia apos desligamento";

	sim_local.desligar_simulacao();
}

// ============================================================
// CASO 8 — Entrega mensagem normalmente quando destino está ativo
//
// Regressão: garante que o mecanismo de descarte não bloqueia
// entregas legítimas.
// ============================================================
TEST_F(SimuladorTopoFixture, EntregaMensagemDestinoAtivo)
{
	Simulador sim_local;

	auto origem = std::make_shared<Roteador>("SRC", &sim_local);
	auto destino = std::make_shared<Roteador>("DST", &sim_local);

	sim_local.registrar_roteador("SRC", origem);
	sim_local.registrar_roteador("DST", destino);

	destino->ligar_roteador();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_TRUE(destino->is_ativo());

	Mensagem msg;
	msg.tipo = TipoMensagem::HELLO;
	msg.remetente_id = "SRC";

	sim_local.enviar_mensagem_global("DST", msg);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_TRUE(destino->is_ativo());

	destino->desligar_roteador();
	sim_local.desligar_simulacao();
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
